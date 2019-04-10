#include "jsapi.h"

#include "jsapi-tests/tests.h"

#include "util/StringBuffer.h"

// Tests for JS_GetErrorInterceptorCallback and JS_SetErrorInterceptorCallback.

namespace {
static JS::PersistentRootedString gLatestMessage;

// An interceptor that stores the error in `gLatestMessage`.
struct SimpleInterceptor : JSErrorInterceptor {
  virtual void interceptError(JSContext* cx, JS::HandleValue val) override {
    js::JSStringBuilder buffer(cx);
    if (!ValueToStringBuffer(cx, val, buffer)) {
      MOZ_CRASH("Could not convert to string buffer");
    }
    gLatestMessage = buffer.finishString();
    if (!gLatestMessage) {
      MOZ_CRASH("Could not convert to string");
    }
  }
};

bool equalStrings(JSContext* cx, JSString* a, JSString* b) {
  int32_t result = 0;
  if (!JS_CompareStrings(cx, a, b, &result)) {
    MOZ_CRASH("Could not compare strings");
  }
  return result == 0;
}
}  // namespace

BEGIN_TEST(testErrorInterceptor) {
  // Run the following snippets.
  const char* SAMPLES[] = {
      "throw new Error('I am an Error')\0",
      "throw new TypeError('I am a TypeError')\0",
      "throw new ReferenceError('I am a ReferenceError')\0",
      "throw new SyntaxError('I am a SyntaxError')\0",
      "throw 5\0",
      "undefined[0]\0",
      "foo[0]\0",
      "b[\0",
  };
  // With the simpleInterceptor, we should end up with the following error:
  const char* TO_STRING[] = {
      "Error: I am an Error\0",
      "TypeError: I am a TypeError\0",
      "ReferenceError: I am a ReferenceError\0",
      "SyntaxError: I am a SyntaxError\0",
      "5\0",
      "TypeError: undefined has no properties\0",
      "ReferenceError: foo is not defined\0",
      "SyntaxError: expected expression, got end of script\0",
  };
  MOZ_ASSERT(mozilla::ArrayLength(SAMPLES) == mozilla::ArrayLength(TO_STRING));

  // Save original callback.
  JSErrorInterceptor* original = JS_GetErrorInterceptorCallback(cx->runtime());
  gLatestMessage.init(cx);

  // Test without callback.
  JS_SetErrorInterceptorCallback(cx->runtime(), nullptr);
  CHECK(gLatestMessage == nullptr);

  for (auto sample : SAMPLES) {
    if (execDontReport(sample, __FILE__, __LINE__)) {
      MOZ_CRASH("This sample should have failed");
    }
    CHECK(JS_IsExceptionPending(cx));
    CHECK(gLatestMessage == nullptr);
    JS_ClearPendingException(cx);
  }

  // Test with callback.
  SimpleInterceptor simpleInterceptor;
  JS_SetErrorInterceptorCallback(cx->runtime(), &simpleInterceptor);

  // Test that we return the right callback.
  CHECK_EQUAL(JS_GetErrorInterceptorCallback(cx->runtime()),
              &simpleInterceptor);

  // This shouldn't cause any error.
  EXEC("function bar() {}");
  CHECK(gLatestMessage == nullptr);

  // Test error throwing with a callback that succeeds.
  for (size_t i = 0; i < mozilla::ArrayLength(SAMPLES); ++i) {
    // This should cause the appropriate error.
    if (execDontReport(SAMPLES[i], __FILE__, __LINE__)) {
      MOZ_CRASH("This sample should have failed");
    }
    CHECK(JS_IsExceptionPending(cx));

    // Check result of callback.
    CHECK(gLatestMessage != nullptr);
    CHECK(js::StringEqualsAscii(&gLatestMessage->asLinear(), TO_STRING[i]));

    // Check the final error.
    JS::RootedValue exn(cx);
    CHECK(JS_GetPendingException(cx, &exn));
    JS_ClearPendingException(cx);

    js::JSStringBuilder buffer(cx);
    CHECK(ValueToStringBuffer(cx, exn, buffer));
    JS::Rooted<JSFlatString*> flat(cx, buffer.finishString());
    CHECK(equalStrings(cx, flat, gLatestMessage));

    // Cleanup.
    gLatestMessage = nullptr;
  }

  // Test again without callback.
  JS_SetErrorInterceptorCallback(cx->runtime(), nullptr);
  for (size_t i = 0; i < mozilla::ArrayLength(SAMPLES); ++i) {
    if (execDontReport(SAMPLES[i], __FILE__, __LINE__)) {
      MOZ_CRASH("This sample should have failed");
    }
    CHECK(JS_IsExceptionPending(cx));

    // Check that the callback wasn't called.
    CHECK(gLatestMessage == nullptr);

    // Check the final error.
    JS::RootedValue exn(cx);
    CHECK(JS_GetPendingException(cx, &exn));
    JS_ClearPendingException(cx);

    js::JSStringBuilder buffer(cx);
    CHECK(ValueToStringBuffer(cx, exn, buffer));
    JS::Rooted<JSFlatString*> flat(cx, buffer.finishString());
    CHECK(js::StringEqualsAscii(flat, TO_STRING[i]));

    // Cleanup.
    gLatestMessage = nullptr;
  }

  // Cleanup
  JS_SetErrorInterceptorCallback(cx->runtime(), original);
  gLatestMessage = nullptr;
  JS_ClearPendingException(cx);

  return true;
}
END_TEST(testErrorInterceptor)
