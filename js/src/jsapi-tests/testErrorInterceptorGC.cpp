#include "jsapi.h"

#include "js/ErrorInterceptor.h"
#include "jsapi-tests/tests.h"

namespace {

// An interceptor that triggers GC:
struct ErrorInterceptorWithGC : JSErrorInterceptor {
  void interceptError(JSContext* cx, JS::HandleValue val) override {
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::DEBUG_GC);
  }
};

}  // namespace

BEGIN_TEST(testErrorInterceptorGC) {
  JSErrorInterceptor* original = JS_GetErrorInterceptorCallback(cx->runtime());

  ErrorInterceptorWithGC interceptor;
  JS_SetErrorInterceptorCallback(cx->runtime(), &interceptor);

  CHECK(!execDontReport("0 = 0;", __FILE__, __LINE__));

  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);

  // Restore the original error interceptor.
  JS_SetErrorInterceptorCallback(cx->runtime(), original);

  return true;
}
END_TEST(testErrorInterceptorGC)
