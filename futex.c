#define _GNU_SOURCE
#include <sys/ucontext.h>
#include <seccomp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <sys/auxv.h>
#include <sys/mman.h>

typedef struct { 
        int counter;
} atomic_t;

#define ATOMIC_INIT(i)  { (i) }
static int
futex(int *uaddr, int futex_op, int val,
     const struct timespec *timeout, int *uaddr2, int val3)
{
   return syscall(SYS_futex, uaddr, futex_op, val,
                  timeout, uaddr, val3);
}

static inline int atomic_read(const atomic_t *v)
{
        return (*(volatile int *)&(v)->counter);
}

static inline void atomic_set(atomic_t *v, int i)
{
        *((volatile int *)&v->counter) = i;
}

int main(int argc, char **argv)
{
  pid_t pid;
  int i, n = atoi(argv[1]);
  atomic_t *switchval;

  switchval = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  if (switchval == MAP_FAILED)
    return 1;

  pid = fork();
  if (pid < 0)
    return 1;
  if (pid == 0) {

    while (1) {
      while (1) {
        int v = atomic_read(switchval);
        if (v != 0)
          break;
        futex(&switchval->counter, FUTEX_WAIT, v, NULL, NULL, 0);
      }
      syscall(__NR_clock_gettime, 1 << 31, NULL);
      atomic_set(switchval, 0);
      futex(&switchval->counter, FUTEX_WAKE, 1, NULL, NULL, 0);
    }

    return 0;
  }

  for (i = 0; i < n; i++) {
      while (1) {
        int v = atomic_read(switchval);
        if (v != 1)
          break;
        futex(&switchval->counter, FUTEX_WAIT, v, NULL, NULL, 0);
      }
      syscall(__NR_clock_gettime, 1 << 31, NULL);
      atomic_set(switchval, 1);
      futex(&switchval->counter, FUTEX_WAKE, 1, NULL, NULL, 0);
  }
  kill(pid, SIGKILL);


  return 0;
}
