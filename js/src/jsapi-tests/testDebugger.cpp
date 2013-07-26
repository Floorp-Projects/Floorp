/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jsdbgapi.h"

#include "jsapi-tests/tests.h"

static int callCount[2] = {0, 0};

static void *
callCountHook(JSContext *cx, JSAbstractFramePtr frame, bool isConstructing, JSBool before,
              JSBool *ok, void *closure)
{
    callCount[before]++;

    JS::RootedValue thisv(cx);
    frame.getThisValue(cx, &thisv); // assert if fp is incomplete

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
nonStrictThisHook(JSContext *cx, JSAbstractFramePtr frame, bool isConstructing, JSBool before,
                  JSBool *ok, void *closure)
{
    if (before) {
        bool *allWrapped = (bool *) closure;
        JS::RootedValue thisv(cx);
        frame.getThisValue(cx, &thisv);
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
strictThisHook(JSContext *cx, JSAbstractFramePtr frame, bool isConstructing, JSBool before,
               JSBool *ok, void *closure)
{
    if (before) {
        bool *anyWrapped = (bool *) closure;
        JS::RootedValue thisv(cx);
        frame.getThisValue(cx, &thisv);
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

    JS::RootedObject global(cx, JS_GetGlobalForScopeChain(cx));

    char text[] = "new Error()";
    jsval _;
    JS_EvaluateScript(cx, global, text, strlen(text), "", 0, &_);

    return JSTRAP_CONTINUE;
}

BEGIN_TEST(testDebugger_throwHook)
{
    CHECK(JS_SetDebugMode(cx, true));
    CHECK(JS_SetThrowHook(rt, ThrowHook, NULL));
    EXEC("function foo() { throw 3 };\n"
         "for (var i = 0; i < 10; ++i) { \n"
         "  var x = {}\n"
         "  try {\n"
         "    foo(); \n"
         "  } catch(e) {}\n"
         "}\n");
    CHECK(called);
    CHECK(JS_SetThrowHook(rt, NULL, NULL));
    return true;
}
END_TEST(testDebugger_throwHook)

BEGIN_TEST(testDebugger_debuggerObjectVsDebugMode)
{
    CHECK(JS_DefineDebuggerObject(cx, global));
    JS::RootedObject debuggee(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL));
    CHECK(debuggee);

    {
        JSAutoCompartment ae(cx, debuggee);
        CHECK(JS_SetDebugMode(cx, true));
        CHECK(JS_InitStandardClasses(cx, debuggee));
    }

    JS::RootedObject debuggeeWrapper(cx, debuggee);
    CHECK(JS_WrapObject(cx, debuggeeWrapper.address()));
    JS::RootedValue v(cx, JS::ObjectValue(*debuggeeWrapper));
    CHECK(JS_SetProperty(cx, global, "debuggee", &v));

    EVAL("var dbg = new Debugger(debuggee);\n"
         "var hits = 0;\n"
         "dbg.onDebuggerStatement = function () { hits++; };\n"
         "debuggee.eval('debugger;');\n"
         "hits;\n",
         v.address());
    CHECK_SAME(v, JSVAL_ONE);

    {
        JSAutoCompartment ae(cx, debuggee);
        CHECK(JS_SetDebugMode(cx, false));
    }

    EVAL("debuggee.eval('debugger; debugger; debugger;');\n"
         "hits;\n",
         v.address());
    CHECK_SAME(v, INT_TO_JSVAL(4));

    return true;
}
END_TEST(testDebugger_debuggerObjectVsDebugMode)

BEGIN_TEST(testDebugger_newScriptHook)
{
    // Test that top-level indirect eval fires the newScript hook.
    CHECK(JS_DefineDebuggerObject(cx, global));
    JS::RootedObject g(cx, JS_NewGlobalObject(cx, getGlobalClass(), NULL));
    CHECK(g);
    {
        JSAutoCompartment ae(cx, g);
        CHECK(JS_InitStandardClasses(cx, g));
    }

    JS::RootedObject gWrapper(cx, g);
    CHECK(JS_WrapObject(cx, gWrapper.address()));
    JS::RootedValue v(cx, JS::ObjectValue(*gWrapper));
    CHECK(JS_SetProperty(cx, global, "g", &v));

    EXEC("var dbg = Debugger(g);\n"
         "var hits = 0;\n"
         "dbg.onNewScript = function (s) {\n"
         "    hits += Number(s instanceof Debugger.Script);\n"
         "};\n");

    // Since g is a debuggee, g.eval should trigger newScript, regardless of
    // what scope object we use to enter the compartment.
    //
    // Scripts are associated with the global where they're compiled, so we
    // deliver them only to debuggers that are watching that particular global.
    //
    return testIndirectEval(g, "Math.abs(0)");
}

bool testIndirectEval(JS::HandleObject scope, const char *code)
{
    EXEC("hits = 0;");

    {
        JSAutoCompartment ae(cx, scope);
        JSString *codestr = JS_NewStringCopyZ(cx, code);
        CHECK(codestr);
        jsval argv[1] = { STRING_TO_JSVAL(codestr) };
        JS::AutoArrayRooter rooter(cx, 1, argv);
        jsval v;
        CHECK(JS_CallFunctionName(cx, scope, "eval", 1, argv, &v));
    }

    JS::RootedValue hitsv(cx);
    EVAL("hits", hitsv.address());
    CHECK_SAME(hitsv, INT_TO_JSVAL(1));
    return true;
}
END_TEST(testDebugger_newScriptHook)

BEGIN_TEST(testDebugger_singleStepThrow)
    {
        CHECK(JS_SetDebugModeForCompartment(cx, cx->compartment(), true));
        CHECK(JS_SetInterrupt(rt, onStep, NULL));

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
        JSScript *script;
        JS_DescribeScriptedCaller(cx, &script, NULL);
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
