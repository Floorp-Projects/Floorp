/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/IDNA.h"
#include "mozilla/Span.h"
#include "TestBuffer.h"

namespace mozilla::intl {

TEST(IntlIDNA, LabelToUnicodeBasic)
{
  auto createResult = IDNA::TryCreate(IDNA::ProcessingType::NonTransitional);
  ASSERT_TRUE(createResult.isOk());
  auto idna = createResult.unwrap();

  // 'A' and 'ª' are mapped to 'a'.
  TestBuffer<char16_t> buf16;
  auto convertResult = idna->LabelToUnicode(MakeStringSpan(u"Aa\u00aa"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  intl::IDNA::Info info = convertResult.unwrap();
  ASSERT_TRUE(!info.HasErrors());
  ASSERT_EQ(buf16.get_string_view(), u"aaa");

  buf16.clear();
  // For nontransitional processing 'ß' is still mapped to 'ß'.
  convertResult = idna->LabelToUnicode(MakeStringSpan(u"Faß"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  info = convertResult.unwrap();
  ASSERT_TRUE(!info.HasErrors());
  ASSERT_EQ(buf16.get_string_view(), u"faß");
}

TEST(IntlIDNA, LabelToUnicodeBasicTransitional)
{
  auto createResult = IDNA::TryCreate(IDNA::ProcessingType::Transitional);
  ASSERT_TRUE(createResult.isOk());
  auto idna = createResult.unwrap();

  TestBuffer<char16_t> buf16;
  // For transitional processing 'ß' will be mapped to 'ss'.
  auto convertResult = idna->LabelToUnicode(MakeStringSpan(u"Faß"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  intl::IDNA::Info info = convertResult.unwrap();
  ASSERT_TRUE(!info.HasErrors());
  ASSERT_EQ(buf16.get_string_view(), u"fass");
}

TEST(IntlIDNA, LabelToUnicodeHasErrors)
{
  auto createResult = IDNA::TryCreate(IDNA::ProcessingType::NonTransitional);
  ASSERT_TRUE(createResult.isOk());
  auto idna = createResult.unwrap();
  TestBuffer<char16_t> buf16;
  // \u0378 is a reserved charactor, conversion should be disallowed.
  auto convertResult = idna->LabelToUnicode(MakeStringSpan(u"\u0378"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  intl::IDNA::Info info = convertResult.unwrap();
  ASSERT_TRUE(info.HasErrors());

  buf16.clear();
  // FULL STOP '.' is not allowed.
  convertResult = idna->LabelToUnicode(MakeStringSpan(u"a.b"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  info = convertResult.unwrap();
  ASSERT_TRUE(info.HasErrors());
}

TEST(IntlIDNA, LabelToUnicodeHasInvalidPunycode)
{
  auto createResult = IDNA::TryCreate(IDNA::ProcessingType::NonTransitional);
  ASSERT_TRUE(createResult.isOk());
  auto idna = createResult.unwrap();
  TestBuffer<char16_t> buf16;
  auto convertResult =
      idna->LabelToUnicode(MakeStringSpan(u"xn--a-ecp.ru"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  intl::IDNA::Info info = convertResult.unwrap();
  ASSERT_TRUE(info.HasInvalidPunycode());

  buf16.clear();
  convertResult = idna->LabelToUnicode(MakeStringSpan(u"xn--0.pt"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  info = convertResult.unwrap();
  ASSERT_TRUE(info.HasInvalidPunycode());
}

TEST(IntlIDNA, LabelToUnicodeHasInvalidHyphen)
{
  auto createResult = IDNA::TryCreate(IDNA::ProcessingType::NonTransitional);
  ASSERT_TRUE(createResult.isOk());
  auto idna = createResult.unwrap();
  TestBuffer<char16_t> buf16;

  // Leading hyphen.
  auto convertResult = idna->LabelToUnicode(MakeStringSpan(u"-a"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  intl::IDNA::Info info = convertResult.unwrap();
  ASSERT_TRUE(info.HasErrors());
  ASSERT_TRUE(info.HasInvalidHyphen());

  buf16.clear();
  // Trailing hyphen.
  convertResult = idna->LabelToUnicode(MakeStringSpan(u"a-"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  info = convertResult.unwrap();
  ASSERT_TRUE(info.HasInvalidHyphen());

  buf16.clear();
  // Contains hyphens in both 3rd and 4th positions.
  convertResult = idna->LabelToUnicode(MakeStringSpan(u"ab--c"), buf16);
  ASSERT_TRUE(convertResult.isOk());
  info = convertResult.unwrap();
  ASSERT_TRUE(info.HasInvalidHyphen());
}

}  // namespace mozilla::intl
