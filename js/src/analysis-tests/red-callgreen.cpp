#include "jstypes.h"

void GreenFunc();

void JS_REQUIRES_STACK RedFunc()
{
  GreenFunc();
}
