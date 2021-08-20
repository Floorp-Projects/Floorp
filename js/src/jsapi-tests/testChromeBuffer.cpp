/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CallAndConstruct.h"          // JS_CallFunctionValue
#include "js/CompilationAndEvaluation.h"  // JS::CompileFunction
#include "js/ContextOptions.h"
#include "js/GlobalObject.h"        // JS_NewGlobalObject
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/SourceText.h"          // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "util/Text.h"

static TestJSPrincipals system_principals(1);

static const JSClass global_class = {"global",
                                     JSCLASS_IS_GLOBAL | JSCLASS_GLOBAL_FLAGS,
                                     &JS::DefaultGlobalClassOps};

static JS::PersistentRootedObject trusted_glob;
static JS::PersistentRootedObject trusted_fun;

static bool CallTrusted(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  bool ok = false;
  {
    JSAutoRealm ar(cx, trusted_glob);
    JS::RootedValue funVal(cx, JS::ObjectValue(*trusted_fun));
    ok = JS_CallFunctionValue(cx, nullptr, funVal,
                              JS::HandleValueArray::empty(), args.rval());
  }
  return ok;
}

BEGIN_TEST(testChromeBuffer) {
  JS_SetTrustedPrincipals(cx, &system_principals);

  JS::RealmOptions options;
  trusted_glob.init(cx,
                    JS_NewGlobalObject(cx, &global_class, &system_principals,
                                       JS::FireOnNewGlobalHook, options));
  CHECK(trusted_glob);

  JS::RootedFunction fun(cx);

  /*
   * Check that, even after untrusted content has exhausted the stack, code
   * compiled with "trusted principals" can run using reserved trusted-only
   * buffer space.
   */
  {
    // Disable the JIT because if we don't this test fails.  See bug 1160414.
    uint32_t oldBaselineInterpreterEnabled;
    CHECK(JS_GetGlobalJitCompilerOption(
        cx, JSJITCOMPILER_BASELINE_INTERPRETER_ENABLE,
        &oldBaselineInterpreterEnabled));
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_INTERPRETER_ENABLE,
                                  0);
    uint32_t oldBaselineJitEnabled;
    CHECK(JS_GetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_ENABLE,
                                        &oldBaselineJitEnabled));
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_ENABLE, 0);
    {
      JSAutoRealm ar(cx, trusted_glob);
      const char* paramName = "x";
      static const char bytes[] = "return x ? 1 + trusted(x-1) : 0";

      JS::SourceText<mozilla::Utf8Unit> srcBuf;
      CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                        JS::SourceOwnership::Borrowed));

      JS::CompileOptions options(cx);
      options.setFileAndLine("", 0);

      JS::RootedObjectVector emptyScopeChain(cx);
      fun = JS::CompileFunction(cx, emptyScopeChain, options, "trusted", 1,
                                &paramName, srcBuf);
      CHECK(fun);
      CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun,
                              JSPROP_ENUMERATE));
      trusted_fun.init(cx, JS_GetFunctionObject(fun));
    }

    JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
    CHECK(JS_WrapValue(cx, &v));

    const char* paramName = "trusted";
    static const char bytes[] =
        "try {                                      "
        "    return untrusted(trusted);             "
        "} catch (e) {                              "
        "    try {                                  "
        "        return trusted(100);               "
        "    } catch(e) {                           "
        "        return -1;                         "
        "    }                                      "
        "}                                          ";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                      JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine("", 0);

    JS::RootedObjectVector emptyScopeChain(cx);
    fun = JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                              &paramName, srcBuf);
    CHECK(fun);
    CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

    JS::RootedValue rval(cx);
    CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(v), &rval));
    CHECK(rval.toInt32() == 100);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_INTERPRETER_ENABLE,
                                  oldBaselineInterpreterEnabled);
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_ENABLE,
                                  oldBaselineJitEnabled);
  }

  /*
   * Check that content called from chrome in the reserved-buffer space
   * immediately ooms.
   */
  {
    {
      JSAutoRealm ar(cx, trusted_glob);

      const char* paramName = "untrusted";
      static const char bytes[] =
          "try {                                  "
          "  untrusted();                         "
          "} catch (e) {                          "
          "  /*                                   "
          "   * Careful!  We must not reenter JS  "
          "   * that might try to push a frame.   "
          "   */                                  "
          "  return 'From trusted: ' +            "
          "         e.name + ': ' + e.message;    "
          "}                                      ";

      JS::SourceText<mozilla::Utf8Unit> srcBuf;
      CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                        JS::SourceOwnership::Borrowed));

      JS::CompileOptions options(cx);
      options.setFileAndLine("", 0);

      JS::RootedObjectVector emptyScopeChain(cx);
      fun = JS::CompileFunction(cx, emptyScopeChain, options, "trusted", 1,
                                &paramName, srcBuf);
      CHECK(fun);
      CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun,
                              JSPROP_ENUMERATE));
      trusted_fun = JS_GetFunctionObject(fun);
    }

    JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
    CHECK(JS_WrapValue(cx, &v));

    const char* paramName = "trusted";
    static const char bytes[] =
        "try {                                      "
        "  return untrusted(trusted);               "
        "} catch (e) {                              "
        "  return trusted(untrusted);               "
        "}                                          ";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                      JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine("", 0);

    JS::RootedObjectVector emptyScopeChain(cx);
    fun = JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                              &paramName, srcBuf);
    CHECK(fun);
    CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

    JS::RootedValue rval(cx);
    CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(v), &rval));
#ifndef JS_SIMULATOR_ARM64
    // The ARM64 simulator does not share a common implementation with the other
    // simulators, and has slightly different end-of-stack behavior. Instead of
    // failing with "too much recursion," it executes one more function call and
    // fails with a type error. This behavior is not incorrect.
    bool match;
    CHECK(JS_StringEqualsAscii(
        cx, rval.toString(), "From trusted: InternalError: too much recursion",
        &match));
    CHECK(match);
#endif
  }

  {
    {
      JSAutoRealm ar(cx, trusted_glob);

      static const char bytes[] = "return 42";

      JS::SourceText<mozilla::Utf8Unit> srcBuf;
      CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                        JS::SourceOwnership::Borrowed));

      JS::CompileOptions options(cx);
      options.setFileAndLine("", 0);

      JS::RootedObjectVector emptyScopeChain(cx);
      fun = JS::CompileFunction(cx, emptyScopeChain, options, "trusted", 0,
                                nullptr, srcBuf);
      CHECK(fun);
      CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun,
                              JSPROP_ENUMERATE));
      trusted_fun = JS_GetFunctionObject(fun);
    }

    JS::RootedFunction fun(
        cx, JS_NewFunction(cx, CallTrusted, 0, 0, "callTrusted"));
    JS::RootedObject callTrusted(cx, JS_GetFunctionObject(fun));

    const char* paramName = "f";
    static const char bytes[] =
        "try {                                      "
        "  return untrusted(trusted);               "
        "} catch (e) {                              "
        "  return f();                              "
        "}                                          ";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, bytes, js_strlen(bytes),
                      JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine("", 0);

    JS::RootedObjectVector emptyScopeChain(cx);
    fun = JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                              &paramName, srcBuf);
    CHECK(fun);
    CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

    JS::RootedValue arg(cx, JS::ObjectValue(*callTrusted));
    JS::RootedValue rval(cx);
    CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(arg), &rval));
    CHECK(rval.toInt32() == 42);
  }

  return true;
}
END_TEST(testChromeBuffer)
