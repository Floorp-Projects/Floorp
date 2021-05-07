/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "jit/Ion.h"                      // js::jit::IsIonEnabled
#include "js/CompilationAndEvaluation.h"  // JS::CompileFunction
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "util/Text.h"

#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"

using namespace JS;

static void ScriptCallback(JSRuntime* rt, void* data, js::BaseScript* script,
                           const JS::AutoRequireNoGC& nogc) {
  unsigned& count = *static_cast<unsigned*>(data);
  if (script->asJSScript()->hasIonScript()) {
    ++count;
  }
}

BEGIN_TEST(test_PreserveJitCode) {
  CHECK(testPreserveJitCode(false, 0));
  CHECK(testPreserveJitCode(true, 1));
  return true;
}

unsigned countIonScripts(JSObject* global) {
  unsigned count = 0;
  js::IterateScripts(cx, global->nonCCWRealm(), &count, ScriptCallback);
  return count;
}

bool testPreserveJitCode(bool preserveJitCode, unsigned remainingIonScripts) {
  cx->runtime()->setOffthreadIonCompilationEnabled(false);

  RootedObject global(cx, createTestGlobal(preserveJitCode));
  CHECK(global);
  JSAutoRealm ar(cx, global);

  // The Ion JIT may be unavailable due to --disable-jit or lack of support
  // for this platform.
  if (!js::jit::IsIonEnabled(cx)) {
    knownFail = true;
  }

  CHECK_EQUAL(countIonScripts(global), 0u);

  static constexpr char source[] =
      "var i = 0;\n"
      "var sum = 0;\n"
      "while (i < 10) {\n"
      "    sum += i;\n"
      "    ++i;\n"
      "}\n"
      "return sum;\n";
  constexpr unsigned length = js_strlen(source);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, source, length, JS::SourceOwnership::Borrowed));

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, 1);

  JS::RootedFunction fun(cx);
  JS::RootedObjectVector emptyScopeChain(cx);
  fun = JS::CompileFunction(cx, emptyScopeChain, options, "f", 0, nullptr,
                            srcBuf);
  CHECK(fun);

  RootedValue value(cx);
  for (unsigned i = 0; i < 1500; ++i) {
    CHECK(JS_CallFunction(cx, global, fun, JS::HandleValueArray::empty(),
                          &value));
  }
  CHECK_EQUAL(value.toInt32(), 45);
  CHECK_EQUAL(countIonScripts(global), 1u);

  NonIncrementalGC(cx, JS::GCOptions::Normal, GCReason::API);
  CHECK_EQUAL(countIonScripts(global), remainingIonScripts);

  NonIncrementalGC(cx, JS::GCOptions::Shrink, GCReason::API);
  CHECK_EQUAL(countIonScripts(global), 0u);

  return true;
}

JSObject* createTestGlobal(bool preserveJitCode) {
  JS::RealmOptions options;
  options.creationOptions().setPreserveJitCode(preserveJitCode);
  return JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                            JS::FireOnNewGlobalHook, options);
}
END_TEST(test_PreserveJitCode)
