#include "jstypes.h"

JS_REQUIRES_STACK void RedFunc();

JS_FORCES_STACK void TurnRedFunc();

void GreenToRedFunc()
{
  TurnRedFunc();

  RedFunc();
}
