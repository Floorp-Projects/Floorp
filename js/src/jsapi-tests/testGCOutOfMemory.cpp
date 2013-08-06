/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

#include "jscntxt.h"

#include "jsapi-tests/tests.h"

static unsigned errorCount = 0;

static void
ErrorCounter(JSContext *cx, const char *message, JSErrorReport *report)
{
    ++errorCount;
}

BEGIN_TEST(testGCOutOfMemory)
{
    JS_SetErrorReporter(cx, ErrorCounter);

    JS::RootedValue root(cx);

    static const char source[] =
        "var max = 0; (function() {"
        "    var array = [];"
        "    for (; ; ++max)"
        "        array.push({});"
        "    array = []; array.push(0);"
        "})();";
    bool ok = JS_EvaluateScript(cx, global, source, strlen(source), "", 1,
                                  root.address());

    /* Check that we get OOM. */
    CHECK(!ok);
    CHECK(!JS_IsExceptionPending(cx));
    CHECK_EQUAL(errorCount, 1);
    JS_GC(rt);

    // Temporarily disabled to reopen the tree. Bug 847579.
    return true;

    EVAL("(function() {"
         "    var array = [];"
         "    for (var i = max >> 2; i != 0;) {"
         "        --i;"
         "        array.push({});"
         "    }"
         "})();", root.address());
    CHECK_EQUAL(errorCount, 1);
    return true;
}

virtual JSRuntime * createRuntime() {
    JSRuntime *rt = JS_NewRuntime(768 * 1024, JS_USE_HELPER_THREADS);
    if (!rt)
        return NULL;
    setNativeStackQuota(rt);
    return rt;
}

virtual void destroyRuntime() {
    JS_DestroyRuntime(rt);
}

END_TEST(testGCOutOfMemory)
