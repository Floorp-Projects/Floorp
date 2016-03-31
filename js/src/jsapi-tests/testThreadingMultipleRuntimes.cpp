#include "jsapi-tests/tests.h"

static bool MultipleRuntimesTestBody(JSContext* cx);

void
MultipleRuntimesTest(void* p)
{
    bool* pb = reinterpret_cast<bool*>(p);

    *pb = false;
    JSRuntime* rt = JS_NewRuntime(8L * 1024 * 1024);
    if (!rt) {
        fprintf(stderr, "JS_NewRuntime failed\n");
        return;
    }

    JSContext* cx = JS_NewContext(rt, 8192);
    JS_BeginRequest(cx);

    bool ok = MultipleRuntimesTestBody(cx);

    JS_EndRequest(cx);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    *pb = ok;
}

bool
MultipleRuntimesTestBody(JSContext* cx)
{
    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    RootedObject global(cx,
                        JS_NewGlobalObject(cx, JSAPITest::basicGlobalClass(), nullptr,
                                           JS::FireOnNewGlobalHook, options));
    if (!global) {
        fprintf(stderr, "JS_NewGlobalObject failed\n");
        return false;
    }

    JSAutoCompartment ac(cx, global);

    if (!JS_InitStandardClasses(cx, global)) {
        fprintf(stderr, "JS_InitStandardClasses failed\n");
        return false;
    }

    JS::CompileOptions opts(cx);
    opts.setFileAndLine(__FILE__, __LINE__ + 3);
    const char* bytes = "for (var sum = 0, i = 0; i < 10000; i++) sum += i;";
    RootedValue rval(cx);
    if (!JS::Evaluate(cx, opts, bytes, strlen(bytes), &rval)) {
        fprintf(stderr, "JS::Evaluate failed\n");
        return false;
    }

    if (!rval.isNumber() || rval.toNumber() != 9999 * 10000 / 2) {
        fprintf(stderr, "JS computed wrong result\n");
        return false;
    }
    return true;
}

BEGIN_TEST(testThreadingMultipleRuntimes)
{
    const static size_t NumThreads = 64;
    js::ExclusiveData<uint64_t> counter(0);

    js::Vector<PRThread*> threads(cx);
    CHECK(threads.reserve(NumThreads));

    js::Vector<bool> results(cx);
    CHECK(results.reserve(NumThreads));

    for (auto i : mozilla::MakeRange(NumThreads)) {
        results.infallibleAppend(false);
        auto thread = PR_CreateThread(PR_USER_THREAD,
                                      MultipleRuntimesTest,
                                      (void *) &results[i],
                                      PR_PRIORITY_NORMAL,
                                      PR_LOCAL_THREAD,
                                      PR_JOINABLE_THREAD,
                                      0);
        CHECK(thread);
        threads.infallibleAppend(thread);
    }

    for (auto thread : threads) {
        CHECK(PR_JoinThread(thread) == PR_SUCCESS);
    }

    for (auto result : results) {
        CHECK(result);
    }

    return true;
}
END_TEST(testThreadingMultipleRuntimes)
