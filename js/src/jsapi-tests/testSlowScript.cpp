#include "tests.h"

JSBool
OperationCallback(JSContext *cx)
{
    return false;
}

static unsigned sRemain;

JSBool
TriggerOperationCallback(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!sRemain--)
        JS_TriggerOperationCallback(JS_GetRuntime(cx));
    *vp = JSVAL_VOID;
    return true;
}

BEGIN_TEST(testSlowScript)
{
    JS_SetOperationCallback(cx, OperationCallback);
    JS_DefineFunction(cx, global, "triggerOperationCallback", TriggerOperationCallback, 0, 0);

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    triggerOperationCallback();");

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    for (j in [0,0,0,0])"
         "      triggerOperationCallback();");

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    for (j in [0,0,0,0])"
         "      for (k in [0,0,0,0])"
         "        triggerOperationCallback();");

    test("function f() { while (true) yield triggerOperationCallback() }"
         "for (i in f()) ;");

    test("function f() { while (true) yield 1 }"
         "for (i in f())"
         "  triggerOperationCallback();");

    test("(function() {"
         "  while (true)"
         "    let (x = 1) { eval('triggerOperationCallback()'); }"
         "})()");

    return true;
}

bool
test(const char *bytes)
{
    jsval v;

    JS_SetOptions(cx, JS_GetOptions(cx) & ~(JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS)); 
    sRemain = 0;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    sRemain = 1000;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS);

    sRemain = 0;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    sRemain = 1000;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    return true;
}
END_TEST(testSlowScript)
