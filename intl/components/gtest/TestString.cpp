/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/String.h"
#include "mozilla/Span.h"

#include "TestBuffer.h"

namespace mozilla::intl {

static Result<std::u16string_view, ICUError> ToLocaleLowerCase(
    const char* aLocale, const char16_t* aString,
    TestBuffer<char16_t>& aBuffer) {
  aBuffer.clear();

  MOZ_TRY(String::ToLocaleLowerCase(aLocale, MakeStringSpan(aString), aBuffer));

  return aBuffer.get_string_view();
}

static Result<std::u16string_view, ICUError> ToLocaleUpperCase(
    const char* aLocale, const char16_t* aString,
    TestBuffer<char16_t>& aBuffer) {
  aBuffer.clear();

  MOZ_TRY(String::ToLocaleUpperCase(aLocale, MakeStringSpan(aString), aBuffer));

  return aBuffer.get_string_view();
}

TEST(IntlString, ToLocaleLowerCase)
{
  TestBuffer<char16_t> buf;

  ASSERT_EQ(ToLocaleLowerCase("en", u"test", buf).unwrap(), u"test");
  ASSERT_EQ(ToLocaleLowerCase("en", u"TEST", buf).unwrap(), u"test");

  // Turkish dotless i.
  ASSERT_EQ(ToLocaleLowerCase("tr", u"I", buf).unwrap(), u"ı");
  ASSERT_EQ(ToLocaleLowerCase("tr", u"İ", buf).unwrap(), u"i");
  ASSERT_EQ(ToLocaleLowerCase("tr", u"I\u0307", buf).unwrap(), u"i");
}

TEST(IntlString, ToLocaleUpperCase)
{
  TestBuffer<char16_t> buf;

  ASSERT_EQ(ToLocaleUpperCase("en", u"test", buf).unwrap(), u"TEST");
  ASSERT_EQ(ToLocaleUpperCase("en", u"TEST", buf).unwrap(), u"TEST");

  // Turkish dotless i.
  ASSERT_EQ(ToLocaleUpperCase("tr", u"i", buf).unwrap(), u"İ");
  ASSERT_EQ(ToLocaleUpperCase("tr", u"ı", buf).unwrap(), u"I");

  // Output can be longer than the input string.
  ASSERT_EQ(ToLocaleUpperCase("en", u"Größenmaßstäbe", buf).unwrap(),
            u"GRÖSSENMASSSTÄBE");
}

TEST(IntlString, NormalizeNFC)
{
  using namespace std::literals;

  using NormalizationForm = String::NormalizationForm;
  using AlreadyNormalized = String::AlreadyNormalized;

  TestBuffer<char16_t> buf;

  auto alreadyNormalized =
      String::Normalize(NormalizationForm::NFC, u""sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFC, u"abcdef"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFC, u"a\u0308"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"ä");

  buf.clear();

  alreadyNormalized = String::Normalize(NormalizationForm::NFC, u"½"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");
}

TEST(IntlString, NormalizeNFD)
{
  using namespace std::literals;

  using NormalizationForm = String::NormalizationForm;
  using AlreadyNormalized = String::AlreadyNormalized;

  TestBuffer<char16_t> buf;

  auto alreadyNormalized =
      String::Normalize(NormalizationForm::NFD, u""sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFD, u"abcdef"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized = String::Normalize(NormalizationForm::NFD, u"ä"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"a\u0308");

  buf.clear();

  alreadyNormalized = String::Normalize(NormalizationForm::NFD, u"½"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");
}

TEST(IntlString, NormalizeNFKC)
{
  using namespace std::literals;

  using NormalizationForm = String::NormalizationForm;
  using AlreadyNormalized = String::AlreadyNormalized;

  TestBuffer<char16_t> buf;

  auto alreadyNormalized =
      String::Normalize(NormalizationForm::NFKC, u""sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFKC, u"abcdef"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFKC, u"a\u0308"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"ä");

  buf.clear();

  alreadyNormalized = String::Normalize(NormalizationForm::NFKC, u"½"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"1⁄2");
}

TEST(IntlString, NormalizeNFKD)
{
  using namespace std::literals;

  using NormalizationForm = String::NormalizationForm;
  using AlreadyNormalized = String::AlreadyNormalized;

  TestBuffer<char16_t> buf;

  auto alreadyNormalized =
      String::Normalize(NormalizationForm::NFKD, u""sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized =
      String::Normalize(NormalizationForm::NFKD, u"abcdef"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::Yes);
  ASSERT_EQ(buf.get_string_view(), u"");

  alreadyNormalized = String::Normalize(NormalizationForm::NFKD, u"ä"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"a\u0308");

  buf.clear();

  alreadyNormalized = String::Normalize(NormalizationForm::NFKD, u"½"sv, buf);
  ASSERT_EQ(alreadyNormalized.unwrap(), AlreadyNormalized::No);
  ASSERT_EQ(buf.get_string_view(), u"1⁄2");
}

TEST(IntlString, IsCased)
{
  ASSERT_TRUE(String::IsCased(U'a'));
  ASSERT_FALSE(String::IsCased(U'0'));
}

TEST(IntlString, IsCaseIgnorable)
{
  ASSERT_FALSE(String::IsCaseIgnorable(U'a'));
  ASSERT_TRUE(String::IsCaseIgnorable(U'.'));
}

}  // namespace mozilla::intl
