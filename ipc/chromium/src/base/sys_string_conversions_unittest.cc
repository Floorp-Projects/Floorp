// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef WCHAR_T_IS_UTF32
static const std::wstring kSysWideOldItalicLetterA = L"\x10300";
#else
static const std::wstring kSysWideOldItalicLetterA = L"\xd800\xdf00";
#endif

TEST(SysStrings, SysWideToUTF8) {
  using base::SysWideToUTF8;
  EXPECT_EQ("Hello, world", SysWideToUTF8(L"Hello, world"));
  EXPECT_EQ("\xe4\xbd\xa0\xe5\xa5\xbd", SysWideToUTF8(L"\x4f60\x597d"));

  // >16 bits
  EXPECT_EQ("\xF0\x90\x8C\x80", SysWideToUTF8(kSysWideOldItalicLetterA));

  // Error case. When Windows finds a UTF-16 character going off the end of
  // a string, it just converts that literal value to UTF-8, even though this
  // is invalid.
  //
  // This is what XP does, but Vista has different behavior, so we don't bother
  // verifying it:
  //EXPECT_EQ("\xE4\xBD\xA0\xED\xA0\x80zyxw",
  //          SysWideToUTF8(L"\x4f60\xd800zyxw"));

  // Test embedded NULLs.
  std::wstring wide_null(L"a");
  wide_null.push_back(0);
  wide_null.push_back('b');

  std::string expected_null("a");
  expected_null.push_back(0);
  expected_null.push_back('b');

  EXPECT_EQ(expected_null, SysWideToUTF8(wide_null));
}

TEST(SysStrings, SysUTF8ToWide) {
  using base::SysUTF8ToWide;
  EXPECT_EQ(L"Hello, world", SysUTF8ToWide("Hello, world"));
  EXPECT_EQ(L"\x4f60\x597d", SysUTF8ToWide("\xe4\xbd\xa0\xe5\xa5\xbd"));
  // >16 bits
  EXPECT_EQ(kSysWideOldItalicLetterA, SysUTF8ToWide("\xF0\x90\x8C\x80"));

  // Error case. When Windows finds an invalid UTF-8 character, it just skips
  // it. This seems weird because it's inconsistent with the reverse conversion.
  //
  // This is what XP does, but Vista has different behavior, so we don't bother
  // verifying it:
  //EXPECT_EQ(L"\x4f60zyxw", SysUTF8ToWide("\xe4\xbd\xa0\xe5\xa5zyxw"));

  // Test embedded NULLs.
  std::string utf8_null("a");
  utf8_null.push_back(0);
  utf8_null.push_back('b');

  std::wstring expected_null(L"a");
  expected_null.push_back(0);
  expected_null.push_back('b');

  EXPECT_EQ(expected_null, SysUTF8ToWide(utf8_null));
}
