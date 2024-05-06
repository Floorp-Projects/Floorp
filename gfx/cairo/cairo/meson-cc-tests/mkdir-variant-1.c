#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif

int main(int ac, char **av)
{
  mkdir("hello.world");
  return 0;
}
