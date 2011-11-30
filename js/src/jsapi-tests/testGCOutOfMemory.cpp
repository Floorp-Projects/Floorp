/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

#include "tests.h"
#include "jsgcchunk.h"
#include "jscntxt.h"

static unsigned errorCount = 0;

static void
ErrorCounter(JSContext *cx, const char *message, JSErrorReport *report)
{
    ++errorCount;
}

BEGIN_TEST(testGCOutOfMemory)
{
    JS_SetErrorReporter(cx, ErrorCounter);

    jsvalRoot root(cx);

    static const char source[] =
        "var max = 0; (function() {"
        "    var array = [];"
        "    for (; ; ++max)"
        "        array.push({});"
        "    array = []; array.push(0);"
        "})();";
    JSBool ok = JS_EvaluateScript(cx, global, source, strlen(source), "", 1,
                                  root.addr());

    /* Check that we get OOM. */
    CHECK(!ok);
    CHECK(!JS_IsExceptionPending(cx));
    CHECK_EQUAL(errorCount, 1);
    JS_GC(cx);
    EVAL("(function() {"
         "    var array = [];"
         "    for (var i = max >> 2; i != 0;) {"
         "        --i;"
         "        array.push({});"
         "    }"
         "})();", root.addr());
    CHECK_EQUAL(errorCount, 1);
    return true;
}

virtual JSRuntime * createRuntime() {
    return JS_NewRuntime(512 * 1024);
}

virtual void destroyRuntime() {
    JS_DestroyRuntime(rt);
}

END_TEST(testGCOutOfMemory)
