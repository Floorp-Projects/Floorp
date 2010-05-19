#include "NativeJSContext.h"
#include "nsServiceManagerUtils.h"
#include "nsIJSRuntimeService.h"

nsIJSRuntimeService* NativeJSContext::sJSRuntimeService = 0;
JSRuntime* NativeJSContext::sJSScriptRuntime = 0;

PRBool
NativeJSContext::AddGCRoot(void *aPtr, const char *aName)
{
  NS_ASSERTION(NS_SUCCEEDED(error), "class failed to initialize and caller used class without checking!");
  if (!sJSScriptRuntime) {
    nsresult rv = CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                                 &sJSRuntimeService);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    NS_ABORT_IF_FALSE(sJSRuntimeService, "CallGetService succeeded but returned a null pointer?");

    sJSRuntimeService->GetRuntime(&sJSScriptRuntime);
    if (!sJSScriptRuntime) {
      NS_RELEASE(sJSRuntimeService);
      NS_WARNING("Unable to get JS runtime from JS runtime service");
      return PR_FALSE;
    }
  }

  PRBool ok;
  return ok = ::JS_AddNamedRootRT(sJSScriptRuntime, aPtr, aName);
}

void
NativeJSContext::ReleaseGCRoot(void *aPtr)
{
  NS_ASSERTION(NS_SUCCEEDED(error), "class failed to initialize and caller used class without checking!");
  if (!sJSScriptRuntime) {
    NS_NOTREACHED("Trying to remove a JS GC root when none were added");
    return;
  }

  ::JS_RemoveRootRT(sJSScriptRuntime, aPtr);
}
