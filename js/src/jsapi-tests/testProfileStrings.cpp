/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Tests the stack-based instrumentation profiler on a JSRuntime
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"

#include "jscntxt.h"

static js::ProfileEntry stack[10];
static uint32_t size = 0;
static uint32_t max_stack = 0;

static void
reset(JSContext *cx)
{
    size = max_stack = 0;
    memset(stack, 0, sizeof(stack));
    cx->runtime->spsProfiler.stringsReset();
}

static JSClass ptestClass = {
    "Prof", 0, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static JSBool
test_fn(JSContext *cx, unsigned argc, jsval *vp)
{
    max_stack = size;
    return JS_TRUE;
}

static JSBool
test_fn2(JSContext *cx, unsigned argc, jsval *vp)
{
    jsval r;
    return JS_CallFunctionName(cx, JS_GetGlobalObject(cx), "d", 0, NULL, &r);
}

static JSBool
test_fn3(JSContext *cx, unsigned argc, jsval *vp)
{
    js::SetRuntimeProfilingStack(cx->runtime, stack, &size, 10);
    return JS_TRUE;
}

static JSBool
Prof(JSContext* cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_NewObjectForConstructor(cx, &ptestClass, vp);
    if (!obj)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
    return JS_TRUE;
}

static JSFunctionSpec ptestFunctions[] = {
    JS_FS("test_fn", test_fn, 0, 0),
    JS_FS("test_fn2", test_fn2, 0, 0),
    JS_FS("test_fn3", test_fn3, 0, 0),
    JS_FS_END
};

static JSObject*
initialize(JSContext *cx)
{
    js::SetRuntimeProfilingStack(cx->runtime, stack, &size, 10);
    return JS_InitClass(cx, JS_GetGlobalObject(cx), NULL, &ptestClass, Prof, 0,
                        NULL, ptestFunctions, NULL, NULL);
}

BEGIN_TEST(testProfileStrings_isCalled)
{
    CHECK(initialize(cx));

    EXEC("function g() { var p = new Prof(); p.test_fn(); }");
    EXEC("function f() { g(); }");
    EXEC("function e() { f(); }");
    EXEC("function d() { e(); }");
    EXEC("function c() { d(); }");
    EXEC("function b() { c(); }");
    EXEC("function a() { b(); }");
    EXEC("function check() { var p = new Prof(); p.test_fn(); a(); }");
    EXEC("function check2() { var p = new Prof(); p.test_fn2(); }");

    reset(cx);
    {
        jsvalRoot rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK(size == 0);
        CHECK(max_stack == 9);
        CHECK(cx->runtime->spsProfiler.stringsCount() == 8);
        /* Make sure the stack resets and we added no new entries */
        max_stack = 0;
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK(size == 0);
        CHECK(max_stack == 9);
        CHECK(cx->runtime->spsProfiler.stringsCount() == 8);
    }
    reset(cx);
    {
        jsvalRoot rval(cx);
        CHECK(JS_CallFunctionName(cx, global, "check2", 0, NULL, rval.addr()));
        CHECK(cx->runtime->spsProfiler.stringsCount() == 5);
        CHECK(max_stack == 7);
        CHECK(size == 0);
    }
    reset(cx);
    {
        jsvalRoot rval(cx);
        js::SetRuntimeProfilingStack(cx->runtime, stack, &size, 3);
        stack[3].string = (char*) 1234;
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK((size_t) stack[3].string == 1234);
        CHECK(max_stack == 9);
        CHECK(size == 0);
    }
    return true;
}
END_TEST(testProfileStrings_isCalled)

BEGIN_TEST(testProfileStrings_isCalledWithJIT)
{
    CHECK(initialize(cx));
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_METHODJIT |
                        JSOPTION_METHODJIT_ALWAYS);

    EXEC("function g() { var p = new Prof(); p.test_fn(); }");
    EXEC("function f() { g(); }");
    EXEC("function e() { f(); }");
    EXEC("function d() { e(); }");
    EXEC("function c() { d(); }");
    EXEC("function b() { c(); }");
    EXEC("function a() { b(); }");
    EXEC("function check() { var p = new Prof(); p.test_fn(); a(); }");
    EXEC("function check2() { var p = new Prof(); p.test_fn2(); }");

    reset(cx);
    {
        jsvalRoot rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK(size == 0);
        CHECK(max_stack == 9);

        /* Make sure the stack resets and we added no new entries */
        uint32_t cnt = cx->runtime->spsProfiler.stringsCount();
        max_stack = 0;
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK(size == 0);
        CHECK(cx->runtime->spsProfiler.stringsCount() == cnt);
        CHECK(max_stack == 9);
    }
    reset(cx);
    {
        /* Limit the size of the stack and make sure we don't overflow */
        jsvalRoot rval(cx);
        js::SetRuntimeProfilingStack(cx->runtime, stack, &size, 3);
        stack[3].string = (char*) 1234;
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK(size == 0);
        CHECK(max_stack == 9);
        CHECK((size_t) stack[3].string == 1234);
    }
    return true;
}
END_TEST(testProfileStrings_isCalledWithJIT)

BEGIN_TEST(testProfileStrings_isCalledWhenError)
{
    CHECK(initialize(cx));
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_METHODJIT |
                        JSOPTION_METHODJIT_ALWAYS);

    EXEC("function check2() { throw 'a'; }");

    reset(cx);
    {
        jsvalRoot rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        JS_CallFunctionName(cx, global, "check2", 0, NULL, rval.addr());
        CHECK(size == 0);
        CHECK(cx->runtime->spsProfiler.stringsCount() == 1);
    }
    return true;
}
END_TEST(testProfileStrings_isCalledWhenError)

BEGIN_TEST(testProfileStrings_worksWhenEnabledOnTheFly)
{
    CHECK(initialize(cx));
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_METHODJIT |
                        JSOPTION_METHODJIT_ALWAYS);

    EXEC("function b() { }");
    EXEC("function a() { var p = new Prof(); p.test_fn3(); b(); }");

    reset(cx);
    js::SetRuntimeProfilingStack(cx->runtime, NULL, NULL, 10);
    {
        jsvalRoot rval(cx);
        JS_CallFunctionName(cx, global, "a", 0, NULL, rval.addr());
        CHECK(size == 0);
        CHECK(cx->runtime->spsProfiler.stringsCount() == 1);
    }
    return true;
}
END_TEST(testProfileStrings_worksWhenEnabledOnTheFly)
