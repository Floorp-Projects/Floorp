#include "gdb-tests.h"

#include "vm/BytecodeUtil.h"

FRAGMENT(jsop, simple) {
  JSOp undefined = JSOP_UNDEFINED;
  JSOp debugger = JSOP_DEBUGGER;
  JSOp limit = JSOP_LIMIT;

  breakpoint();

  use(undefined);
  use(debugger);
  use(limit);
}
