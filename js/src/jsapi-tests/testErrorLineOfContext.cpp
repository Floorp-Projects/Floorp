/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CompilationAndEvaluation.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"
#include "vm/ErrorReporting.h"

BEGIN_TEST(testErrorLineOfContext) {
  static const char16_t fullLineR[] = u"\n  var x = @;  \r  ";
  CHECK(testLineOfContextHasNoLineTerminator(fullLineR, ' '));

  static const char16_t fullLineN[] = u"\n  var x = @; !\n  ";
  CHECK(testLineOfContextHasNoLineTerminator(fullLineN, '!'));

  static const char16_t fullLineLS[] = u"\n  var x = @; +\u2028  ";
  CHECK(testLineOfContextHasNoLineTerminator(fullLineLS, '+'));

  static const char16_t fullLinePS[] = u"\n  var x = @; #\u2029  ";
  CHECK(testLineOfContextHasNoLineTerminator(fullLinePS, '#'));

  static_assert(js::ErrorMetadata::lineOfContextRadius == 60,
                "current max count past offset is 60, hits 'X' below");

  static const char16_t truncatedLine[] =
      u"@ + 4567890123456789012345678901234567890123456789012345678XYZW\n";
  CHECK(testLineOfContextHasNoLineTerminator(truncatedLine, 'X'));

  return true;
}

bool eval(const char16_t* chars, size_t len, JS::MutableHandleValue rval) {
  JS::RealmOptions globalOptions;
  JS::RootedObject global(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(global);

  JSAutoRealm ar(cx, global);

  JS::SourceText<char16_t> srcBuf;
  CHECK(srcBuf.init(cx, chars, len, JS::SourceOwnership::Borrowed));

  JS::CompileOptions options(cx);
  return JS::Evaluate(cx, options, srcBuf, rval);
}

template <size_t N>
bool testLineOfContextHasNoLineTerminator(const char16_t (&chars)[N],
                                          char16_t expectedLast) {
  JS::RootedValue rval(cx);
  CHECK(!eval(chars, N - 1, &rval));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  JS::ErrorReportBuilder report(cx);
  CHECK(report.init(cx, exnStack, JS::ErrorReportBuilder::WithSideEffects));

  const auto* errorReport = report.report();

  const char16_t* lineOfContext = errorReport->linebuf();
  size_t lineOfContextLength = errorReport->linebufLength();

  CHECK(lineOfContext[lineOfContextLength] == '\0');
  char16_t last = lineOfContext[lineOfContextLength - 1];
  CHECK(last == expectedLast);

  return true;
}
END_TEST(testErrorLineOfContext)
