/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
#include "tests.h"

static const char CODE[] =
    "function f()                                                   "
    "{                                                              "
    "  return new otherGlobal.Array() instanceof otherGlobal.Array; "
    "}                                                              "
    "f() && f() && f();                                             ";

BEGIN_TEST(testCrossGlobal_call)
{
    JSObject *otherGlobal = JS_NewGlobalObject(cx, basicGlobalClass());
    CHECK(otherGlobal);

    JSBool res = JS_InitStandardClasses(cx, otherGlobal);
    CHECK(res);

    res = JS_DefineProperty(cx, global, "otherGlobal", OBJECT_TO_JSVAL(otherGlobal),
                            JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
    CHECK(res);

    uintN oldopts = JS_GetOptions(cx);
    uintN newopts = oldopts & ~(JSOPTION_JIT | JSOPTION_METHODJIT | JSOPTION_PROFILING);
    newopts |= JSOPTION_METHODJIT;
    JS_SetOptions(cx, newopts);

    jsval rv;
    EVAL(CODE, &rv);

    CHECK_SAME(rv, JSVAL_TRUE);

    JS_SetOptions(cx, oldopts);

    return true;
}
END_TEST(testCrossGlobal_call)

BEGIN_TEST(testCrossGlobal_compileAndGo)
{

    JSObject *otherGlobal = JS_NewGlobalObject(cx, basicGlobalClass());
    CHECK(otherGlobal);

    JSBool res = JS_InitStandardClasses(cx, otherGlobal);
    CHECK(res);

    res = JS_DefineProperty(cx, global, "otherGlobal", OBJECT_TO_JSVAL(otherGlobal),
                            JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
    CHECK(res);

    res = JS_DefineProperty(cx, otherGlobal, "otherGlobal", OBJECT_TO_JSVAL(otherGlobal),
                            JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE);
    CHECK(res);

    uintN oldopts = JS_GetOptions(cx);
    uintN newopts =
        oldopts
        & ~(JSOPTION_COMPILE_N_GO | JSOPTION_JIT | JSOPTION_METHODJIT | JSOPTION_PROFILING);
    newopts |= JSOPTION_METHODJIT;
    JS_SetOptions(cx, newopts);

    jsval rv;

    // Compile script

    JSScript *script = JS_CompileScript(cx, global, CODE, strlen(CODE), __FILE__, __LINE__);
    CHECK(script);
    JSObject *scriptObj = JS_NewScriptObject(cx, script);
    CHECK(scriptObj);
    JS::Anchor<JSObject *> anch(scriptObj);

    // Run script against new other global
    res = JS_ExecuteScript(cx, otherGlobal, script, &rv);
    CHECK(res);
    CHECK_SAME(rv, JSVAL_TRUE);

    // Run script against original global
    res = JS_ExecuteScript(cx, global, script, &rv);
    CHECK(res);
    CHECK_SAME(rv, JSVAL_TRUE);

    JS_SetOptions(cx, oldopts);

    return true;
}
END_TEST(testCrossGlobal_compileAndGo)
