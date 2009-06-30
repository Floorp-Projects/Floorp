#include "jstypes.h"

void GreenFunc();

typedef void (JS_REQUIRES_STACK *RedFuncPtr)();

RedFuncPtr Test()
{
  // assigning a green function to a red function pointer is ok
  RedFuncPtr p = GreenFunc;
  return p;
}
