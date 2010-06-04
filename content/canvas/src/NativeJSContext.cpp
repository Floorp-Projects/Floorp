#include "NativeJSContext.h"
#include "nsServiceManagerUtils.h"
#include "nsIJSRuntimeService.h"

PRBool
NativeJSContext::AddGCRoot(void *aPtr, const char *aName)
{
  NS_ASSERTION(NS_SUCCEEDED(error), "class failed to initialize and caller used class without checking!");

  PRBool ok;
  return ok = ::JS_AddNamedObjectRoot(ctx, aPtr, aName);
}

void
NativeJSContext::ReleaseGCRoot(JSObject **aPtr)
{
  NS_ASSERTION(NS_SUCCEEDED(error), "class failed to initialize and caller used class without checking!");
  ::JS_RemoveObjectRoot(ctx, aPtr);
}
