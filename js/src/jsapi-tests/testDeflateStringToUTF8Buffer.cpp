/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

using namespace JS;

BEGIN_TEST(test_DeflateStringToUTF8Buffer)
{
    JSString* str;
    JSFlatString* flatStr;

    // DeflateStringToUTF8Buffer does not write a null terminator, so the byte
    // following the last byte written to the |actual| buffer should retain
    // the value it held before the call to DeflateStringToUTF8Buffer, which is
    // initialized to 0x1.

    char actual[100];
    mozilla::RangedPtr<char> range = mozilla::RangedPtr<char>(actual, 100);

    // Test with an ASCII string, which calls JSFlatString::latin1Chars
    // to retrieve the characters from the string and generates UTF-8 output
    // that is identical to the ASCII input.

    str = JS_NewStringCopyZ(cx, "Ohai"); // { 0x4F, 0x68, 0x61, 0x69 }
    MOZ_RELEASE_ASSERT(str);
    flatStr = JS_FlattenString(cx, str);

    {
        const char expected[] = { 0x4F, 0x68, 0x61, 0x69, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    }

    {
        size_t dstlen = 4;
        const char expected[] = { 0x4F, 0x68, 0x61, 0x69, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 4u);
    }

    {
        size_t numchars = 0;
        const char expected[] = { 0x4F, 0x68, 0x61, 0x69, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, nullptr, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 4;
        size_t numchars = 0;
        const char expected[] = { 0x4F, 0x68, 0x61, 0x69, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 4u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 3;
        size_t numchars = 0;
        const char expected[] = { 0x4F, 0x68, 0x61, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 3u);
        CHECK_EQUAL(numchars, 3u);
    }

    {
        size_t dstlen = 100;
        size_t numchars = 0;
        const char expected[] = { 0x4F, 0x68, 0x61, 0x69, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 4u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 0;
        size_t numchars = 0;
        const unsigned char expected[] = { 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 0u);
        CHECK_EQUAL(numchars, 0u);
    }

    // Test with a Latin-1 string, which calls JSFlatString::latin1Chars
    // like with the ASCII string but generates UTF-8 output that is different
    // from the ASCII input.

    str = JS_NewUCStringCopyZ(cx, MOZ_UTF16("\xD3\x68\xE3\xEF")); // u"Óhãï"
    MOZ_RELEASE_ASSERT(str);
    flatStr = JS_FlattenString(cx, str);

    {
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0xC3, 0xA3, 0xC3, 0xAF, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    }

    {
        size_t dstlen = 7;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0xC3, 0xA3, 0xC3, 0xAF, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
    }

    {
        size_t numchars = 0;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0xC3, 0xA3, 0xC3, 0xAF, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, nullptr, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 7;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0xC3, 0xA3, 0xC3, 0xAF, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        // Specify a destination buffer length of 3.  That's exactly enough
        // space to encode the first two characters, which takes three bytes.
        size_t dstlen = 3;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 3u);
        CHECK_EQUAL(numchars, 2u);
    }

    {
        // Specify a destination buffer length of 4.  That's only enough space
        // to encode the first two characters, which takes three bytes, because
        // the third character would take another two bytes.
        size_t dstlen = 4;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 3u);
        CHECK_EQUAL(numchars, 2u);
    }

    {
        size_t dstlen = 100;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xC3, 0x93, 0x68, 0xC3, 0xA3, 0xC3, 0xAF, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 0;
        size_t numchars = 0;
        const unsigned char expected[] = { 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 0u);
        CHECK_EQUAL(numchars, 0u);
    }

    // Test with a UTF-16 string, which calls JSFlatString::twoByteChars
    // to retrieve the characters from the string.

    str = JS_NewUCStringCopyZ(cx, MOZ_UTF16("\x038C\x0068\x0203\x0457")); // u"Όhȃї"
    MOZ_RELEASE_ASSERT(str);
    flatStr = JS_FlattenString(cx, str);

    {
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0xC8, 0x83, 0xD1, 0x97, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
    }

    {
        size_t dstlen = 7;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0xC8, 0x83, 0xD1, 0x97, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
    }

    {
        size_t numchars = 0;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0xC8, 0x83, 0xD1, 0x97, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, nullptr, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 7;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0xC8, 0x83, 0xD1, 0x97, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        // Specify a destination buffer length of 3.  That's exactly enough
        // space to encode the first two characters, which takes three bytes.
        size_t dstlen = 3;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 3u);
        CHECK_EQUAL(numchars, 2u);
    }

    {
        // Specify a destination buffer length of 4.  That's only enough space
        // to encode the first two characters, which takes three bytes, because
        // the third character would take another two bytes.
        size_t dstlen = 4;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 3u);
        CHECK_EQUAL(numchars, 2u);
    }

    {
        size_t dstlen = 100;
        size_t numchars = 0;
        const unsigned char expected[] = { 0xCE, 0x8C, 0x68, 0xC8, 0x83, 0xD1, 0x97, 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 7u);
        CHECK_EQUAL(numchars, 4u);
    }

    {
        size_t dstlen = 0;
        size_t numchars = 0;
        const unsigned char expected[] = { 0x1 };
        memset(actual, 0x1, 100);
        JS::DeflateStringToUTF8Buffer(flatStr, range, &dstlen, &numchars);
        CHECK_EQUAL(memcmp(actual, expected, sizeof(expected)), 0);
        CHECK_EQUAL(dstlen, 0u);
        CHECK_EQUAL(numchars, 0u);
    }

    return true;
}
END_TEST(test_DeflateStringToUTF8Buffer)
