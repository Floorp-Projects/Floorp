#include "jstypes.h"

typedef void (*GreenFuncPtr)();
typedef void (JS_REQUIRES_STACK *RedFuncPtr)();

struct Foo
{
  int i;
  GreenFuncPtr p;
};

void Test(Foo *foo, RedFuncPtr p)
{
  foo->p = p;
}
