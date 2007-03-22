#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  if (argc != 2)
    return 1;

  uid_t realuser = getuid();
  char uidstring[20];
  snprintf(uidstring, 19, "%i", realuser);
  uidstring[19] = '\0';

  return execl("/usr/sbin/chown",
               "/usr/sbin/chown", "-R", "-h", uidstring, argv[1], (char*) 0);
}
