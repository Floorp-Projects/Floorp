/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "gtest/gtest.h"

#include "mozilla/Encoding.h"

#define ENCODING_TEST(name) TEST(EncodingTest, name)

using namespace mozilla;

// These tests mainly test that the C++ interface seems to
// reach the Rust code. More thorough testing of the back
// end is done in Rust.

ENCODING_TEST(ForLabel)
{
  nsAutoCString label("  uTf-8   ");
  ASSERT_EQ(Encoding::ForLabel(label), UTF_8_ENCODING);
  label.AssignLiteral("   cseucpkdfmTjapanese  ");
  ASSERT_EQ(Encoding::ForLabel(label), EUC_JP_ENCODING);
}

ENCODING_TEST(ForName)
{
  nsAutoCString encoding("UTF-8");
  ASSERT_EQ(Encoding::ForName(encoding), UTF_8_ENCODING);
  encoding.AssignLiteral("EUC-JP");
  ASSERT_EQ(Encoding::ForName(encoding), EUC_JP_ENCODING);
}

ENCODING_TEST(ForBOM)
{
  nsAutoCString data("\xEF\xBB\xBF\x61");
  const Encoding* encoding;
  size_t bomLength;
  Tie(encoding, bomLength) = Encoding::ForBOM(data);
  ASSERT_EQ(encoding, UTF_8_ENCODING);
  ASSERT_EQ(bomLength, 3U);
  data.AssignLiteral("\xFF\xFE");
  Tie(encoding, bomLength) = Encoding::ForBOM(data);
  ASSERT_EQ(encoding, UTF_16LE_ENCODING);
  ASSERT_EQ(bomLength, 2U);
  data.AssignLiteral("\xFE\xFF");
  Tie(encoding, bomLength) = Encoding::ForBOM(data);
  ASSERT_EQ(encoding, UTF_16BE_ENCODING);
  ASSERT_EQ(bomLength, 2U);
  data.AssignLiteral("\xEF\xBB");
  Tie(encoding, bomLength) = Encoding::ForBOM(data);
  ASSERT_EQ(encoding, nullptr);
  ASSERT_EQ(bomLength, 0U);
}

ENCODING_TEST(Name)
{
  nsAutoCString name;
  UTF_8_ENCODING->Name(name);
  ASSERT_TRUE(name.EqualsLiteral("UTF-8"));
  GBK_ENCODING->Name(name);
  ASSERT_TRUE(name.EqualsLiteral("GBK"));
}

ENCODING_TEST(CanEncodeEverything)
{
  ASSERT_TRUE(UTF_8_ENCODING->CanEncodeEverything());
  ASSERT_FALSE(GB18030_ENCODING->CanEncodeEverything());
}

ENCODING_TEST(IsAsciiCompatible)
{
  ASSERT_TRUE(UTF_8_ENCODING->IsAsciiCompatible());
  ASSERT_FALSE(ISO_2022_JP_ENCODING->IsAsciiCompatible());
}
