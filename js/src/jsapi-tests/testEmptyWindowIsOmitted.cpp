/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"

#include <cstring>

#include "jsfriendapi.h"

#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/Exception.h"
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"
#include "vm/ErrorReporting.h"

using mozilla::ArrayLength;
using mozilla::IsAsciiHexDigit;
using mozilla::Utf8Unit;

BEGIN_TEST(testEmptyWindow) { return testUtf8() && testUtf16(); }

bool testUtf8() {
  // Bad unit with nothing before it.
  static const char badLeadingUnit[] = "\x80";
  CHECK(testOmittedWindow(badLeadingUnit, JSMSG_BAD_LEADING_UTF8_UNIT, "0x80"));

  // Bad unit at start of a fresh line.
  static const char badStartingFreshLine[] = "var x = 5;\n\x98";
  CHECK(testOmittedWindow(badStartingFreshLine, JSMSG_BAD_LEADING_UTF8_UNIT,
                          "0x98"));

  // Bad trailing unit in initial code point.
  static const char badTrailingUnit[] = "\xD8\x20";
  CHECK(testOmittedWindow(badTrailingUnit, JSMSG_BAD_TRAILING_UTF8_UNIT,
                          "0xD8 0x20"));

  // Bad trailing unit at start of a fresh line.
  static const char badTrailingUnitFreshLine[] = "var x = 5;\n\xD8\x20";
  CHECK(testOmittedWindow(badTrailingUnitFreshLine,
                          JSMSG_BAD_TRAILING_UTF8_UNIT, "0xD8 0x20"));

  // Overlong in initial code point.
  static const char overlongInitial[] = "\xC0\x80";
  CHECK(testOmittedWindow(overlongInitial, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
                          "0xC0 0x80"));

  // Overlong at start of a fresh line.
  static const char overlongFreshLine[] = "var x = 5;\n\xC0\x81";
  CHECK(testOmittedWindow(overlongFreshLine, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
                          "0xC0 0x81"));

  // Not-enough in initial code point.
  static const char notEnoughInitial[] = "\xF0";
  CHECK(
      testOmittedWindow(notEnoughInitial, JSMSG_NOT_ENOUGH_CODE_UNITS, "0xF0"));

  // Not-enough at start of a fresh line.
  static const char notEnoughFreshLine[] = "var x = 5;\n\xF0";
  CHECK(testOmittedWindow(notEnoughFreshLine, JSMSG_NOT_ENOUGH_CODE_UNITS,
                          "0xF0"));

  return true;
}

bool testUtf16() {
  // Bad unit with nothing before it.
  static const char16_t badLeadingUnit[] = u"\xDFFF";
  CHECK(testOmittedWindow(badLeadingUnit, JSMSG_ILLEGAL_CHARACTER));

  // Bad unit at start of a fresh line.
  static const char16_t badStartingFreshLine[] = u"var x = 5;\n\xDFFF";
  CHECK(testOmittedWindow(badStartingFreshLine, JSMSG_ILLEGAL_CHARACTER));

  return true;
}

static bool startsWith(const char* str, const char* prefix) {
  return std::strncmp(prefix, str, strlen(prefix)) == 0;
}

static bool equals(const char* str, const char* expected) {
  return std::strcmp(str, expected) == 0;
}

JSScript* compile(const char16_t* chars, size_t len) {
  JS::SourceText<char16_t> source;
  MOZ_RELEASE_ASSERT(
      source.init(cx, chars, len, JS::SourceOwnership::Borrowed));

  JS::CompileOptions options(cx);
  return JS::Compile(cx, options, source);
}

JSScript* compile(const char* chars, size_t len) {
  JS::SourceText<Utf8Unit> source;
  MOZ_RELEASE_ASSERT(
      source.init(cx, chars, len, JS::SourceOwnership::Borrowed));

  JS::CompileOptions options(cx);
  return JS::Compile(cx, options, source);
}

template <typename CharT, size_t N>
bool testOmittedWindow(const CharT (&chars)[N], unsigned expectedErrorNumber,
                       const char* badCodeUnits = nullptr) {
  JS::Rooted<JSScript*> script(cx, compile(chars, N - 1));
  CHECK(!script);

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  js::ErrorReport report(cx);
  CHECK(report.init(cx, exnStack, js::ErrorReport::WithSideEffects));

  const auto* errorReport = report.report();

  CHECK(errorReport->errorNumber == expectedErrorNumber);

  if (const auto& notes = errorReport->notes) {
    CHECK(sizeof(CharT) == 1);
    CHECK(badCodeUnits != nullptr);

    auto iter = notes->begin();
    CHECK(iter != notes->end());

    const char* noteMessage = (*iter)->message().c_str();

    // The prefix ought always be the same.
    static const char expectedPrefix[] =
        "the code units comprising this invalid code point were: ";
    constexpr size_t expectedPrefixLen = ArrayLength(expectedPrefix) - 1;

    CHECK(startsWith(noteMessage, expectedPrefix));

    // The end of the prefix is the bad code units.
    CHECK(equals(noteMessage + expectedPrefixLen, badCodeUnits));

    ++iter;
    CHECK(iter == notes->end());
  } else {
    CHECK(sizeof(CharT) == 2);

    // UTF-16 encoding "errors" are not categorical errors, so the errors
    // are just of the invalid-character sort, without an accompanying note
    // spelling out a series of invalid code units.
    CHECK(!badCodeUnits);
  }

  CHECK(!errorReport->linebuf());

  return true;
}
END_TEST(testEmptyWindow)
