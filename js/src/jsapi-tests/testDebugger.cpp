/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsdbgapi.h"

static int callCount[2] = {0, 0};

static void *
callHook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok, void *closure)
{
    callCount[before]++;
    JS_GetFrameThis(cx, fp);  // assert if fp is incomplete
    return cx;  // any non-null value causes the hook to be called again after
}

BEGIN_TEST(testDebugger_bug519719)
{
    JS_SetCallHook(rt, callHook, NULL);
    EXEC("function call(fn) { fn(0); }\n"
         "function f(g) { for (var i = 0; i < 9; i++) call(g); }\n"
         "f(Math.sin);\n"    // record loop, starting in f
         "f(Math.cos);\n");  // side exit in f -> call
    CHECK(callCount[0] == 20);
    CHECK(callCount[1] == 20);
    return true;
}
END_TEST(testDebugger_bug519719)
