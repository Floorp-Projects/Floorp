/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Tests the stack-based instrumentation profiler on a JSRuntime
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"

#include "js/CallAndConstruct.h"
#include "js/ContextOptions.h"
#include "js/PropertySpec.h"
#include "jsapi-tests/tests.h"
#include "vm/JSContext.h"

static ProfilingStack profilingStack;
static uint32_t peakStackPointer = 0;

static void reset(JSContext* cx) {
  profilingStack.stackPointer = 0;
  cx->runtime()->geckoProfiler().stringsReset();
  cx->runtime()->geckoProfiler().enableSlowAssertions(true);
  js::EnableContextProfilingStack(cx, true);
}

static const JSClass ptestClass = {"Prof", 0};

static bool test_fn(JSContext* cx, unsigned argc, JS::Value* vp) {
  peakStackPointer = profilingStack.stackPointer;
  return true;
}

static bool test_fn2(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::RootedValue r(cx);
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  return JS_CallFunctionName(cx, global, "d", JS::HandleValueArray::empty(),
                             &r);
}

static bool enable(JSContext* cx, unsigned argc, JS::Value* vp) {
  js::EnableContextProfilingStack(cx, true);
  return true;
}

static bool disable(JSContext* cx, unsigned argc, JS::Value* vp) {
  js::EnableContextProfilingStack(cx, false);
  return true;
}

static bool Prof(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSObject* obj = JS_NewObjectForConstructor(cx, &ptestClass, args);
  if (!obj) {
    return false;
  }
  args.rval().setObject(*obj);
  return true;
}

static const JSFunctionSpec ptestFunctions[] = {
    JS_FN("test_fn", test_fn, 0, 0), JS_FN("test_fn2", test_fn2, 0, 0),
    JS_FN("enable", enable, 0, 0), JS_FN("disable", disable, 0, 0), JS_FS_END};

static JSObject* initialize(JSContext* cx) {
  js::SetContextProfilingStack(cx, &profilingStack);
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  return JS_InitClass(cx, global, nullptr, &ptestClass, Prof, 0, nullptr,
                      ptestFunctions, nullptr, nullptr);
}

BEGIN_TEST(testProfileStrings_isCalledWithInterpreter) {
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
    CHECK(JS_CallFunctionName(cx, global, "check",
                              JS::HandleValueArray::empty(), &rval));
    CHECK(profilingStack.stackPointer == 0);
    CHECK(peakStackPointer >= 8);
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == 8);
    /* Make sure the stack resets and we added no new entries */
    peakStackPointer = 0;
    CHECK(JS_CallFunctionName(cx, global, "check",
                              JS::HandleValueArray::empty(), &rval));
    CHECK(profilingStack.stackPointer == 0);
    CHECK(peakStackPointer >= 8);
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == 8);
  }
  reset(cx);
  {
    JS::RootedValue rval(cx);
    CHECK(JS_CallFunctionName(cx, global, "check2",
                              JS::HandleValueArray::empty(), &rval));
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == 5);
    CHECK(peakStackPointer >= 6);
    CHECK(profilingStack.stackPointer == 0);
  }
  return true;
}
END_TEST(testProfileStrings_isCalledWithInterpreter)

BEGIN_TEST(testProfileStrings_isCalledWithJIT) {
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
    CHECK(JS_CallFunctionName(cx, global, "check",
                              JS::HandleValueArray::empty(), &rval));
    CHECK(profilingStack.stackPointer == 0);
    CHECK(peakStackPointer >= 8);

    /* Make sure the stack resets and we added no new entries */
    uint32_t cnt = cx->runtime()->geckoProfiler().stringsCount();
    peakStackPointer = 0;
    CHECK(JS_CallFunctionName(cx, global, "check",
                              JS::HandleValueArray::empty(), &rval));
    CHECK(profilingStack.stackPointer == 0);
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == cnt);
    CHECK(peakStackPointer >= 8);
  }

  return true;
}
END_TEST(testProfileStrings_isCalledWithJIT)

BEGIN_TEST(testProfileStrings_isCalledWhenError) {
  CHECK(initialize(cx));

  EXEC("function check2() { throw 'a'; }");

  reset(cx);
  {
    JS::RootedValue rval(cx);
    /* Make sure the stack resets and we have an entry for each stack */
    bool ok = JS_CallFunctionName(cx, global, "check2",
                                  JS::HandleValueArray::empty(), &rval);
    CHECK(!ok);
    CHECK(profilingStack.stackPointer == 0);
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == 1);

    JS_ClearPendingException(cx);
  }

  return true;
}
END_TEST(testProfileStrings_isCalledWhenError)

BEGIN_TEST(testProfileStrings_worksWhenEnabledOnTheFly) {
  CHECK(initialize(cx));

  EXEC("function b(p) { p.test_fn(); }");
  EXEC("function a() { var p = new Prof(); p.enable(); b(p); }");
  reset(cx);
  js::EnableContextProfilingStack(cx, false);
  {
    /* enable it in the middle of JS and make sure things check out */
    JS::RootedValue rval(cx);
    JS_CallFunctionName(cx, global, "a", JS::HandleValueArray::empty(), &rval);
    CHECK(profilingStack.stackPointer == 0);
    CHECK(peakStackPointer >= 1);
    CHECK(cx->runtime()->geckoProfiler().stringsCount() == 1);
  }

  EXEC("function d(p) { p.disable(); }");
  EXEC("function c() { var p = new Prof(); d(p); }");
  reset(cx);
  {
    /* now disable in the middle of js */
    JS::RootedValue rval(cx);
    JS_CallFunctionName(cx, global, "c", JS::HandleValueArray::empty(), &rval);
    CHECK(profilingStack.stackPointer == 0);
  }

  EXEC("function e() { var p = new Prof(); d(p); p.enable(); b(p); }");
  reset(cx);
  {
    /* now disable in the middle of js, but re-enable before final exit */
    JS::RootedValue rval(cx);
    JS_CallFunctionName(cx, global, "e", JS::HandleValueArray::empty(), &rval);
    CHECK(profilingStack.stackPointer == 0);
    CHECK(peakStackPointer >= 3);
  }

  EXEC("function h() { }");
  EXEC("function g(p) { p.disable(); for (var i = 0; i < 100; i++) i++; }");
  EXEC("function f() { g(new Prof()); }");
  reset(cx);
  cx->runtime()->geckoProfiler().enableSlowAssertions(false);
  {
    JS::RootedValue rval(cx);
    /* disable, and make sure that if we try to re-enter the JIT the pop
     * will still happen */
    JS_CallFunctionName(cx, global, "f", JS::HandleValueArray::empty(), &rval);
    CHECK(profilingStack.stackPointer == 0);
  }
  return true;
}
END_TEST(testProfileStrings_worksWhenEnabledOnTheFly)
