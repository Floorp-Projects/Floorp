/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Tests the stack-based instrumentation profiler on a JSRuntime
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"

#include "jsapi-tests/tests.h"

static js::ProfileEntry pstack[10];
static uint32_t psize = 0;
static uint32_t max_stack = 0;

static void
reset(JSContext *cx)
{
    psize = max_stack = 0;
    memset(pstack, 0, sizeof(pstack));
    cx->runtime()->spsProfiler.stringsReset();
    cx->runtime()->spsProfiler.enableSlowAssertions(true);
    js::EnableRuntimeProfilingStack(cx->runtime(), true);
}

static const JSClass ptestClass = {
    "Prof", 0, JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub,
    JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static bool
test_fn(JSContext *cx, unsigned argc, jsval *vp)
{
    max_stack = psize;
    return true;
}

static bool
test_fn2(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::RootedValue r(cx);
    JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    return JS_CallFunctionName(cx, global, "d", JS::HandleValueArray::empty(), &r);
}

static bool
enable(JSContext *cx, unsigned argc, jsval *vp)
{
    js::EnableRuntimeProfilingStack(cx->runtime(), true);
    return true;
}

static bool
disable(JSContext *cx, unsigned argc, jsval *vp)
{
    js::EnableRuntimeProfilingStack(cx->runtime(), false);
    return true;
}

static bool
Prof(JSContext* cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject *obj = JS_NewObjectForConstructor(cx, &ptestClass, args);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static const JSFunctionSpec ptestFunctions[] = {
    JS_FS("test_fn", test_fn, 0, 0),
    JS_FS("test_fn2", test_fn2, 0, 0),
    JS_FS("enable", enable, 0, 0),
    JS_FS("disable", disable, 0, 0),
    JS_FS_END
};

static JSObject*
initialize(JSContext *cx)
{
    js::SetRuntimeProfilingStack(cx->runtime(), pstack, &psize, 10);
    JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    return JS_InitClass(cx, global, js::NullPtr(), &ptestClass, Prof, 0,
                        nullptr, ptestFunctions, nullptr, nullptr);
}

BEGIN_TEST(testProfileStrings_isCalledWithInterpreter)
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
        JS::RootedValue rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(psize == 0);
        CHECK(max_stack >= 8);
        CHECK(cx->runtime()->spsProfiler.stringsCount() == 8);
        /* Make sure the stack resets and we added no new entries */
        max_stack = 0;
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(psize == 0);
        CHECK(max_stack >= 8);
        CHECK(cx->runtime()->spsProfiler.stringsCount() == 8);
    }
    reset(cx);
    {
        JS::RootedValue rval(cx);
        CHECK(JS_CallFunctionName(cx, global, "check2", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(cx->runtime()->spsProfiler.stringsCount() == 5);
        CHECK(max_stack >= 6);
        CHECK(psize == 0);
    }
    js::EnableRuntimeProfilingStack(cx->runtime(), false);
    js::SetRuntimeProfilingStack(cx->runtime(), pstack, &psize, 3);
    reset(cx);
    {
        JS::RootedValue rval(cx);
        pstack[3].setLabel((char*) 1234);
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK((size_t) pstack[3].label() == 1234);
        CHECK(max_stack >= 8);
        CHECK(psize == 0);
    }
    return true;
}
END_TEST(testProfileStrings_isCalledWithInterpreter)

BEGIN_TEST(testProfileStrings_isCalledWithJIT)
{
    CHECK(initialize(cx));
    JS::RuntimeOptionsRef(cx).setBaseline(true)
                             .setIon(true);

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
        JS::RootedValue rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(psize == 0);
        CHECK(max_stack >= 8);

        /* Make sure the stack resets and we added no new entries */
        uint32_t cnt = cx->runtime()->spsProfiler.stringsCount();
        max_stack = 0;
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(psize == 0);
        CHECK(cx->runtime()->spsProfiler.stringsCount() == cnt);
        CHECK(max_stack >= 8);
    }

    js::EnableRuntimeProfilingStack(cx->runtime(), false);
    js::SetRuntimeProfilingStack(cx->runtime(), pstack, &psize, 3);
    reset(cx);
    {
        /* Limit the size of the stack and make sure we don't overflow */
        JS::RootedValue rval(cx);
        pstack[3].setLabel((char*) 1234);
        CHECK(JS_CallFunctionName(cx, global, "check", JS::HandleValueArray::empty(),
                                  &rval));
        CHECK(psize == 0);
        CHECK(max_stack >= 8);
        CHECK((size_t) pstack[3].label() == 1234);
    }
    return true;
}
END_TEST(testProfileStrings_isCalledWithJIT)

BEGIN_TEST(testProfileStrings_isCalledWhenError)
{
    CHECK(initialize(cx));
    JS::RuntimeOptionsRef(cx).setBaseline(true)
                             .setIon(true);

    EXEC("function check2() { throw 'a'; }");

    reset(cx);
    JS::ContextOptionsRef(cx).setDontReportUncaught(true);
    {
        JS::RootedValue rval(cx);
        /* Make sure the stack resets and we have an entry for each stack */
        bool ok = JS_CallFunctionName(cx, global, "check2", JS::HandleValueArray::empty(),
                                      &rval);
        CHECK(!ok);
        CHECK(psize == 0);
        CHECK(cx->runtime()->spsProfiler.stringsCount() == 1);

        JS_ClearPendingException(cx);
    }
    return true;
}
END_TEST(testProfileStrings_isCalledWhenError)

BEGIN_TEST(testProfileStrings_worksWhenEnabledOnTheFly)
{
    CHECK(initialize(cx));
    JS::RuntimeOptionsRef(cx).setBaseline(true)
                             .setIon(true);

    EXEC("function b(p) { p.test_fn(); }");
    EXEC("function a() { var p = new Prof(); p.enable(); b(p); }");
    reset(cx);
    js::EnableRuntimeProfilingStack(cx->runtime(), false);
    {
        /* enable it in the middle of JS and make sure things check out */
        JS::RootedValue rval(cx);
        JS_CallFunctionName(cx, global, "a", JS::HandleValueArray::empty(), &rval);
        CHECK(psize == 0);
        CHECK(max_stack >= 1);
        CHECK(cx->runtime()->spsProfiler.stringsCount() == 1);
    }

    EXEC("function d(p) { p.disable(); }");
    EXEC("function c() { var p = new Prof(); d(p); }");
    reset(cx);
    {
        /* now disable in the middle of js */
        JS::RootedValue rval(cx);
        JS_CallFunctionName(cx, global, "c", JS::HandleValueArray::empty(), &rval);
        CHECK(psize == 0);
    }

    EXEC("function e() { var p = new Prof(); d(p); p.enable(); b(p); }");
    reset(cx);
    {
        /* now disable in the middle of js, but re-enable before final exit */
        JS::RootedValue rval(cx);
        JS_CallFunctionName(cx, global, "e", JS::HandleValueArray::empty(), &rval);
        CHECK(psize == 0);
        CHECK(max_stack >= 3);
    }

    EXEC("function h() { }");
    EXEC("function g(p) { p.disable(); for (var i = 0; i < 100; i++) i++; }");
    EXEC("function f() { g(new Prof()); }");
    reset(cx);
    cx->runtime()->spsProfiler.enableSlowAssertions(false);
    {
        JS::RootedValue rval(cx);
        /* disable, and make sure that if we try to re-enter the JIT the pop
         * will still happen */
        JS_CallFunctionName(cx, global, "f", JS::HandleValueArray::empty(), &rval);
        CHECK(psize == 0);
    }
    return true;
}
END_TEST(testProfileStrings_worksWhenEnabledOnTheFly)
