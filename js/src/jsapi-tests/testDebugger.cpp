/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsdbgapi.h"

static int callCount[2] = {0, 0};

static void *
callCountHook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok, void *closure)
{
    callCount[before]++;

    jsval thisv;
    JS_GetFrameThis(cx, fp, &thisv);  // assert if fp is incomplete

    return cx;  // any non-null value causes the hook to be called again after
}

BEGIN_TEST(testDebugger_bug519719)
{
    CHECK(JS_SetDebugMode(cx, JS_TRUE));
    JS_SetCallHook(rt, callCountHook, NULL);
    EXEC("function call(fn) { fn(0); }\n"
         "function f(g) { for (var i = 0; i < 9; i++) call(g); }\n"
         "f(Math.sin);\n"    // record loop, starting in f
         "f(Math.cos);\n");  // side exit in f -> call
    CHECK_EQUAL(callCount[0], 20);
    CHECK_EQUAL(callCount[1], 20);
    return true;
}
END_TEST(testDebugger_bug519719)

static void *
nonStrictThisHook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok, void *closure)
{
    if (before) {
        bool *allWrapped = (bool *) closure;
        jsval thisv;
        JS_GetFrameThis(cx, fp, &thisv);
        *allWrapped = *allWrapped && !JSVAL_IS_PRIMITIVE(thisv);
    }
    return NULL;
}

BEGIN_TEST(testDebugger_getThisNonStrict)
{
    bool allWrapped = true;
    CHECK(JS_SetDebugMode(cx, JS_TRUE));
    JS_SetCallHook(rt, nonStrictThisHook, (void *) &allWrapped);
    EXEC("function nonstrict() { }\n"
         "Boolean.prototype.nonstrict = nonstrict;\n"
         "String.prototype.nonstrict = nonstrict;\n"
         "Number.prototype.nonstrict = nonstrict;\n"
         "Object.prototype.nonstrict = nonstrict;\n"
         "nonstrict.call(true);\n"
         "true.nonstrict();\n"
         "nonstrict.call('');\n"
         "''.nonstrict();\n"
         "nonstrict.call(42);\n"
         "(42).nonstrict();\n"
         // The below don't really get 'wrapped', but it's okay.
         "nonstrict.call(undefined);\n"
         "nonstrict.call(null);\n"
         "nonstrict.call({});\n"
         "({}).nonstrict();\n");
    CHECK(allWrapped);
    return true;
}
END_TEST(testDebugger_getThisNonStrict)

static void *
strictThisHook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok, void *closure)
{
    if (before) {
        bool *anyWrapped = (bool *) closure;
        jsval thisv;
        JS_GetFrameThis(cx, fp, &thisv);
        *anyWrapped = *anyWrapped || !JSVAL_IS_PRIMITIVE(thisv);
    }
    return NULL;
}

BEGIN_TEST(testDebugger_getThisStrict)
{
    bool anyWrapped = false;
    CHECK(JS_SetDebugMode(cx, JS_TRUE));
    JS_SetCallHook(rt, strictThisHook, (void *) &anyWrapped);
    EXEC("function strict() { 'use strict'; }\n"
         "Boolean.prototype.strict = strict;\n"
         "String.prototype.strict = strict;\n"
         "Number.prototype.strict = strict;\n"
         "strict.call(true);\n"
         "true.strict();\n"
         "strict.call('');\n"
         "''.strict();\n"
         "strict.call(42);\n"
         "(42).strict();\n"
         "strict.call(undefined);\n"
         "strict.call(null);\n");
    CHECK(!anyWrapped);
    return true;
}
END_TEST(testDebugger_getThisStrict)

bool called = false;

static JSTrapStatus
ThrowHook(JSContext *cx, JSScript *, jsbytecode *, jsval *rval, void *closure)
{
    called = true;

    JSObject *global = JS_GetGlobalForScopeChain(cx);

    char text[] = "new Error()";
    jsval _;
    JS_EvaluateScript(cx, global, text, strlen(text), "", 0, &_);

    return JSTRAP_CONTINUE;
}

BEGIN_TEST(testDebugger_throwHook)
{
    uint32 newopts = JS_GetOptions(cx) | JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS;
    uint32 oldopts = JS_SetOptions(cx, newopts);

    JSDebugHooks hooks = { 0 };
    hooks.throwHook = ThrowHook;
    JSDebugHooks *old = JS_SetContextDebugHooks(cx, &hooks);
    EXEC("function foo() { throw 3 };\n"
         "for (var i = 0; i < 10; ++i) { \n"
         "  var x = <tag></tag>;\n"
         "  try {\n"
         "    foo(); \n"
         "  } catch(e) {}\n"
         "}\n");
    CHECK(called);

    JS_SetContextDebugHooks(cx, old);
    JS_SetOptions(cx, oldopts);
    return true;
}
END_TEST(testDebugger_throwHook)
