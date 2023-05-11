/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "mozilla/Utf8.h"

#include <string>

#include "js/CompileOptions.h"
#include "js/experimental/CompileScript.h"
#include "js/SourceText.h"
#include "js/Stack.h"
#include "jsapi-tests/tests.h"

using namespace JS;

BEGIN_FRONTEND_TEST(testFrontendContextCompileGlobalScriptToStencil) {
  JS::FrontendContext* fc = JS::NewFrontendContext();
  CHECK(fc);

  JS::CompileOptions options((JS::CompileOptions::ForFrontendContext()));
  JS::NativeStackLimit stackLimit = JS::NativeStackLimitMax;

  {
    const char source[] = "var a = 10;";

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(
        srcBuf.init(fc, source, strlen(source), JS::SourceOwnership::Borrowed));
    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> stencil = JS::CompileGlobalScriptToStencil(
        fc, options, stackLimit, srcBuf, compileStorage);
    CHECK(stencil);
    CHECK(compileStorage.hasInput());
  }

  {
    const char16_t source[] = u"var a = 10;";

    JS::SourceText<char16_t> srcBuf;
    CHECK(srcBuf.init(fc, source, std::char_traits<char16_t>::length(source),
                      JS::SourceOwnership::Borrowed));
    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> stencil = JS::CompileGlobalScriptToStencil(
        fc, options, stackLimit, srcBuf, compileStorage);
    CHECK(stencil);
    CHECK(compileStorage.hasInput());
  }

  JS::DestroyFrontendContext(fc);

  return true;
}

END_TEST(testFrontendContextCompileGlobalScriptToStencil)
