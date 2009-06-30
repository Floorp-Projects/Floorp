#include "jstypes.h"

JS_REQUIRES_STACK void RedFunc();

typedef void (*GreenFuncPtr)();

GreenFuncPtr funcp = RedFunc;
