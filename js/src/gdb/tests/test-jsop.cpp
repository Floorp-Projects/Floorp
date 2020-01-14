#include "gdb-tests.h"

#include "vm/BytecodeUtil.h"

FRAGMENT(jsop, simple) {
  JSOp undefined = JSOP_UNDEFINED;
  JSOp debugger = JSOP_DEBUGGER;

  breakpoint();

  use(undefined);
  use(debugger);
}
