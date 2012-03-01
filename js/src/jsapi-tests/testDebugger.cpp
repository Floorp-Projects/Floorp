/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsdbgapi.h"
#include "jscntxt.h"

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
    JS_ASSERT(!closure);
    called = true;

    JSObject *global = JS_GetGlobalForScopeChain(cx);

    char text[] = "new Error()";
    jsval _;
    JS_EvaluateScript(cx, global, text, strlen(text), "", 0, &_);

    return JSTRAP_CONTINUE;
}

BEGIN_TEST(testDebugger_throwHook)
{
    uint32_t newopts = JS_GetOptions(cx) | JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS;
    uint32_t oldopts = JS_SetOptions(cx, newopts);

    CHECK(JS_SetThrowHook(rt, ThrowHook, NULL));
    EXEC("function foo() { throw 3 };\n"
         "for (var i = 0; i < 10; ++i) { \n"
         "  var x = <tag></tag>;\n"
         "  try {\n"
         "    foo(); \n"
         "  } catch(e) {}\n"
         "}\n");
    CHECK(called);
    CHECK(JS_SetThrowHook(rt, NULL, NULL));
    JS_SetOptions(cx, oldopts);
    return true;
}
END_TEST(testDebugger_throwHook)

BEGIN_TEST(testDebugger_debuggerObjectVsDebugMode)
{
    CHECK(JS_DefineDebuggerObject(cx, global));
    JSObject *debuggee = JS_NewCompartmentAndGlobalObject(cx, getGlobalClass(), NULL);
    CHECK(debuggee);

    {
        JSAutoEnterCompartment ae;
        CHECK(ae.enter(cx, debuggee));
        CHECK(JS_SetDebugMode(cx, true));
        CHECK(JS_InitStandardClasses(cx, debuggee));
    }

    JSObject *debuggeeWrapper = debuggee;
    CHECK(JS_WrapObject(cx, &debuggeeWrapper));
    jsval v = OBJECT_TO_JSVAL(debuggeeWrapper);
    CHECK(JS_SetProperty(cx, global, "debuggee", &v));

    EVAL("var dbg = new Debugger(debuggee);\n"
         "var hits = 0;\n"
         "dbg.onDebuggerStatement = function () { hits++; };\n"
         "debuggee.eval('debugger;');\n"
         "hits;\n",
         &v);
    CHECK_SAME(v, JSVAL_ONE);

    {
        JSAutoEnterCompartment ae;
        CHECK(ae.enter(cx, debuggee));
        CHECK(JS_SetDebugMode(cx, false));
    }

    EVAL("debuggee.eval('debugger; debugger; debugger;');\n"
         "hits;\n",
         &v);
    CHECK_SAME(v, INT_TO_JSVAL(4));
    
    return true;
}
END_TEST(testDebugger_debuggerObjectVsDebugMode)

BEGIN_TEST(testDebugger_newScriptHook)
{
    // Test that top-level indirect eval fires the newScript hook.
    CHECK(JS_DefineDebuggerObject(cx, global));
    JSObject *g1, *g2;
    g1 = JS_NewCompartmentAndGlobalObject(cx, getGlobalClass(), NULL);
    CHECK(g1);
    {
        JSAutoEnterCompartment ae;
        CHECK(ae.enter(cx, g1));
        CHECK(JS_InitStandardClasses(cx, g1));
        g2 = JS_NewGlobalObject(cx, getGlobalClass());
        CHECK(g2);
        CHECK(JS_InitStandardClasses(cx, g2));
    }

    JSObject *g1Wrapper = g1;
    CHECK(JS_WrapObject(cx, &g1Wrapper));
    jsval v = OBJECT_TO_JSVAL(g1Wrapper);
    CHECK(JS_SetProperty(cx, global, "g1", &v));

    JSObject *g2Wrapper = g2;
    CHECK(JS_WrapObject(cx, &g2Wrapper));
    v = OBJECT_TO_JSVAL(g2Wrapper);
    CHECK(JS_SetProperty(cx, global, "g2", &v));

    EXEC("var dbg = Debugger(g1);\n"
         "var hits = 0;\n"
         "dbg.onNewScript = function (s) {\n"
         "    hits += Number(s instanceof Debugger.Script);\n"
         "};\n");

    // Since g1 is a debuggee and g2 is not, g1.eval should trigger newScript
    // and g2.eval should not, regardless of what scope object we use to enter
    // the compartment.
    //
    // (Not all scripts are permanently associated with specific global
    // objects, but eval scripts are, so we deliver them only to debuggers that
    // are watching that particular global.)
    //
    bool ok = true;
    ok = ok && testIndirectEval(g1, g1, "Math.abs(0)", 1);
    ok = ok && testIndirectEval(g2, g1, "Math.abs(1)", 1);
    ok = ok && testIndirectEval(g1, g2, "Math.abs(-1)", 0);
    ok = ok && testIndirectEval(g2, g2, "Math.abs(-2)", 0);
    return ok;
}

