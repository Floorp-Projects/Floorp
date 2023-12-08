/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsapi-tests/tests.h"

#include "vm/HelperThreads.h"

BEGIN_TEST(testOOM) {
  JS::RootedValue v(cx, JS::Int32Value(9));
  JS::RootedString jsstr(cx, JS::ToString(cx, v));
  char16_t ch;
  if (!JS_GetStringCharAt(cx, jsstr, 0, &ch)) {
    return false;
  }
  MOZ_RELEASE_ASSERT(ch == '9');
  return true;
}

virtual JSContext* createContext() override {
  JSContext* cx = JS_NewContext(0);
  if (!cx) {
    return nullptr;
  }
  JS_SetGCParameter(cx, JSGC_MAX_BYTES, (uint32_t)-1);
  return cx;
}
END_TEST(testOOM)

#ifdef DEBUG  // js::oom functions are only available in debug builds.

const uint32_t maxAllocsPerTest = 100;

#  define START_OOM_TEST(name)                                      \
    testName = name;                                                \
    for (bool always : {false, true}) {                             \
      const char* subTest = always ? "fail always" : "fail once";   \
      printf("Test %s (%s): started: ", testName, subTest);         \
      for (oomAfter = 1; oomAfter < maxAllocsPerTest; ++oomAfter) { \
        js::oom::simulator.simulateFailureAfter(                    \
            js::oom::FailureSimulator::Kind::OOM, oomAfter,         \
            js::THREAD_TYPE_MAIN, always)

#  define END_OOM_TEST                                                  \
    if (!js::oom::HadSimulatedOOM()) {                                  \
      printf("\nTest %s (%s): finished with %" PRIu64 " allocations\n", \
             testName, subTest, oomAfter - 1);                          \
      break;                                                            \
    }                                                                   \
    }                                                                   \
    }                                                                   \
    js::oom::simulator.reset();                                         \
    CHECK(oomAfter != maxAllocsPerTest)

#  define MARK_STAR printf("*");
#  define MARK_PLUS printf("+");
#  define MARK_DOT printf(".");

BEGIN_TEST(testNewContextOOM) {
  uninit();  // Get rid of test harness' original JSContext.

  const char* testName;
  uint64_t oomAfter;
  JSContext* cx;
  START_OOM_TEST("new context");
  cx = JS_NewContext(8L * 1024 * 1024);
  if (cx) {
    MARK_PLUS;
    JS_DestroyContext(cx);
  } else {
    MARK_DOT;
    CHECK(!JSRuntime::hasLiveRuntimes());
  }
  END_OOM_TEST;
  return true;
}
END_TEST(testNewContextOOM)

BEGIN_TEST(testHelperThreadOOM) {
  const char* testName;
  uint64_t oomAfter;
  START_OOM_TEST("helper thread state");

  if (js::CreateHelperThreadsState()) {
    if (js::EnsureHelperThreadsInitialized()) {
      MARK_STAR;
    } else {
      MARK_PLUS;
    }
  } else {
    MARK_DOT;
  }

  // Reset the helper threads to ensure they get re-initalised in following
  // iterations
  js::DestroyHelperThreadsState();

  END_OOM_TEST;

  return true;
}

bool init() override {
  JSAPIRuntimeTest::uninit();       // Discard the just-created JSContext.
  js::DestroyHelperThreadsState();  // The test creates this state.
  return true;
}
void uninit() override {
  // Leave things initialized after this test finishes.
  js::CreateHelperThreadsState();
}

END_TEST(testHelperThreadOOM)

#endif
