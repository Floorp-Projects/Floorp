/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/NumberFormat.h"
#include "TestBuffer.h"

#include <string_view>

namespace mozilla {
namespace intl {

TEST(IntlNumberFormat, Basic)
{
  NumberFormatOptions options;
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("en-US", options).unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(nf->format(1234.56, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "1,234.56");
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(nf->format(1234.56, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"1,234.56");
  const char16_t* res16 = nf->format(1234.56).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"1,234.56");

  UniquePtr<NumberFormat> nfAr =
      NumberFormat::TryCreate("ar", options).unwrap();
  ASSERT_TRUE(nfAr->format(1234.56, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "١٬٢٣٤٫٥٦");
  ASSERT_TRUE(nfAr->format(1234.56, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"١٬٢٣٤٫٥٦");
  res16 = nfAr->format(1234.56).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"١٬٢٣٤٫٥٦");
}

TEST(IntlNumberFormat, Numbers)
{
  NumberFormatOptions options;
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("es-ES", options).unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(nf->format(123456.789, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "123.456,789");
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(nf->format(123456.789, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"123.456,789");

  const char16_t* res = nf->format(123456.789).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"123.456,789");
}

TEST(IntlNumberFormat, SignificantDigits)
{
  NumberFormatOptions options;
  options.mSignificantDigits = Some(std::make_pair(3, 5));
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("es-ES", options).unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(nf->format(123456.789, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "123.460");
  ASSERT_TRUE(nf->format(0.7, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "0,700");
}

TEST(IntlNumberFormat, Currency)
{
  NumberFormatOptions options;
  options.mCurrency =
      Some(std::make_pair("MXN", NumberFormatOptions::CurrencyDisplay::Symbol));
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("es-MX", options).unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(nf->format(123456.789, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "$123,456.79");
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(nf->format(123456.789, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"$123,456.79");
  const char16_t* res = nf->format(123456.789).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"$123,456.79");
}

TEST(IntlNumberFormat, Unit)
{
  NumberFormatOptions options;
  options.mUnit = Some(std::make_pair("meter-per-second",
                                      NumberFormatOptions::UnitDisplay::Long));
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("es-MX", options).unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(nf->format(12.34, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "12.34 metros por segundo");
  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(nf->format(12.34, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"12.34 metros por segundo");
  const char16_t* res = nf->format(12.34).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros por segundo");

  // Create a string view into a longer string and make sure everything works
  // correctly.
  const char* unit = "meter-per-second-with-some-trailing-garbage";
  options.mUnit = Some(std::make_pair(std::string_view(unit, 5),
                                      NumberFormatOptions::UnitDisplay::Long));
  UniquePtr<NumberFormat> nf2 =
      NumberFormat::TryCreate("es-MX", options).unwrap();
  res = nf2->format(12.34).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros");

  options.mUnit = Some(std::make_pair(std::string_view(unit, 16),
                                      NumberFormatOptions::UnitDisplay::Long));
  UniquePtr<NumberFormat> nf3 =
      NumberFormat::TryCreate("es-MX", options).unwrap();
  res = nf3->format(12.34).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34 metros por segundo");
}

TEST(IntlNumberFormat, RoundingMode)
{
  NumberFormatOptions options;
  options.mFractionDigits = Some(std::make_pair(0, 2));
  options.mStripTrailingZero = true;
  options.mRoundingIncrement = 5;
  options.mRoundingMode = NumberFormatOptions::RoundingMode::Ceil;

  UniquePtr<NumberFormat> nf = NumberFormat::TryCreate("en", options).unwrap();

  const char16_t* res16 = nf->format(1.92).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"1.95");

  res16 = nf->format(1.96).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"2");
}

TEST(IntlNumberFormat, Grouping)
{
  NumberFormatOptions options;
  options.mGrouping = NumberFormatOptions::Grouping::Min2;

  UniquePtr<NumberFormat> nf = NumberFormat::TryCreate("en", options).unwrap();

  const char16_t* res16 = nf->format(1'000.0).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"1000");

  res16 = nf->format(10'000.0).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"10,000");
}

TEST(IntlNumberFormat, RoundingPriority)
{
  NumberFormatOptions options;
  options.mFractionDigits = Some(std::make_pair(2, 2));
  options.mSignificantDigits = Some(std::make_pair(1, 2));
  options.mRoundingPriority =
      NumberFormatOptions::RoundingPriority::LessPrecision;

  UniquePtr<NumberFormat> nf1 = NumberFormat::TryCreate("en", options).unwrap();

  const char16_t* res16 = nf1->format(4.321).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"4.30");

  options.mRoundingPriority =
      NumberFormatOptions::RoundingPriority::MorePrecision;

  UniquePtr<NumberFormat> nf2 = NumberFormat::TryCreate("en", options).unwrap();

  res16 = nf2->format(4.321).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(std::u16string_view(res16), u"4.32");
}

TEST(IntlNumberFormat, FormatToParts)
{
  NumberFormatOptions options;
  UniquePtr<NumberFormat> nf =
      NumberFormat::TryCreate("es-ES", options).unwrap();
  NumberPartVector parts;
  const char16_t* res = nf->formatToParts(123456.789, parts).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"123.456,789");
  ASSERT_EQ(parts.length(), 5U);

  // NumberFormat only ever produces number parts with NumberPartSource::Shared.

  ASSERT_EQ(parts[0],
            (NumberPart{NumberPartType::Integer, NumberPartSource::Shared, 3}));
  ASSERT_EQ(parts[1],
            (NumberPart{NumberPartType::Group, NumberPartSource::Shared, 4}));
  ASSERT_EQ(parts[2],
            (NumberPart{NumberPartType::Integer, NumberPartSource::Shared, 7}));
  ASSERT_EQ(parts[3],
            (NumberPart{NumberPartType::Decimal, NumberPartSource::Shared, 8}));
  ASSERT_EQ(parts[4], (NumberPart{NumberPartType::Fraction,
                                  NumberPartSource::Shared, 11}));
}

TEST(IntlNumberFormat, GetAvailableLocales)
{
  using namespace std::literals;

  int32_t english = 0;
  int32_t german = 0;
  int32_t chinese = 0;

  // Since this list is dependent on ICU, and may change between upgrades, only
  // test a subset of the available locales.
  for (const char* locale : NumberFormat::GetAvailableLocales()) {
    if (locale == "en"sv) {
      english++;
    } else if (locale == "de"sv) {
      german++;
    } else if (locale == "zh"sv) {
      chinese++;
    }
  }

  // Each locale should be found exactly once.
  ASSERT_EQ(english, 1);
  ASSERT_EQ(german, 1);
  ASSERT_EQ(chinese, 1);
}

}  // namespace intl
}  // namespace mozilla
