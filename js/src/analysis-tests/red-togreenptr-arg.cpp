#include "jstypes.h"

typedef void (*GreenFuncPtr)();
typedef void (JS_REQUIRES_STACK *RedFuncPtr)();

void TakesAsArgument(GreenFuncPtr g);

void Test(RedFuncPtr p)
{
  TakesAsArgument(p);
}
