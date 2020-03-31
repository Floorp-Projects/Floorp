/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CompilationAndEvaluation.h"  // JS::Compile{,ForNonSyntacticScope{,DontInflate}}
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "vm/HelperThreads.h"
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"

using namespace JS;
using js::AutoLockMonitor;

struct OffThreadTask {
  OffThreadTask() : monitor(js::mutexid::ShellOffThreadState), token(nullptr) {}

  OffThreadToken* waitUntilDone(JSContext* cx) {
    if (js::OffThreadParsingMustWaitForGC(cx->runtime())) {
      js::gc::FinishGC(cx);
    }

    AutoLockMonitor alm(monitor);
    while (!token) {
      alm.wait();
    }
    OffThreadToken* result = token;
    token = nullptr;
    return result;
  }

  void markDone(JS::OffThreadToken* tokenArg) {
    AutoLockMonitor alm(monitor);
    token = tokenArg;
    alm.notify();
  }

  static void OffThreadCallback(OffThreadToken* token, void* context) {
    auto self = static_cast<OffThreadTask*>(context);
    self->markDone(token);
  }

  js::Monitor monitor;
  OffThreadToken* token;
};

BEGIN_TEST(testCompileScript) {
  CHECK(testCompile(true));

  CHECK(testCompile(false));
  return true;
}

bool testCompile(bool nonSyntactic) {
  static const char src[] = "42\n";
  static const char16_t src_16[] = u"42\n";

  constexpr size_t length = sizeof(src) - 1;
  static_assert(sizeof(src_16) / sizeof(*src_16) - 1 == length,
                "Source buffers must be same length");

  JS::CompileOptions options(cx);
  options.setNonSyntacticScope(nonSyntactic);

  JS::SourceText<char16_t> buf16;
  CHECK(buf16.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx);

  // Check explicit non-syntactic compilation first to make sure it doesn't
  // modify our options object.
  script = CompileForNonSyntacticScope(cx, options, buf16);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), true);

  JS::SourceText<mozilla::Utf8Unit> buf8;
  CHECK(buf8.init(cx, src, length, JS::SourceOwnership::Borrowed));

  script = CompileForNonSyntacticScopeDontInflate(cx, options, buf8);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), true);

  {
    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

    script = CompileForNonSyntacticScope(cx, options, srcBuf);
    CHECK(script);
    CHECK_EQUAL(script->hasNonSyntacticScope(), true);
  }

  script = Compile(cx, options, buf16);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  script = Compile(cx, options, buf8);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  {
    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

    script = Compile(cx, options, srcBuf);
    CHECK(script);
    CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);
  }

  options.forceAsync = true;
  OffThreadTask task;
  OffThreadToken* token;

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, src_16, length, JS::SourceOwnership::Borrowed));

  CHECK(CompileOffThread(cx, options, srcBuf, task.OffThreadCallback, &task));
  CHECK(token = task.waitUntilDone(cx));
  CHECK(script = FinishOffThreadScript(cx, token));
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  return true;
}
END_TEST(testCompileScript);
