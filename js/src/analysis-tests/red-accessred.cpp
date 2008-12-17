#include "jstypes.h"

struct Foo
{
  JS_REQUIRES_STACK void *redmember;
};

JS_REQUIRES_STACK void * RedFunc(Foo *f)
{
  return f->redmember;
}
