#include "jstypes.h"

JS_REQUIRES_STACK void RedFunc1();

JS_REQUIRES_STACK void RedFunc2()
{
  RedFunc1();
}
