/* escape.c: Generates XMLterm/LineTerm escape sequences */

#include <stdio.h>

int main(int argc, char *argv[])
{
  int nparam, j;
  char *param1, *param2, *param3, *code, *sparam;

  param1 = NULL;
  param2 = NULL;
  param3 = NULL;
  code =   NULL;
  sparam = "";
  nparam = 0;

  for (j=1; j<argc; j++) {
    if ((*argv[j] >= '0') && (*argv[j] <= '9')) {
      if (j == 1) {
        param1 = argv[j];
        nparam = 1;
      } else if (j == 2) {
        param2 = argv[j];
        nparam = 2;
      } else if (j == 3) {
        param3 = argv[j];
        nparam = 3;
      }
    } else if (code == NULL) {
      code = argv[j];
    } else {
      sparam = argv[j];
    }
  }

  if (code == NULL) {
    fprintf(stderr, "Usage: %s [<param1> [<param2> [<param3>]]] <code-character> [<string-param>] \n", argv[0]);
    return 1;
  }

  if (nparam == 0) {
    fprintf(stderr, "ESC{%s%s\\n\n", code, sparam);
    fprintf(stdout, "\033{%s%s\n", code, sparam);
  } else if (nparam == 1) {
    fprintf(stderr, "ESC{%s%s%s\\n\n", param1, code, sparam);
    fprintf(stdout, "\033{%s%s%s\n", param1, code, sparam);
  } else if (nparam == 2) {
    fprintf(stderr, "ESC{%s;%s%s%s\\n\n", param1, param2, code, sparam);
    fprintf(stdout, "\033{%s;%s%s%s\n", param1, param2, code, sparam);
  } else {
    fprintf(stderr, "ESC{%s;%s;%s%s%s\\n\n", param1, param2, param3, code, sparam);
    fprintf(stdout, "\033{%s;%s;%s%s%s\n", param1, param2, param3, code, sparam);
  }

  return 0;
}
