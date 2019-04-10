/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::CompileForNonSyntacticScopeDontInflate
#include "js/PropertySpec.h"
#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "vm/EnvironmentObject.h"
#include "vm/EnvironmentObject-inl.h"

BEGIN_TEST(testExecuteInJSMEnvironment_Basic) {
  static const char src[] =
      "var output = input;\n"
      "\n"
      "a = 1;\n"
      "var b = 2;\n"
      "let c = 3;\n"
      "this.d = 4;\n"
      "eval('this.e = 5');\n"
      "(0,eval)('this.f = 6');\n"
      "(function() { this.g = 7; })();\n"
      "function f_h() { this.h = 8; }; f_h();\n";

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);
  options.setNoScriptRval(true);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, src, mozilla::ArrayLength(src) - 1,
                    JS::SourceOwnership::Borrowed));

  JS::RootedScript script(
      cx, JS::CompileForNonSyntacticScopeDontInflate(cx, options, srcBuf));
  CHECK(script);

  JS::RootedObject varEnv(cx, js::NewJSMEnvironment(cx));
  JS::RootedObject lexEnv(cx, JS_ExtensibleLexicalEnvironment(varEnv));
  CHECK(varEnv && varEnv->is<js::NonSyntacticVariablesObject>());
  CHECK(lexEnv && js::IsExtensibleLexicalEnvironment(lexEnv));
  CHECK(lexEnv->enclosingEnvironment() == varEnv);

  JS::RootedValue vi(cx, JS::Int32Value(1000));
  CHECK(JS_SetProperty(cx, varEnv, "input", vi));

  CHECK(js::ExecuteInJSMEnvironment(cx, script, varEnv));

  JS::RootedValue v(cx);
  CHECK(JS_GetProperty(cx, varEnv, "output", &v) && v == vi);
  CHECK(JS_GetProperty(cx, varEnv, "a", &v) && v == JS::Int32Value(1));
  CHECK(JS_GetProperty(cx, varEnv, "b", &v) && v == JS::Int32Value(2));
  CHECK(JS_GetProperty(cx, lexEnv, "c", &v) && v == JS::Int32Value(3));
  CHECK(JS_GetProperty(cx, varEnv, "d", &v) && v == JS::Int32Value(4));
  CHECK(JS_GetProperty(cx, varEnv, "e", &v) && v == JS::Int32Value(5));
  // TODO: Bug 1396050 will fix this
  // CHECK(JS_GetProperty(cx, varEnv, "f", &v) && v == JS::Int32Value(6));
  CHECK(JS_GetProperty(cx, varEnv, "g", &v) && v == JS::Int32Value(7));
  CHECK(JS_GetProperty(cx, varEnv, "h", &v) && v == JS::Int32Value(8));

  return true;
}
END_TEST(testExecuteInJSMEnvironment_Basic);

static bool test_callback(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::RootedObject env(cx, js::GetJSMEnvironmentOfScriptedCaller(cx));
  if (!env) {
    return false;
  }

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setObject(*env);
  return true;
}

static const JSFunctionSpec testFunctions[] = {
    JS_FN("callback", test_callback, 0, 0), JS_FS_END};

BEGIN_TEST(testExecuteInJSMEnvironment_Callback) {
  static const char src[] = "var output = callback();\n";

  CHECK(JS_DefineFunctions(cx, global, testFunctions));

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, __LINE__);
  options.setNoScriptRval(true);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, src, mozilla::ArrayLength(src) - 1,
                    JS::SourceOwnership::Borrowed));

  JS::RootedScript script(
      cx, JS::CompileForNonSyntacticScopeDontInflate(cx, options, srcBuf));
  CHECK(script);

  JS::RootedObject nsvo(cx, js::NewJSMEnvironment(cx));
  CHECK(nsvo);
  CHECK(js::ExecuteInJSMEnvironment(cx, script, nsvo));

  JS::RootedValue v(cx);
  CHECK(JS_GetProperty(cx, nsvo, "output", &v) && v == JS::ObjectValue(*nsvo));

  return true;
}
END_TEST(testExecuteInJSMEnvironment_Callback)
