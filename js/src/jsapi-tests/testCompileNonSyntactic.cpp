/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"  // RefPtr
#include "mozilla/Utf8.h"    // mozilla::Utf8Unit

#include <string_view>

#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/CompileOptions.h"  // JS::CompileOptions, JS::InstantiateOptions
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::CompileToStencilOffThread, JS::FinishCompileToStencilOffThread, JS::InstantiateGlobalStencil

#include "js/SourceText.h"  // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"
#include "vm/HelperThreads.h"
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"

using namespace JS;
using js::AutoLockMonitor;

struct OffThreadTask {
  OffThreadTask() : monitor(js::mutexid::ShellOffThreadState), token(nullptr) {}

  OffThreadToken* waitUntilDone(JSContext* cx) {
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

  js::Monitor monitor MOZ_UNANNOTATED;
  OffThreadToken* token;
};

BEGIN_TEST(testCompileScript) {
  CHECK(testCompile(true));

  CHECK(testCompile(false));
  return true;
}

bool testCompile(bool nonSyntactic) {
  static constexpr std::string_view src = "42\n";
  static constexpr std::u16string_view src_16 = u"42\n";

  static_assert(src.length() == src_16.length(),
                "Source buffers must be same length");

  JS::CompileOptions options(cx);
  options.setNonSyntacticScope(nonSyntactic);

  JS::SourceText<char16_t> buf16;
  CHECK(buf16.init(cx, src_16.data(), src_16.length(),
                   JS::SourceOwnership::Borrowed));

  JS::SourceText<mozilla::Utf8Unit> buf8;
  CHECK(buf8.init(cx, src.data(), src.length(), JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx);

  script = Compile(cx, options, buf16);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  script = Compile(cx, options, buf8);
  CHECK(script);
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  {
    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(cx, src_16.data(), src_16.length(),
                      JS::SourceOwnership::Borrowed));

    script = Compile(cx, options, srcBuf);
    CHECK(script);
    CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);
  }

  options.forceAsync = true;
  OffThreadTask task;
  OffThreadToken* token;

  JS::SourceText<char16_t> srcBuf;
  RefPtr<JS::Stencil> stencil;
  CHECK(srcBuf.init(cx, src_16.data(), src_16.length(),
                    JS::SourceOwnership::Borrowed));

  CHECK(CompileToStencilOffThread(cx, options, srcBuf, task.OffThreadCallback,
                                  &task));
  CHECK(token = task.waitUntilDone(cx));
  CHECK(stencil = FinishOffThreadStencil(cx, token));
  InstantiateOptions instantiateOptions(options);
  CHECK(script = InstantiateGlobalStencil(cx, instantiateOptions, stencil));
  CHECK_EQUAL(script->hasNonSyntacticScope(), nonSyntactic);

  return true;
}
END_TEST(testCompileScript);
