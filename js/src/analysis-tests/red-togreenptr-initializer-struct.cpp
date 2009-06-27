#include "jstypes.h"

JS_REQUIRES_STACK void RedFunc();

typedef void (*GreenFuncPtr)();

struct GreenStruct
{
  GreenFuncPtr func;
};

GreenStruct gs = { RedFunc };
