/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextUtils.h"

#include <clocale>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <iterator>
#include <string_view>

#include "js/CharacterEncoding.h"
#include "jsapi-tests/tests.h"

static bool EqualsIgnoreCase(const char* xs, const char* ys) {
  while (*xs && *ys) {
    char x = *xs++;
    char y = *ys++;

    // Convert both to lower-case.
    if (mozilla::IsAsciiAlpha(x) && mozilla::IsAsciiAlpha(y)) {
      x |= 0x20;
      y |= 0x20;
    }

    // Fail if the characters aren't the same.
    if (x != y) {
      return false;
    }
  }

  // Both strings must be read to the end.
  return !*xs && !*ys;
}

class ToUTF8Locale {
  const char* previousLocale_ = nullptr;
  bool supported_ = false;

 public:
  ToUTF8Locale() {
    // Store the old locale so we can reset it in the destructor.
    previousLocale_ = std::setlocale(LC_ALL, nullptr);

    // Query the system default locale.
    const char* defaultLocale = std::setlocale(LC_ALL, "");
    if (!defaultLocale) {
      // std::setlocale returns nullptr on failure.
      return;
    }

    // Switch the default locale to be UTF-8 aware.
    const char* newLocale = std::setlocale(LC_ALL, "en_US.UTF-8");
    if (!newLocale) {
      // std::setlocale returns nullptr on failure.
      return;
    }

    const char* defaultCodepage = std::strchr(defaultLocale, '.');
    const char* newCodepage = std::strchr(newLocale, '.');

    // Return if either the default or new locale don't contain a code-page.
    if (!defaultCodepage || !newCodepage) {
      return;
    }

    // Skip past the '.'.
    defaultCodepage++;
    newCodepage++;

    // UTF-8 is supported when the default locale and new locale support it:
    //
    // The default locale needs to support UTF-8, because this test is compiled
    // using the default locale.
    //
    // The new locale needs to support UTF-8 to ensure UTF-8 encoding works at
    // runtime.
    supported_ = EqualsIgnoreCase(defaultCodepage, "UTF-8") &&
                 EqualsIgnoreCase(newCodepage, "UTF-8");
  }

  bool supported() const { return supported_; }

  ~ToUTF8Locale() {
    // Restore the previous locale.
    if (previousLocale_) {
      std::setlocale(LC_ALL, previousLocale_);
    }
  }
};

BEGIN_TEST(testCharacterEncoding_narrow_to_utf8) {
  // Assume the narrow charset is ASCII-compatible. ASCII to UTF-8 conversion is
  // a no-op.
  for (std::string_view string : {
           "",
           "a",
           "abc",
           "abc\0def",
       }) {
    auto utf8 = JS::EncodeNarrowToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);
    CHECK_EQUAL(std::strlen(utf8.get()), string.length());
    CHECK(utf8.get() == string);
  }
  return true;
}
END_TEST(testCharacterEncoding_narrow_to_utf8)

BEGIN_TEST(testCharacterEncoding_wide_to_utf8) {
  // Assume the wide charset is ASCII-compatible. ASCII to UTF-8 conversion is
  // a no-op.
  for (std::wstring_view string : {
           L"",
           L"a",
           L"abc",
           L"abc\0def",
       }) {
    auto utf8 = JS::EncodeWideToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);
    CHECK_EQUAL(std::strlen(utf8.get()), string.length());
    CHECK(std::equal(
        string.begin(), string.end(), utf8.get(),
        [](wchar_t x, char y) { return char32_t(x) == char32_t(y); }));
  }
  return true;
}
END_TEST(testCharacterEncoding_wide_to_utf8)

BEGIN_TEST(testCharacterEncoding_wide_to_utf8_non_ascii) {
  // Change the locale to be UTF-8 aware for the emoji string.
  ToUTF8Locale utf8locale;

  // Skip this test if UTF-8 isn't supported on this system.
  if (!utf8locale.supported()) {
    return true;
  }

  {
    std::wstring_view string = L"ä";
    auto utf8 = JS::EncodeWideToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);

    CHECK_EQUAL(std::strlen(utf8.get()), 2U);
    CHECK_EQUAL(utf8[0], char(0xC3));
    CHECK_EQUAL(utf8[1], char(0xA4));
  }
  {
    std::wstring_view string = L"💩";
    auto utf8 = JS::EncodeWideToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);

    CHECK_EQUAL(std::strlen(utf8.get()), 4U);
    CHECK_EQUAL(utf8[0], char(0xF0));
    CHECK_EQUAL(utf8[1], char(0x9F));
    CHECK_EQUAL(utf8[2], char(0x92));
    CHECK_EQUAL(utf8[3], char(0xA9));
  }
  return true;
}
END_TEST(testCharacterEncoding_wide_to_utf8_non_ascii)

BEGIN_TEST(testCharacterEncoding_utf8_to_narrow) {
  // Assume the narrow charset is ASCII-compatible. ASCII to UTF-8 conversion is
  // a no-op.
  for (std::string_view string : {
           "",
           "a",
           "abc",
           "abc\0def",
       }) {
    auto narrow = JS::EncodeUtf8ToNarrow(cx, string.data());
    CHECK(narrow != nullptr);
    CHECK_EQUAL(std::strlen(narrow.get()), string.length());
    CHECK(narrow.get() == string);
  }
  return true;
}
END_TEST(testCharacterEncoding_utf8_to_narrow)

BEGIN_TEST(testCharacterEncoding_utf8_to_wide) {
  // Assume the wide charset is ASCII-compatible. ASCII to UTF-8 conversion is
  // a no-op.
  for (std::string_view string : {
           "",
           "a",
           "abc",
           "abc\0def",
       }) {
    auto wide = JS::EncodeUtf8ToWide(cx, string.data());
    CHECK(wide != nullptr);
    CHECK_EQUAL(std::wcslen(wide.get()), string.length());
    CHECK(std::equal(
        string.begin(), string.end(), wide.get(),
        [](char x, wchar_t y) { return char32_t(x) == char32_t(y); }));
  }
  return true;
}
END_TEST(testCharacterEncoding_utf8_to_wide)

BEGIN_TEST(testCharacterEncoding_narrow_roundtrip) {
  // Change the locale to be UTF-8 aware for the emoji string.
  ToUTF8Locale utf8locale;

  // Skip this test if UTF-8 isn't supported on this system.
  if (!utf8locale.supported()) {
    return true;
  }

  for (std::string_view string : {
           "",
           "a",
           "abc",
           "ä",
           "💩",
       }) {
    auto utf8 = JS::EncodeNarrowToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);

    auto narrow = JS::EncodeUtf8ToNarrow(cx, utf8.get());
    CHECK(narrow != nullptr);

    CHECK(narrow.get() == string);
  }
  return true;
}
END_TEST(testCharacterEncoding_narrow_roundtrip)

BEGIN_TEST(testCharacterEncoding_wide_roundtrip) {
  // Change the locale to be UTF-8 aware for the emoji string.
  ToUTF8Locale utf8locale;

  // Skip this test if UTF-8 isn't supported on this system.
  if (!utf8locale.supported()) {
    return true;
  }

  for (std::wstring_view string : {
           L"",
           L"a",
           L"abc",
           L"ä",
           L"💩",
       }) {
    auto utf8 = JS::EncodeWideToUtf8(cx, string.data());
    CHECK(utf8 != nullptr);

    auto wide = JS::EncodeUtf8ToWide(cx, utf8.get());
    CHECK(wide != nullptr);

    CHECK(wide.get() == string);
  }
  return true;
}
END_TEST(testCharacterEncoding_wide_roundtrip)
