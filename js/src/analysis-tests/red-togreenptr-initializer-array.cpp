#include "jstypes.h"

void GreenFunc();
JS_REQUIRES_STACK void RedFunc();

typedef void (*GreenFuncPtr)();

GreenFuncPtr fpa[2] = {GreenFunc, RedFunc};
