#include "gdb-tests.h"

typedef int my_typedef;

FRAGMENT(typedef_printers, one) {
  my_typedef i = 0;

  breakpoint();

  (void) i;
}
