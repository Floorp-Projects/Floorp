#include <unistd.h>

int main(int argc, char **argv)
{
  if (argc != 2)
    return 1;

  return execl("/usr/sbin/chown",
               "/usr/sbin/chown", "-R", "-h", "root:admin", argv[1], (char*) 0);
}
