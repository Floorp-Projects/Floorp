/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "mozilla/Utf8.h"

#include <string>

#include "frontend/FrontendContext.h"  // js::FrontendContext
#include "js/CompileOptions.h"
#include "js/experimental/CompileScript.h"
#include "js/SourceText.h"
#include "js/Stack.h"
#include "jsapi-tests/tests.h"
#include "util/NativeStack.h"  // js::GetNativeStackBase

using namespace JS;

BEGIN_FRONTEND_TEST(testFrontendContextCompileGlobalScriptToStencil) {
  JS::FrontendContext* fc = JS::NewFrontendContext();
  CHECK(fc);

  static constexpr JS::NativeStackSize stackSize = 128 * sizeof(size_t) * 1024;

  JS::SetNativeStackQuota(fc, stackSize);

#ifndef __wasi__
  CHECK(fc->stackLimit() ==
        JS::GetNativeStackLimit(js::GetNativeStackBase(), stackSize - 1));
#endif

  JS::PrefableCompileOptions prefableOptions;
  JS::CompileOptions options(prefableOptions);

  {
    const char source[] = "var a = 10;";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(
        srcBuf.init(fc, source, strlen(source), JS::SourceOwnership::Borrowed));
    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> stencil =
        JS::CompileGlobalScriptToStencil(fc, options, srcBuf, compileStorage);
    CHECK(stencil);
    CHECK(compileStorage.hasInput());
  }

  {
    const char16_t source[] = u"var a = 10;";

    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(fc, source, std::char_traits<char16_t>::length(source),
                      JS::SourceOwnership::Borrowed));
    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> stencil =
        JS::CompileGlobalScriptToStencil(fc, options, srcBuf, compileStorage);
    CHECK(stencil);
    CHECK(compileStorage.hasInput());
  }

  JS::DestroyFrontendContext(fc);

  return true;
}

END_TEST(testFrontendContextCompileGlobalScriptToStencil)
