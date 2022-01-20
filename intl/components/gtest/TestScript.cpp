/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Script.h"
#include "nsUnicodeScriptCodes.h"

namespace mozilla::intl {
TEST(IntlScript, GetExtensions)
{
  ScriptExtensionVector extensions;

  // 0x0000..0x0040 are Common.
  for (char32_t ch = 0; ch < 0x0041; ch++) {
    ASSERT_TRUE(Script::GetExtensions(ch, extensions).isOk());
    ASSERT_EQ(extensions.length(), 1u);
    ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::COMMON);
  }

  // 0x0300..0x0341 are Inherited.
  for (char32_t ch = 0x300; ch < 0x0341; ch++) {
    ASSERT_TRUE(Script::GetExtensions(ch, extensions).isOk());
    ASSERT_EQ(extensions.length(), 1u);
    ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::INHERITED);
  }

  // 0x1cf7's script code is Common, but its script extension is Beng.
  ASSERT_TRUE(Script::GetExtensions(0x1cf7, extensions).isOk());
  ASSERT_EQ(extensions.length(), 1u);
  ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::BENGALI);

  // ؿ
  // https://unicode-table.com/en/063F/
  // This character doesn't have any script extension, so the script code is
  // returned.
  ASSERT_TRUE(Script::GetExtensions(0x063f, extensions).isOk());
  ASSERT_EQ(extensions.length(), 1u);
  ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::ARABIC);

  // 0xff65 is the unicode character '･', see https://unicode-table.com/en/FF65/
  // Halfwidth Katakana Middle Dot.
  ASSERT_TRUE(Script::GetExtensions(0xff65, extensions).isOk());

  // 0xff65 should have the following script extensions:
  // Bopo Hang Hani Hira Kana Yiii.
  ASSERT_EQ(extensions.length(), 6u);

  ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::BOPOMOFO);
  ASSERT_EQ(unicode::Script(extensions[1]), unicode::Script::HAN);
  ASSERT_EQ(unicode::Script(extensions[2]), unicode::Script::HANGUL);
  ASSERT_EQ(unicode::Script(extensions[3]), unicode::Script::HIRAGANA);
  ASSERT_EQ(unicode::Script(extensions[4]), unicode::Script::KATAKANA);
  ASSERT_EQ(unicode::Script(extensions[5]), unicode::Script::YI);

  // The max code point is 0x10ffff, so 0x110000 should be invalid.
  // Script::UNKNOWN should be returned for an invalid code point.
  ASSERT_TRUE(Script::GetExtensions(0x110000, extensions).isOk());
  ASSERT_EQ(extensions.length(), 1u);
  ASSERT_EQ(unicode::Script(extensions[0]), unicode::Script::UNKNOWN);
}
}  // namespace mozilla::intl
