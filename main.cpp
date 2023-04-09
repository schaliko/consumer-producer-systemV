#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <iostream>

#define MAX_COUNT 100

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main(){
    struct sembuf semops;
    union semun semarg;
    key_t key;

    // create a unique key for the semaphore set
    if ((key = ftok(".", 'S')) == -1) {
        perror("ftok");
    }

    // Create a shared memory segment
    int shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // Attach the shared memory segment
    char* shared_memory = (char*) shmat(shmid, NULL, 0);
    if (shared_memory == (void*) -1) {
        perror("shmat");
        exit(1);
    }

    // Create semaphores for synchronization
    int semid = semget(key, 3, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    // initialize the semaphore value to 0 (full)
    semarg.val = 0;
    if (semctl(semid, 1, SETVAL, semarg) == -1) {
        perror("semctl(full)");
        exit(1);
    }

    // mutex
    semarg.val = 1;
    if (semctl(semid, 2, SETVAL, semarg) == -1) {
        perror("semctl(mutex)");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    // producer process
    if (pid == 0) {

        // wait for empty 
        semops.sem_num = 0; // empty
        semops.sem_op = -1; 
        semops.sem_flg = SEM_UNDO;
        if (semop(semid, &semops, 1) == -1) {
             perror("semop(empty)");
            exit(1);
        }

        // acquire the mutex
        semops.sem_num = 2; // mutex
        semops.sem_op = -1; 
        semops.sem_flg = SEM_UNDO;
        if (semop(semid, &semops, 1) == -1) {
            perror("semop(mutex)");
            exit(1);
        }
    }

     // consumer process
    else {

        for (int i = 0; i < MAX_COUNT; i++) {

            // acquire the mutex
            semops.sem_num = 2; // mutex
            semops.sem_op = -1; // decrement
            semops.sem_flg = SEM_UNDO;
            if (semop(semid, &semops, 1) == -1) {
                perror("semop(mutex)");
                exit(1);
            }

            // from the buffer
            int n = *shared_memory;
            std::cout<< "Consumer: " << n << std::endl;;

            // release the mutex
            semops.sem_num = 2; // mutex
            semops.sem_op = 1; 
            semops.sem_flg = SEM_UNDO;
            if (semop(semid, &semops, 1) == -1) {
                perror("semop(mutex)");
                exit(1);
            }

            // signal that empty is available
            semops.sem_num = 0; // empty
            semops.sem_op = 1; 
            semops.sem_flg = SEM_UNDO;
            if (semop(semid, &semops, 1) == -1) {
                perror("semop(empty)");
                exit(1);
            }

            usleep(100000);
        }

        // wait for the child process
        wait(NULL);

        // Detach the shared memory segment
        if (shmdt(shared_memory) == -1) {
            perror("shmdt");
            exit(1);
        }

        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            exit(1);
        }

        if (semctl(semid, 0, IPC_RMID, NULL) == -1) {
            perror("semctl");
            exit(1);
        }

        std::cout << "End" << std::endl;

    }

    return 0;

}