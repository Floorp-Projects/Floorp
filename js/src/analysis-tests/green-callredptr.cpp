#include "jstypes.h"

typedef void (JS_REQUIRES_STACK *RedFuncPtr)();

void GreenFunc(RedFuncPtr f)
{
  f();
}
