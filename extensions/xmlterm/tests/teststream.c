/* teststream.c: Test driver to generate escaped stream data */

#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <data-string> [<terminator-string>]\n",argv[0]);
    return 1;
  }

  if (argc > 2) {
    /* fprintf(stderr, "{ESCS%sBEL%s%s", argv[2], argv[1], argv[2]); */
    fprintf(stdout, "\033{S%s\007%s%s", argv[2], argv[1], argv[2]);

  } else {
    fprintf(stdout, "\033{S\007%s%c", argv[1], '\0');
  }

  return 0;
}
