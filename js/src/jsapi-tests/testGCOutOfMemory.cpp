/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCOutOfMemory)
{
    JS::RootedValue root(cx);

    // Count the number of allocations until we hit OOM, and store it in 'max'.
    static const char source[] =
        "var max = 0; (function() {"
        "    var array = [];"
        "    for (; ; ++max)"
        "        array.push({});"
        "    array = []; array.push(0);"
        "})();";
    JS::CompileOptions opts(cx);
    bool ok = JS::Evaluate(cx, opts, source, strlen(source), &root);

    /* Check that we get OOM. */
    CHECK(!ok);
    CHECK(JS_GetPendingException(cx, &root));
    CHECK(root.isString());
    bool match = false;
    CHECK(JS_StringEqualsAscii(cx, root.toString(), "out of memory", &match));
    CHECK(match);
    JS_ClearPendingException(cx);

    JS_GC(cx);

    // The above GC should have discarded everything. Verify that we can now
    // allocate half as many objects without OOMing.
    EVAL("(function() {"
         "    var array = [];"
         "    for (var i = max >> 2; i != 0;) {"
         "        --i;"
         "        array.push({});"
         "    }"
         "})();", &root);
    CHECK(!JS_IsExceptionPending(cx));
    return true;
}

virtual JSContext* createContext() override {
    // Note that the max nursery size must be less than the whole heap size, or
    // the test will fail because 'max' (the number of allocations required for
    // OOM) will be based on the nursery size, and that will overflow the
    // tenured heap, which will cause the second pass with max/4 allocations to
    // OOM. (Actually, this only happens with nursery zeal, because normally
    // the nursery will start out with only a single chunk before triggering a
    // major GC.)
    JSContext* cx = JS_NewContext(768 * 1024, 128 * 1024);
    if (!cx)
        return nullptr;
    setNativeStackQuota(cx);
    return cx;
}

virtual void destroyContext() override {
    JS_DestroyContext(cx);
}

END_TEST(testGCOutOfMemory)
