/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CharacterEncoding.h"  // JS::FindSmallestEncoding, JS::SmallestEncoding
#include "js/GCAPI.h"  // JSExternalStringCallbacks, JS_NewExternalUCString, JS_NewExternalStringLatin1, JS_NewMaybeExternalStringUTF8, JS::AutoRequireNoGC
#include "js/String.h"  // JS::IsExternalStringLatin1
#include "jsapi-tests/tests.h"
#include "util/Text.h"

static const char16_t arr[] = u"hi, don't delete me";
static const size_t arrlen = js_strlen(arr);

static const char arr2[] = "hi, don't delete me";
static const size_t arrlen2 = js_strlen(arr2);

static int finalized1 = 0;
static int finalized2 = 0;
static int finalized3 = 0;
static int finalized4 = 0;

struct ExternalStringCallbacks : public JSExternalStringCallbacks {
  int* finalizedCount = nullptr;
  bool isTwoBytes = false;

  explicit ExternalStringCallbacks(int* finalizedCount, bool isTwoBytes)
      : finalizedCount(finalizedCount), isTwoBytes(isTwoBytes) {}

  void finalize(JS::Latin1Char* chars) const override {
    MOZ_ASSERT(!isTwoBytes);
    MOZ_ASSERT(chars == (JS::Latin1Char*)arr2);
    (*finalizedCount)++;
  }

  void finalize(char16_t* chars) const override {
    MOZ_ASSERT(isTwoBytes);
    MOZ_ASSERT(chars == arr);
    (*finalizedCount)++;
  }

  size_t sizeOfBuffer(const JS::Latin1Char* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }

  size_t sizeOfBuffer(const char16_t* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }
};

static const ExternalStringCallbacks callbacks1(&finalized1, true);
static const ExternalStringCallbacks callbacks2(&finalized2, true);
static const ExternalStringCallbacks callbacks3(&finalized3, false);
static const ExternalStringCallbacks callbacks4(&finalized4, false);

BEGIN_TEST(testExternalStrings) {
  const unsigned N = 1000;

  for (unsigned i = 0; i < N; ++i) {
    CHECK(JS_NewExternalUCString(cx, arr, arrlen, &callbacks1));
    CHECK(JS_NewExternalUCString(cx, arr, arrlen, &callbacks2));
    CHECK(JS_NewExternalStringLatin1(cx, (JS::Latin1Char*)arr2, arrlen2,
                                     &callbacks3));
    CHECK(JS_NewExternalStringLatin1(cx, (JS::Latin1Char*)arr2, arrlen2,
                                     &callbacks4));
  }

  JS_GC(cx);

  CHECK((N - finalized1) == 0);
  CHECK((N - finalized2) == 0);
  CHECK((N - finalized3) == 0);
  CHECK((N - finalized4) == 0);

  return true;
}
END_TEST(testExternalStrings)

struct SimpleExternalStringCallbacks : public JSExternalStringCallbacks {
  SimpleExternalStringCallbacks() = default;

  void finalize(JS::Latin1Char* chars) const override {}

  void finalize(char16_t* chars) const override {
    MOZ_CRASH("Unexpected call");
  }

  size_t sizeOfBuffer(const JS::Latin1Char* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }

  size_t sizeOfBuffer(const char16_t* chars,
                      mozilla::MallocSizeOf mallocSizeOf) const override {
    MOZ_CRASH("Unexpected call");
  }
};

static const SimpleExternalStringCallbacks simpleCallback;

static const char utf8ASCII[] = "hi, I'm UTF-8 string";
static const size_t utf8ASCIILen = js_strlen(utf8ASCII);

static const char utf8Latin1[] = "hi, I'm \xC3\x9CTF-8 string";
static const size_t utf8Latin1Len = js_strlen(utf8Latin1);

static const char latin1[] = "hi, I'm \xDCTF-8 string";
static const size_t latin1Len = js_strlen(latin1);

static const char utf8UTF16[] = "hi, I'm UTF-\xEF\xBC\x98 string";
static const size_t utf8UTF16Len = js_strlen(utf8UTF16);

