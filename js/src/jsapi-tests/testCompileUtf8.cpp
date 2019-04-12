/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"

#include <cstring>

#include "jsfriendapi.h"

#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"  // JS::CompileDontInflate
#include "js/SourceText.h"
#include "jsapi-tests/tests.h"
#include "vm/ErrorReporting.h"

using mozilla::ArrayEqual;
using mozilla::ArrayLength;
using mozilla::IsAsciiHexDigit;
using mozilla::Utf8Unit;

BEGIN_TEST(testUtf8BadBytes) {
  static const char badLeadingUnit[] = "var x = \x80";
  CHECK(testBadUtf8(
      badLeadingUnit, JSMSG_BAD_LEADING_UTF8_UNIT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(startsWith(chars, "0x80"));
        CHECK(isBadLeadUnitMessage(chars));
        return true;
      },
      "0x80"));

  static const char badSecondInTwoByte[] = "var x = \xDF\x20";
  CHECK(testBadUtf8(
      badSecondInTwoByte, JSMSG_BAD_TRAILING_UTF8_UNIT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isBadTrailingBytesMessage(chars));
        CHECK(contains(chars, "0x20"));
        return true;
      },
      "0xDF 0x20"));

  static const char badSecondInThreeByte[] = "var x = \xEF\x17\xA7";
  CHECK(testBadUtf8(
      badSecondInThreeByte, JSMSG_BAD_TRAILING_UTF8_UNIT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isBadTrailingBytesMessage(chars));
        CHECK(contains(chars, "0x17"));
        return true;
      },
      // Validating stops with the first invalid code unit and
      // shouldn't go beyond that.
      "0xEF 0x17"));

  static const char lengthTwoTooShort[] = "var x = \xDF";
  CHECK(testBadUtf8(
      lengthTwoTooShort, JSMSG_NOT_ENOUGH_CODE_UNITS,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isNotEnoughUnitsMessage(chars));
        CHECK(contains(chars, "0xDF"));
        CHECK(contains(chars, " 1 byte, but 0 bytes were present"));
        return true;
      },
      "0xDF"));

  static const char forbiddenHighSurrogate[] = "var x = \xED\xA2\x87";
  CHECK(testBadUtf8(
      forbiddenHighSurrogate, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isSurrogateMessage(chars));
        CHECK(contains(chars, "0xD887"));
        return true;
      },
      "0xED 0xA2 0x87"));

  static const char forbiddenLowSurrogate[] = "var x = \xED\xB7\xAF";
  CHECK(testBadUtf8(
      forbiddenLowSurrogate, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isSurrogateMessage(chars));
        CHECK(contains(chars, "0xDDEF"));
        return true;
      },
      "0xED 0xB7 0xAF"));

  static const char oneTooBig[] = "var x = \xF4\x90\x80\x80";
  CHECK(testBadUtf8(
      oneTooBig, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isTooBigMessage(chars));
        CHECK(contains(chars, "0x110000"));
        return true;
      },
      "0xF4 0x90 0x80 0x80"));

  static const char notShortestFormZero[] = "var x = \xC0\x80";
  CHECK(testBadUtf8(
      notShortestFormZero, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isNotShortestFormMessage(chars));
        CHECK(startsWith(chars, "0x0 isn't "));
        return true;
      },
      "0xC0 0x80"));

  static const char notShortestFormNonzero[] = "var x = \xE0\x87\x80";
  CHECK(testBadUtf8(
      notShortestFormNonzero, JSMSG_FORBIDDEN_UTF8_CODE_POINT,
      [this](JS::ConstUTF8CharsZ message) {
        const char* chars = message.c_str();
        CHECK(isNotShortestFormMessage(chars));
        CHECK(startsWith(chars, "0x1C0 isn't "));
        return true;
      },
      "0xE0 0x87 0x80"));

  return true;
}

static constexpr size_t LengthOfByte = ArrayLength("0xFF") - 1;

static bool startsWithByte(const char* str) {
  return str[0] == '0' && str[1] == 'x' && IsAsciiHexDigit(str[2]) &&
         IsAsciiHexDigit(str[3]);
}

static bool startsWith(const char* str, const char* prefix) {
  return std::strncmp(prefix, str, strlen(prefix)) == 0;
}

static bool contains(const char* str, const char* substr) {
  return std::strstr(str, substr) != nullptr;
}

static bool equals(const char* str, const char* expected) {
  return std::strcmp(str, expected) == 0;
}

static bool isBadLeadUnitMessage(const char* str) {
  return startsWithByte(str) &&
         equals(str + LengthOfByte,
                " byte doesn't begin a valid UTF-8 code point");
}

static bool isBadTrailingBytesMessage(const char* str) {
  return startsWith(str, "bad trailing UTF-8 byte ");
}

static bool isNotEnoughUnitsMessage(const char* str) {
  return startsWithByte(str) &&
         startsWith(str + LengthOfByte, " byte in UTF-8 must be followed by ");
}

