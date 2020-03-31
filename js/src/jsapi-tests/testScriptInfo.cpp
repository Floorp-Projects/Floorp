/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include "jsapi.h"

#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "jsapi-tests/tests.h"

static const char code[] =
    "xx = 1;       \n\
                   \n\
try {              \n\
	 debugger; \n\
                   \n\
	 xx += 1;  \n\
}                  \n\
catch (e)          \n\
{                  \n\
	 xx += 1;  \n\
}\n\
//@ sourceMappingURL=http://example.com/path/to/source-map.json";

BEGIN_TEST(testScriptInfo) {
  unsigned startLine = 1000;

  JS::CompileOptions options(cx);
  options.setFileAndLine(__FILE__, startLine);

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  CHECK(srcBuf.init(cx, code, mozilla::ArrayLength(code) - 1,
                    JS::SourceOwnership::Borrowed));

  JS::RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  CHECK(script);

  CHECK_EQUAL(JS_GetScriptBaseLineNumber(cx, script), startLine);
  CHECK(strcmp(JS_GetScriptFilename(script), __FILE__) == 0);

  return true;
}
static bool CharsMatch(const char16_t* p, const char* q) {
  while (*q) {
    if (*p++ != *q++) {
      return false;
    }
  }
  return true;
}
END_TEST(testScriptInfo)
