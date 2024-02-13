/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"  // mozilla::Range
#include "mozilla/Span.h"   // mozilla::Span
#include "mozilla/Utf8.h"   // mozilla::ConvertUtf8toUtf16

#include "js/CharacterEncoding.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testUTF8_badUTF8) {
  static const char badUTF8[] = "...\xC0...";
  JSString* str = JS_NewStringCopyZ(cx, badUTF8);
  CHECK(str);
  char16_t ch;
  if (!JS_GetStringCharAt(cx, str, 3, &ch)) {
    return false;
  }
  CHECK(ch == 0x00C0);
  return true;
}
END_TEST(testUTF8_badUTF8)

BEGIN_TEST(testUTF8_bigUTF8) {
  static const char bigUTF8[] = "...\xFB\xBF\xBF\xBF\xBF...";
  JSString* str = JS_NewStringCopyZ(cx, bigUTF8);
  CHECK(str);
  char16_t ch;
  if (!JS_GetStringCharAt(cx, str, 3, &ch)) {
    return false;
  }
  CHECK(ch == 0x00FB);
  return true;
}
END_TEST(testUTF8_bigUTF8)

BEGIN_TEST(testUTF8_badSurrogate) {
  static const char16_t badSurrogate[] = {'A', 'B', 'C', 0xDEEE, 'D', 'E', 0};
  mozilla::Range<const char16_t> tbchars(badSurrogate, js_strlen(badSurrogate));
  JS::Latin1CharsZ latin1 = JS::LossyTwoByteCharsToNewLatin1CharsZ(cx, tbchars);
  CHECK(latin1);
  CHECK(latin1[3] == 0x00EE);
  return true;
}
END_TEST(testUTF8_badSurrogate)

BEGIN_TEST(testUTF8_LossyConversion) {
  // Maximal subparts of an ill-formed subsequence should be replaced with
  // single REPLACEMENT CHARACTER.

  // Input ends with partial sequence.
  // clang-format off
  const char* inputs1[] = {
    "\xC2",
    "\xDF",
    "\xE0",
    "\xE0\xA0",
    "\xF0",
    "\xF0\x90",
    "\xF0\x90\x80",
  };
  // clang-format on

  char16_t outputBuf[8];
  mozilla::Span output(outputBuf, 8);

  for (const char* input : inputs1) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 1);
    CHECK(utf16[0] == 0xFFFD);

    // Make sure the behavior matches to encoding_rs.
    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 1);
    CHECK(outputBuf[0] == 0xFFFD);
  }

  // Partial sequence followed by ASCII range.
  // clang-format off
  const char* inputs2[] = {
    "\xC2 ",
    "\xDF ",
    "\xE0 ",
    "\xE0\xA0 ",
    "\xF0 ",
    "\xF0\x90 ",
    "\xF0\x90\x80 ",
  };
  // clang-format on

  for (const char* input : inputs2) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 2);
    CHECK(utf16[0] == 0xFFFD);
    CHECK(utf16[1] == 0x20);

    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 2);
    CHECK(outputBuf[0] == 0xFFFD);
    CHECK(outputBuf[1] == 0x20);
  }

  // Partial sequence followed by other first code unit.
  // clang-format off
  const char* inputs3[] = {
    "\xC2\xC2\x80",
    "\xDF\xC2\x80",
    "\xE0\xC2\x80",
    "\xE0\xA0\xC2\x80",
    "\xF0\xC2\x80",
    "\xF0\x90\xC2\x80",
    "\xF0\x90\x80\xC2\x80",
  };
  // clang-format on

  for (const char* input : inputs3) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 2);
    CHECK(utf16[0] == 0xFFFD);
    CHECK(utf16[1] == 0x80);

    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 2);
    CHECK(outputBuf[0] == 0xFFFD);
    CHECK(outputBuf[1] == 0x80);
  }

  // Invalid second byte.
  // clang-format off
  const char* inputs4[] = {
    "\xE0\x9F\x80\x80",
    "\xED\xA0\x80\x80",
    "\xF0\x80\x80\x80",
    "\xF4\x90\x80\x80",
  };
  // clang-format on

  for (const char* input : inputs4) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 4);
    CHECK(utf16[0] == 0xFFFD);
    CHECK(utf16[1] == 0xFFFD);
    CHECK(utf16[2] == 0xFFFD);
    CHECK(utf16[3] == 0xFFFD);

    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 4);
    CHECK(outputBuf[0] == 0xFFFD);
    CHECK(outputBuf[1] == 0xFFFD);
    CHECK(outputBuf[2] == 0xFFFD);
    CHECK(outputBuf[3] == 0xFFFD);
  }

  // Invalid second byte, with not sufficient number of units.
  // clang-format off
  const char* inputs5[] = {
    "\xE0\x9F\x80",
    "\xED\xA0\x80",
    "\xF0\x80\x80",
    "\xF4\x90\x80",
  };
  const char* inputs6[] = {
    "\xE0\x9F",
    "\xED\xA0",
    "\xF0\x80",
    "\xF4\x90",
  };
  // clang-format on

  for (const char* input : inputs5) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 3);
    CHECK(utf16[0] == 0xFFFD);
    CHECK(utf16[1] == 0xFFFD);
    CHECK(utf16[2] == 0xFFFD);

    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 3);
    CHECK(outputBuf[0] == 0xFFFD);
    CHECK(outputBuf[1] == 0xFFFD);
    CHECK(outputBuf[2] == 0xFFFD);
  }

  for (const char* input : inputs6) {
    size_t len;
    JS::TwoByteCharsZ utf16 = JS::LossyUTF8CharsToNewTwoByteCharsZ(
        cx, JS::UTF8Chars(input, js_strlen(input)), &len,
        js::StringBufferArena);
    CHECK(utf16);
    CHECK(len == 2);
    CHECK(utf16[0] == 0xFFFD);
    CHECK(utf16[1] == 0xFFFD);

    len = mozilla::ConvertUtf8toUtf16(mozilla::Span(input, js_strlen(input)),
                                      output);
    CHECK(len == 2);
    CHECK(outputBuf[0] == 0xFFFD);
    CHECK(outputBuf[1] == 0xFFFD);
  }
  return true;
}
END_TEST(testUTF8_LossyConversion)
