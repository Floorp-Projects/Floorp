/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using namespace JS;

BEGIN_TEST(test_DeflateStringToUTF8Buffer) {
  JSString* str;
  JSLinearString* linearStr;

  // DeflateStringToUTF8Buffer does not write a null terminator, so the byte
  // following the last byte written to the |actual| buffer should retain
  // the value it held before the call to DeflateStringToUTF8Buffer, which is
  // initialized to 0x1.

  char actual[100];
  auto span = mozilla::Span(actual);

  // Test with an ASCII string, which calls JSLinearString::latin1Chars
  // to retrieve the characters from the string and generates UTF-8 output
  // that is identical to the ASCII input.

  str = JS_NewStringCopyZ(cx, "Ohai");  // { 0x4F, 0x68, 0x61, 0x69 }
  MOZ_RELEASE_ASSERT(str);
  linearStr = JS_EnsureLinearString(cx, str);

  {
    const char expected[] = {0x4F, 0x68, 0x61, 0x69, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span);
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 4u);
  }

  {
    const char expected[] = {0x4F, 0x68, 0x61, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(3));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 3u);
  }

  {
    const unsigned char expected[] = {0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(0));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 0u);
  }

  // Test with a Latin-1 string, which calls JSLinearString::latin1Chars
  // like with the ASCII string but generates UTF-8 output that is different
  // from the ASCII input.

  str = JS_NewUCStringCopyZ(cx, u"\xD3\x68\xE3\xEF");  // u"Óhãï"
  MOZ_RELEASE_ASSERT(str);
  linearStr = JS_EnsureLinearString(cx, str);

  {
    const unsigned char expected[] = {0xC3, 0x93, 0x68, 0xC3,
                                      0xA3, 0xC3, 0xAF, 0x1};
    memset(actual, 0x1, 100);
    JS::DeflateStringToUTF8Buffer(linearStr, span);
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
  }

  {
    const unsigned char expected[] = {0xC3, 0x93, 0x68, 0xC3,
                                      0xA3, 0xC3, 0xAF, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(7));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 7u);
  }

  {
    // Specify a destination buffer length of 3.  That's exactly enough
    // space to encode the first two characters, which takes three bytes.
    const unsigned char expected[] = {0xC3, 0x93, 0x68, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(3));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 3u);
  }

  {
    // Specify a destination buffer length of 4.  That's only enough space
    // to encode the first two characters, which takes three bytes, because
    // the third character would take another two bytes.
    const unsigned char expected[] = {0xC3, 0x93, 0x68, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(4));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 3u);
  }

  {
    const unsigned char expected[] = {0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(0));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 0u);
  }

  // Test with a UTF-16 string, which calls JSLinearString::twoByteChars
  // to retrieve the characters from the string.

  str = JS_NewUCStringCopyZ(cx, u"\x038C\x0068\x0203\x0457");  // u"Όhȃї"
  MOZ_RELEASE_ASSERT(str);
  linearStr = JS_EnsureLinearString(cx, str);

  {
    const unsigned char expected[] = {0xCE, 0x8C, 0x68, 0xC8,
                                      0x83, 0xD1, 0x97, 0x1};
    memset(actual, 0x1, 100);
    JS::DeflateStringToUTF8Buffer(linearStr, span);
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
  }

  {
    const unsigned char expected[] = {0xCE, 0x8C, 0x68, 0xC8,
                                      0x83, 0xD1, 0x97, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(7));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 7u);
  }

  {
    // Specify a destination buffer length of 3.  That's exactly enough
    // space to encode the first two characters, which takes three bytes.
    const unsigned char expected[] = {0xCE, 0x8C, 0x68, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(3));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 3u);
  }

  {
    // Specify a destination buffer length of 4.  That's only enough space
    // to encode the first two characters, which takes three bytes, because
    // the third character would take another two bytes.
    const unsigned char expected[] = {0xCE, 0x8C, 0x68, 0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(4));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 3u);
  }

  {
    const unsigned char expected[] = {0x1};
    memset(actual, 0x1, 100);
    size_t dstlen = JS::DeflateStringToUTF8Buffer(linearStr, span.To(0));
    CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    CHECK_EQUAL(dstlen, 0u);
  }

  return true;
}
END_TEST(test_DeflateStringToUTF8Buffer)
