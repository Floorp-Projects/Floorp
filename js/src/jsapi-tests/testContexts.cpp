/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testContexts_IsRunning)
    {
        CHECK(JS_DefineFunction(cx, global, "chk", chk, 0, 0));
        EXEC("for (var i = 0; i < 9; i++) chk();");
        return true;
    }

    static JSBool chk(JSContext *cx, uintN argc, jsval *vp)
    {
        JSRuntime *rt = JS_GetRuntime(cx);
        JSContext *acx = JS_NewContext(rt, 8192);
        if (!acx) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }

        // acx should not be running
        bool ok = !JS_IsRunning(acx);
        if (!ok)
            JS_ReportError(cx, "Assertion failed: brand new context claims to be running");
        JS_DestroyContext(acx);
        return ok;
    }
END_TEST(testContexts_IsRunning)

BEGIN_TEST(testContexts_bug563735)
{
    JSContext *cx2 = JS_NewContext(rt, 8192);
    CHECK(cx2);

    JSBool ok;
    {
        JSAutoRequest req(cx2);
        JSAutoEnterCompartment ac;
        CHECK(ac.enter(cx2, global));
        jsval v = JSVAL_NULL;
        ok = JS_SetProperty(cx2, global, "x", &v);
    }
    CHECK(ok);

    EXEC("(function () { for (var i = 0; i < 9; i++) ; })();");

    JS_DestroyContext(cx2);
    return true;
}
END_TEST(testContexts_bug563735)
