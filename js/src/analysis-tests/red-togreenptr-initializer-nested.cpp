#include "jstypes.h"

void GreenFunc();
JS_REQUIRES_STACK void RedFunc();

typedef void (*GreenFuncPtr)();

struct S
{
  int i;
  GreenFuncPtr p;
};

S sa[] = {
  { 2, GreenFunc },
  { 1, RedFunc },
};