static bool isForbiddenCodePointMessage(const char* str) {
  return contains(str, "isn't a valid code point because");
}

static bool isSurrogateMessage(const char* str) {
  return isForbiddenCodePointMessage(str) &&
         contains(str, " it's a UTF-16 surrogate");
}

static bool isTooBigMessage(const char* str) {
  return isForbiddenCodePointMessage(str) &&
         contains(str, "the maximum code point is U+10FFFF");
}

static bool isNotShortestFormMessage(const char* str) {
  return isForbiddenCodePointMessage(str) &&
         contains(str, "it wasn't encoded in shortest possible form");
}

template <size_t N, typename TestMessage>
bool testBadUtf8(const char (&chars)[N], unsigned errorNumber,
                 TestMessage testMessage, const char* badBytes) {
  JS::Rooted<JSScript*> script(cx);
  {
    JS::CompileOptions options(cx);

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, chars, N - 1, JS::SourceOwnership::Borrowed));

    script = JS::CompileDontInflate(cx, options, srcBuf);
    CHECK(!script);
  }

  JS::RootedValue exn(cx);
  CHECK(JS_GetPendingException(cx, &exn));
  JS_ClearPendingException(cx);

  js::ErrorReport report(cx);
  CHECK(report.init(cx, exn, js::ErrorReport::WithSideEffects));

  const auto* errorReport = report.report();

  CHECK(errorReport->errorNumber == errorNumber);

  CHECK(testMessage(errorReport->message()));

  {
    const auto& notes = errorReport->notes;
    CHECK(notes != nullptr);

    auto iter = notes->begin();
    CHECK(iter != notes->end());

    const char* noteMessage = (*iter)->message().c_str();

    // The prefix ought always be the same.
    static const char expectedPrefix[] =
        "the code units comprising this invalid code point were: ";
    constexpr size_t expectedPrefixLen = ArrayLength(expectedPrefix) - 1;

    CHECK(startsWith(noteMessage, expectedPrefix));

    // The end of the prefix is the bad bytes.
    CHECK(equals(noteMessage + expectedPrefixLen, badBytes));

    ++iter;
    CHECK(iter == notes->end());
  }

  static const char16_t expectedContext[] = u"var x = ";
  constexpr size_t expectedContextLen = ArrayLength(expectedContext) - 1;

  const char16_t* lineOfContext = errorReport->linebuf();
  size_t lineOfContextLength = errorReport->linebufLength();

  CHECK(lineOfContext[lineOfContextLength] == '\0');
  CHECK(lineOfContextLength == expectedContextLen);

  CHECK(std::memcmp(lineOfContext, expectedContext,
                    expectedContextLen * sizeof(char16_t)) == 0);

  return true;
}
END_TEST(testUtf8BadBytes)

BEGIN_TEST(testMultiUnitUtf8InWindow) {
  static const char firstInWindowIsMultiUnit[] =
      "\xCF\x80\xCF\x80 = 6.283185307; @ bad starts HERE:\x80\xFF\xFF";
  CHECK(testContext(firstInWindowIsMultiUnit,
                    u"ππ = 6.283185307; @ bad starts HERE:"));

  static const char atTokenOffsetIsMulti[] = "var z = 💯";
  CHECK(testContext(atTokenOffsetIsMulti, u"var z = 💯"));

  static const char afterTokenOffsetIsMulti[] = "var z = @💯💯💯X";
  CHECK(testContext(afterTokenOffsetIsMulti, u"var z = @💯💯💯X"));

  static const char atEndIsMulti[] = "var z = @@💯💯💯";
  CHECK(testContext(atEndIsMulti, u"var z = @@💯💯💯"));

  return true;
}

template <size_t N, size_t ContextLenWithNull>
bool testContext(const char (&chars)[N],
                 const char16_t (&expectedContext)[ContextLenWithNull]) {
  JS::Rooted<JSScript*> script(cx);
  {
    JS::CompileOptions options(cx);

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, chars, N - 1, JS::SourceOwnership::Borrowed));

    script = JS::CompileDontInflate(cx, options, srcBuf);
    CHECK(!script);
  }

  JS::RootedValue exn(cx);
  CHECK(JS_GetPendingException(cx, &exn));
  JS_ClearPendingException(cx);

  js::ErrorReport report(cx);
  CHECK(report.init(cx, exn, js::ErrorReport::WithSideEffects));

  const auto* errorReport = report.report();

  CHECK(errorReport->errorNumber == JSMSG_ILLEGAL_CHARACTER);

  const char16_t* lineOfContext = errorReport->linebuf();
  size_t lineOfContextLength = errorReport->linebufLength();

  CHECK(lineOfContext[lineOfContextLength] == '\0');
  CHECK(lineOfContextLength == ContextLenWithNull - 1);

  CHECK(ArrayEqual(lineOfContext, expectedContext, ContextLenWithNull));

  return true;
}
END_TEST(testMultiUnitUtf8InWindow)
