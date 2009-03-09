#include "jstypes.h"

JS_REQUIRES_STACK void RedFunc();

JS_FORCES_STACK void TurnRedFunc();

void GreenPartlyRedFunc(int i)
{
  if (i) {
    TurnRedFunc();
  }

  RedFunc();
}
