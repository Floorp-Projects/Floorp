/* teststream.c: Test driver to generate escaped stream data */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  char* cookie;

  cookie = getenv("LTERM_COOKIE");  /* Get security cookie */
  if (cookie == NULL)
    cookie = "";

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <data-string> [<terminator-string>]\n",argv[0]);
    return 1;
  }

  if (argc > 2) {
    /* NOT IMPLEMENTED YET */
    fprintf(stdout, "\033{T%s\n%s%s", argv[2], argv[1], argv[2]);

  } else {
    fprintf(stdout, "\033{S%s\n%s%c", cookie, argv[1], '\0');
  }

  return 0;
}
