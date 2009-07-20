#include "jstypes.h"

typedef void (*GreenFuncPtr)();
typedef void (JS_REQUIRES_STACK *RedFuncPtr)();

GreenFuncPtr Test(RedFuncPtr p)
{
  return p;
}
