#include "jstypes.h"

struct Foo
{
  JS_REQUIRES_STACK void *redmember;
};

void* GreenFunc(Foo *f)
{
  return f->redmember;
}
