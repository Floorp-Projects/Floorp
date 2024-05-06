#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
int main()
{
    char *shmaddr;
    int id = shmget (IPC_PRIVATE, 4, IPC_CREAT | 0600);
    if (id == -1) return 2;
    shmaddr = shmat (id, 0, 0);
    shmctl (id, IPC_RMID, 0);
    if ((char*) shmat (id, 0, 0) == (char*) -1) {
  shmdt (shmaddr);
  return 1;
    }
    shmdt (shmaddr);
    shmdt (shmaddr);
    return 0;
}
