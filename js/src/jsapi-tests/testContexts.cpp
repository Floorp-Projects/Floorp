#include "tests.h"

BEGIN_TEST(testContexts_IsRunning)
    {
        CHECK(JS_DefineFunction(cx, global, "chk", chk, 0, 0));
        EXEC("for (var i = 0; i < 9; i++) chk();");
        return true;
    }

    static JSBool chk(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
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