static const char16_t utf16[] = u"hi, I'm UTF-８ string";
static const size_t utf16Len = js_strlen(utf16);

BEGIN_TEST(testExternalStringsUTF8) {
  // UTF-8 chars with ASCII range content should be converted into external
  // string.
  {
    JS::UTF8Chars utf8Chars(utf8ASCII, utf8ASCIILen);
    CHECK(JS::FindSmallestEncoding(utf8Chars) == JS::SmallestEncoding::ASCII);
    bool allocatedExternal = false;
    JS::Rooted<JSString*> str(
        cx, JS_NewMaybeExternalStringUTF8(cx, utf8Chars, &simpleCallback,
                                          &allocatedExternal));
    CHECK(str);
    CHECK(allocatedExternal);

    const JSExternalStringCallbacks* callbacks = nullptr;
    const JS::Latin1Char* chars = nullptr;
    CHECK(JS::IsExternalStringLatin1(str, &callbacks, &chars));
    CHECK(callbacks == &simpleCallback);
    CHECK((void*)chars == (void*)utf8ASCII);

    CHECK(StringHasLatin1Chars(str));

    JS::AutoAssertNoGC nogc(cx);
    size_t length;
    chars = JS_GetLatin1StringCharsAndLength(cx, nogc, str, &length);
    CHECK(length == utf8ASCIILen);
    CHECK(memcmp(chars, utf8ASCII, length) == 0);
  }

  // UTF-8 chars with latin-1 range content shouldn't be converted into external
  // string, but regular latin-1 string.
  {
    JS::UTF8Chars utf8Chars(utf8Latin1, utf8Latin1Len);
    CHECK(JS::FindSmallestEncoding(utf8Chars) == JS::SmallestEncoding::Latin1);
    bool allocatedExternal = false;
    JS::Rooted<JSString*> str(
        cx, JS_NewMaybeExternalStringUTF8(cx, utf8Chars, &simpleCallback,
                                          &allocatedExternal));
    CHECK(str);
    CHECK(!allocatedExternal);

    const JSExternalStringCallbacks* callbacks = nullptr;
    const JS::Latin1Char* chars = nullptr;
    CHECK(!JS::IsExternalStringLatin1(str, &callbacks, &chars));
    CHECK(!callbacks);
    CHECK(!chars);

    CHECK(StringHasLatin1Chars(str));

    JS::AutoAssertNoGC nogc(cx);
    size_t length;
    chars = JS_GetLatin1StringCharsAndLength(cx, nogc, str, &length);
    CHECK(length == latin1Len);
    CHECK(memcmp(chars, latin1, length) == 0);
  }

  // UTF-8 chars with UTF-16 range content shouldn't be converted into external
  // string, but regular TwoBytes string.
  {
    JS::UTF8Chars utf8Chars(utf8UTF16, utf8UTF16Len);
    CHECK(JS::FindSmallestEncoding(utf8Chars) == JS::SmallestEncoding::UTF16);
    bool allocatedExternal = false;
    JS::Rooted<JSString*> str(
        cx, JS_NewMaybeExternalStringUTF8(cx, utf8Chars, &simpleCallback,
                                          &allocatedExternal));
    CHECK(str);
    CHECK(!allocatedExternal);

    const JSExternalStringCallbacks* callbacks = nullptr;
    const JS::Latin1Char* chars = nullptr;
    CHECK(!JS::IsExternalStringLatin1(str, &callbacks, &chars));
    CHECK(!callbacks);
    CHECK(!chars);

    CHECK(!StringHasLatin1Chars(str));

    JS::AutoAssertNoGC nogc(cx);
    size_t length;
    const char16_t* chars16 = nullptr;
    chars16 = JS_GetTwoByteStringCharsAndLength(cx, nogc, str, &length);
    CHECK(length == utf16Len);
    CHECK(memcmp(chars16, utf16, length * sizeof(char16_t)) == 0);
  }

  return true;
}
END_TEST(testExternalStringsUTF8)
