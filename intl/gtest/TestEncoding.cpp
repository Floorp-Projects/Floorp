/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "gtest/gtest.h"

#include "mozilla/Encoding.h"
#include <type_traits>

#define ENCODING_TEST(name) TEST(EncodingTest, name)

using namespace mozilla;

static_assert(std::is_standard_layout<NotNull<const Encoding*>>::value,
              "NotNull<const Encoding*> must be a standard layout type.");

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
  nsAutoCString encoding("GBK");
  ASSERT_EQ(Encoding::ForName(encoding), GBK_ENCODING);
  encoding.AssignLiteral("Big5");
  ASSERT_EQ(Encoding::ForName(encoding), BIG5_ENCODING);
  encoding.AssignLiteral("UTF-8");
  ASSERT_EQ(Encoding::ForName(encoding), UTF_8_ENCODING);
  encoding.AssignLiteral("IBM866");
  ASSERT_EQ(Encoding::ForName(encoding), IBM866_ENCODING);
  encoding.AssignLiteral("EUC-JP");
  ASSERT_EQ(Encoding::ForName(encoding), EUC_JP_ENCODING);
  encoding.AssignLiteral("KOI8-R");
  ASSERT_EQ(Encoding::ForName(encoding), KOI8_R_ENCODING);
  encoding.AssignLiteral("EUC-KR");
  ASSERT_EQ(Encoding::ForName(encoding), EUC_KR_ENCODING);
  encoding.AssignLiteral("KOI8-U");
  ASSERT_EQ(Encoding::ForName(encoding), KOI8_U_ENCODING);
  encoding.AssignLiteral("gb18030");
  ASSERT_EQ(Encoding::ForName(encoding), GB18030_ENCODING);
  encoding.AssignLiteral("UTF-16BE");
  ASSERT_EQ(Encoding::ForName(encoding), UTF_16BE_ENCODING);
  encoding.AssignLiteral("UTF-16LE");
  ASSERT_EQ(Encoding::ForName(encoding), UTF_16LE_ENCODING);
  encoding.AssignLiteral("Shift_JIS");
  ASSERT_EQ(Encoding::ForName(encoding), SHIFT_JIS_ENCODING);
  encoding.AssignLiteral("macintosh");
  ASSERT_EQ(Encoding::ForName(encoding), MACINTOSH_ENCODING);
  encoding.AssignLiteral("ISO-8859-2");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_2_ENCODING);
  encoding.AssignLiteral("ISO-8859-3");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_3_ENCODING);
  encoding.AssignLiteral("ISO-8859-4");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_4_ENCODING);
  encoding.AssignLiteral("ISO-8859-5");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_5_ENCODING);
  encoding.AssignLiteral("ISO-8859-6");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_6_ENCODING);
  encoding.AssignLiteral("ISO-8859-7");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_7_ENCODING);
  encoding.AssignLiteral("ISO-8859-8");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_8_ENCODING);
  encoding.AssignLiteral("ISO-8859-10");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_10_ENCODING);
  encoding.AssignLiteral("ISO-8859-13");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_13_ENCODING);
  encoding.AssignLiteral("ISO-8859-14");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_14_ENCODING);
  encoding.AssignLiteral("windows-874");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_874_ENCODING);
  encoding.AssignLiteral("ISO-8859-15");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_15_ENCODING);
  encoding.AssignLiteral("ISO-8859-16");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_16_ENCODING);
  encoding.AssignLiteral("ISO-2022-JP");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_2022_JP_ENCODING);
  encoding.AssignLiteral("replacement");
  ASSERT_EQ(Encoding::ForName(encoding), REPLACEMENT_ENCODING);
  encoding.AssignLiteral("windows-1250");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1250_ENCODING);
  encoding.AssignLiteral("windows-1251");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1251_ENCODING);
  encoding.AssignLiteral("windows-1252");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1252_ENCODING);
  encoding.AssignLiteral("windows-1253");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1253_ENCODING);
  encoding.AssignLiteral("windows-1254");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1254_ENCODING);
  encoding.AssignLiteral("windows-1255");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1255_ENCODING);
  encoding.AssignLiteral("windows-1256");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1256_ENCODING);
  encoding.AssignLiteral("windows-1257");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1257_ENCODING);
  encoding.AssignLiteral("windows-1258");
  ASSERT_EQ(Encoding::ForName(encoding), WINDOWS_1258_ENCODING);
  encoding.AssignLiteral("ISO-8859-8-I");
  ASSERT_EQ(Encoding::ForName(encoding), ISO_8859_8_I_ENCODING);
  encoding.AssignLiteral("x-mac-cyrillic");
  ASSERT_EQ(Encoding::ForName(encoding), X_MAC_CYRILLIC_ENCODING);
  encoding.AssignLiteral("x-user-defined");
  ASSERT_EQ(Encoding::ForName(encoding), X_USER_DEFINED_ENCODING);
}

// Test disabled pending bug 1393711
#if 0
ENCODING_TEST(BogusName)
{
  nsAutoCString encoding("utf-8");
  ASSERT_DEATH_IF_SUPPORTED(Encoding::ForName(encoding), "Bogus encoding name");
  encoding.AssignLiteral("ISO-8859-1");
  ASSERT_DEATH_IF_SUPPORTED(Encoding::ForName(encoding), "Bogus encoding name");
  encoding.AssignLiteral("gbk");
  ASSERT_DEATH_IF_SUPPORTED(Encoding::ForName(encoding), "Bogus encoding name");
  encoding.AssignLiteral(" UTF-8 ");
  ASSERT_DEATH_IF_SUPPORTED(Encoding::ForName(encoding), "Bogus encoding name");
}
#endif

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
