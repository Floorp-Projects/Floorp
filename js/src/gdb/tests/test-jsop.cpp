#include "gdb-tests.h"

#include "vm/BytecodeUtil.h"

FRAGMENT(jsop, simple) {
  JSOp undefined = JSOp::Undefined;
  JSOp debugger = JSOp::Debugger;

  breakpoint();

  use(undefined);
  use(debugger);
}
