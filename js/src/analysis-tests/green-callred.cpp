#include "jstypes.h"

#ifndef NS_STATIC_CHECKING
#error Running this without NS_STATIC_CHECKING is silly
#endif

JS_REQUIRES_STACK void RedFunc();

void GreenFunc()
{
  RedFunc();
}