bool testIndirectEval(JSObject *scope, JSObject *g, const char *code, int expectedHits)
{
    EXEC("hits = 0;");

    {
        JSAutoEnterCompartment ae;
        CHECK(ae.enter(cx, scope));
        JSString *codestr = JS_NewStringCopyZ(cx, code);
        CHECK(codestr);
        jsval argv[1] = { STRING_TO_JSVAL(codestr) };
        jsval v;
        CHECK(JS_CallFunctionName(cx, g, "eval", 1, argv, &v));
    }

    jsval hitsv;
    EVAL("hits", &hitsv);
    CHECK_SAME(hitsv, INT_TO_JSVAL(expectedHits));
    return true;
}
END_TEST(testDebugger_newScriptHook)

BEGIN_TEST(testDebugger_singleStepThrow)
    {
        CHECK(JS_SetDebugModeForCompartment(cx, cx->compartment, true));
        CHECK(JS_SetInterrupt(rt, onStep, NULL));

        uint32_t opts = JS_GetOptions(cx);
        opts |= JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS;
        JS_SetOptions(cx, opts);

        CHECK(JS_DefineFunction(cx, global, "setStepMode", setStepMode, 0, 0));
        EXEC("var e;\n"
             "setStepMode();\n"
             "function f() { throw 0; }\n"
             "try { f(); }\n"
             "catch (x) { e = x; }\n");
        return true;
    }

    static JSBool
    setStepMode(JSContext *cx, unsigned argc, jsval *vp)
    {
        JSStackFrame *fp = JS_GetScriptedCaller(cx, NULL);
        JS_ASSERT(fp);
        JSScript *script = JS_GetFrameScript(cx, fp);
        JS_ASSERT(script);
        if (!JS_SetSingleStepMode(cx, script, true))
            return false;
        JS_SET_RVAL(cx, vp, JSVAL_VOID);
        return true;
    }

    static JSTrapStatus
    onStep(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
    {
        return JSTRAP_CONTINUE;
    }
END_TEST(testDebugger_singleStepThrow)

BEGIN_TEST(testDebugger_emptyObjectPropertyIterator)
{
    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    JSScopeProperty *prop = NULL;
    CHECK(!JS_PropertyIterator(obj, &prop));
    CHECK(!prop);

    return true;
}
END_TEST(testDebugger_emptyObjectPropertyIterator)

BEGIN_TEST(testDebugger_nonEmptyObjectPropertyIterator)
{
    jsval v;
    EVAL("({a: 15})", &v);
    JSObject *obj = JSVAL_TO_OBJECT(v);
    JSScopeProperty *prop = NULL;
    CHECK(JS_PropertyIterator(obj, &prop));
    JSPropertyDesc desc;
    CHECK(JS_GetPropertyDesc(cx, obj, prop, &desc));
    CHECK_EQUAL(JSVAL_IS_INT(desc.value), true);
    CHECK_EQUAL(JSVAL_TO_INT(desc.value), 15);
    CHECK(!JS_PropertyIterator(obj, &prop));
    CHECK(!prop);

    return true;
}
END_TEST(testDebugger_nonEmptyObjectPropertyIterator)
