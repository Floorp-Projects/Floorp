/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testOOM)
{
    JS::RootedValue v(cx, JS::Int32Value(9));
    JS::RootedString jsstr(cx, JS::ToString(cx, v));
    char16_t ch;
    if (!JS_GetStringCharAt(cx, jsstr, 0, &ch))
        return false;
    MOZ_RELEASE_ASSERT(ch == '9');
    return true;
}

virtual JSRuntime * createRuntime() override
{
    JSRuntime* rt = JS_NewRuntime(0);
    if (!rt)
        return nullptr;
    JS_SetGCParameter(rt, JSGC_MAX_BYTES, (uint32_t)-1);
    setNativeStackQuota(rt);
    return rt;
}
END_TEST(testOOM)

#ifdef DEBUG  // js::oom functions are only available in debug builds.

const uint32_t maxAllocsPerTest = 100;

#define START_OOM_TEST(name)                                                  \
    testName = name;                                                          \
    printf("Test %s: started\n", testName);                                   \
    for (oomAfter = 1; oomAfter < maxAllocsPerTest; ++oomAfter) {             \
    js::oom::SimulateOOMAfter(oomAfter, js::oom::THREAD_TYPE_MAIN, true)

#define OOM_TEST_FINISHED                                                     \
    {                                                                         \
        printf("Test %s: finished with %" PRIu64 " allocations\n",            \
               testName, oomAfter - 1);                                       \
        break;                                                                \
    }

#define END_OOM_TEST                                                          \
    }                                                                         \
    js::oom::ResetSimulatedOOM();                                             \
    CHECK(oomAfter != maxAllocsPerTest)

BEGIN_TEST(testNewRuntime)
{
    uninit(); // Get rid of test harness' original JSRuntime.

    JSRuntime* rt;
    START_OOM_TEST("new runtime");
    rt = JS_NewRuntime(8L * 1024 * 1024);
    if (rt)
        OOM_TEST_FINISHED;
    END_OOM_TEST;
    JS_DestroyRuntime(rt);
    return true;
}

const char* testName;
uint64_t oomAfter;
END_TEST(testNewRuntime)

#endif
