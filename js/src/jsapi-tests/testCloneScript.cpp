/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Test script cloning.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(test_cloneScript)
{
    JS::RootedObject A(cx, createGlobal());
    JS::RootedObject B(cx, createGlobal());

    CHECK(A);
    CHECK(B);

    const char *source =
        "var i = 0;\n"
        "var sum = 0;\n"
        "while (i < 10) {\n"
        "    sum += i;\n"
        "    ++i;\n"
        "}\n"
        "(sum);\n";

    JS::RootedObject obj(cx);

    // compile for A
    {
        JSAutoCompartment a(cx, A);
        JS::RootedFunction fun(cx);
        JS::CompileOptions options(cx);
        options.setFileAndLine(__FILE__, 1);
        JS::AutoObjectVector emptyScopeChain(cx);
        CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "f", 0, nullptr,
                                  source, strlen(source), &fun));
        CHECK(obj = JS_GetFunctionObject(fun));
    }

    // clone into B
    {
        JSAutoCompartment b(cx, B);
        CHECK(JS::CloneFunctionObject(cx, obj));
    }

    return true;
}
END_TEST(test_cloneScript)

static void
DestroyPrincipals(JSPrincipals *principals)
{
    delete principals;
}

struct Principals : public JSPrincipals
{
  public:
    Principals()
    {
        refcount = 0;
    }
};

class AutoDropPrincipals
{
    JSRuntime *rt;
    JSPrincipals *principals;

  public:
    AutoDropPrincipals(JSRuntime *rt, JSPrincipals *principals)
      : rt(rt), principals(principals)
    {
        JS_HoldPrincipals(principals);
    }

    ~AutoDropPrincipals()
    {
        JS_DropPrincipals(rt, principals);
    }
};

BEGIN_TEST(test_cloneScriptWithPrincipals)
{
    JS_InitDestroyPrincipalsCallback(rt, DestroyPrincipals);

    JSPrincipals *principalsA = new Principals();
    AutoDropPrincipals dropA(rt, principalsA);
    JSPrincipals *principalsB = new Principals();
    AutoDropPrincipals dropB(rt, principalsB);

    JS::RootedObject A(cx, createGlobal(principalsA));
    JS::RootedObject B(cx, createGlobal(principalsB));

    CHECK(A);
    CHECK(B);

    const char *argnames[] = { "arg" };
    const char *source = "return function() { return arg; }";

    JS::RootedObject obj(cx);

    // Compile in A
    {
        JSAutoCompartment a(cx, A);
        JS::CompileOptions options(cx);
        options.setFileAndLine(__FILE__, 1);
        JS::RootedFunction fun(cx);
        JS::AutoObjectVector emptyScopeChain(cx);
        JS::CompileFunction(cx, emptyScopeChain, options, "f",
                           mozilla::ArrayLength(argnames), argnames, source,
                           strlen(source), &fun);
        CHECK(fun);

        JSScript *script;
        CHECK(script = JS_GetFunctionScript(cx, fun));

        CHECK(JS_GetScriptPrincipals(script) == principalsA);
        CHECK(obj = JS_GetFunctionObject(fun));
    }

    // Clone into B
    {
        JSAutoCompartment b(cx, B);
        JS::RootedObject cloned(cx);
        CHECK(cloned = JS::CloneFunctionObject(cx, obj));

        JS::RootedFunction fun(cx);
        JS::RootedValue clonedValue(cx, JS::ObjectValue(*cloned));
        CHECK(fun = JS_ValueToFunction(cx, clonedValue));

        JSScript *script;
        CHECK(script = JS_GetFunctionScript(cx, fun));

        CHECK(JS_GetScriptPrincipals(script) == principalsB);

        JS::RootedValue v(cx);
        JS::RootedValue arg(cx, JS::Int32Value(1));
        CHECK(JS_CallFunctionValue(cx, B, clonedValue, JS::HandleValueArray(arg), &v));
        CHECK(v.isObject());

        JSObject *funobj = &v.toObject();
        CHECK(JS_ObjectIsFunction(cx, funobj));
        CHECK(fun = JS_ValueToFunction(cx, v));
        CHECK(script = JS_GetFunctionScript(cx, fun));
        CHECK(JS_GetScriptPrincipals(script) == principalsB);
    }

    return true;
}
END_TEST(test_cloneScriptWithPrincipals)
