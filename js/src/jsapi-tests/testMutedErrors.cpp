/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testMutedErrors) {
  CHECK(testOuter("function f() {return 1}; f;"));
  CHECK(testOuter(
      "function outer() { return (function () {return 2}); }; outer();"));
  CHECK(testOuter("eval('(function() {return 3})');"));
  CHECK(
      testOuter("(function (){ return eval('(function() {return 4})'); })()"));
  CHECK(
      testOuter("(function (){ return eval('(function() { return "
                "eval(\"(function(){return 5})\") })()'); })()"));
  CHECK(testOuter("new Function('return 6')"));
  CHECK(testOuter("function f() { return new Function('return 7') }; f();"));
  CHECK(testOuter("eval('new Function(\"return 8\")')"));
  CHECK(
      testOuter("(new Function('return eval(\"(function(){return 9})\")'))()"));
  CHECK(testOuter("(function(){return function(){return 10}}).bind()()"));
  CHECK(testOuter(
      "var e = eval; (function() { return e('(function(){return 11})') })()"));

  CHECK(testError("eval(-)"));
  CHECK(testError("-"));
  CHECK(testError("new Function('x', '-')"));
  CHECK(testError("eval('new Function(\"x\", \"-\")')"));

  /*
   * NB: uncaught exceptions, when reported, have nothing on the stack so
   * both the filename and mutedErrors are empty. E.g., this would fail:
   *
   *   CHECK(testError("throw 3"));
   */
  return true;
}

bool eval(const char* asciiChars, bool mutedErrors,
          JS::MutableHandleValue rval) {
  size_t len = strlen(asciiChars);
  mozilla::UniquePtr<char16_t[]> chars(new char16_t[len + 1]);
  for (size_t i = 0; i < len; ++i) {
    chars[i] = asciiChars[i];
  }
  chars[len] = 0;

  JS::RealmOptions globalOptions;
  JS::RootedObject global(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(global);
  JSAutoRealm ar(cx, global);

  JS::CompileOptions options(cx);
  options.setMutedErrors(mutedErrors).setFileAndLine("", 0);

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, chars.get(), len, JS::SourceOwnership::Borrowed));

  return JS::Evaluate(cx, options, srcBuf, rval);
}

bool testOuter(const char* asciiChars) {
  CHECK(testInner(asciiChars, false));
  CHECK(testInner(asciiChars, true));
  return true;
}

bool testInner(const char* asciiChars, bool mutedErrors) {
  JS::RootedValue rval(cx);
  CHECK(eval(asciiChars, mutedErrors, &rval));

  JS::RootedFunction fun(cx, &rval.toObject().as<JSFunction>());
  JSScript* script = JS_GetFunctionScript(cx, fun);
  CHECK(JS_ScriptHasMutedErrors(script) == mutedErrors);

  return true;
}

bool testError(const char* asciiChars) {
  JS::RootedValue rval(cx);
  CHECK(!eval(asciiChars, true, &rval));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  JS::ErrorReportBuilder report(cx);
  CHECK(report.init(cx, exnStack, JS::ErrorReportBuilder::WithSideEffects));
  CHECK(report.report()->isMuted == true);
  return true;
}
END_TEST(testMutedErrors)
