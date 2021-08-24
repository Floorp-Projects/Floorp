/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include <string_view>

#include "mozilla/intl/NumberRangeFormat.h"
#include "./TestBuffer.h"

namespace mozilla {
namespace intl {

using namespace std::literals;

TEST(IntlNumberRangeFormat, Basic)
{
#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
  NumberRangeFormatOptions options;
  UniquePtr<NumberRangeFormat> nf =
      NumberRangeFormat::TryCreate("en-US", options).unwrap();

  const char16_t* res16 = nf->format(1234.56, 1234.56).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(res16, u"1,234.56"sv);

  options.mRangeIdentityFallback = NumberRangeFormatOptions::Approximately;
  nf = std::move(NumberRangeFormat::TryCreate("en-US", options).unwrap());

  res16 = nf->format("1234.56", "1234.56").unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(res16, u"~1,234.56"sv);

  res16 = nf->format("1234.56", "2999.89").unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(res16, u"1,234.56–2,999.89"sv);

  nf = std::move(NumberRangeFormat::TryCreate("ar", options).unwrap());

  res16 = nfAr->format(1234.56, 1234.56).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(res16, u"~١٬٢٣٤٫٥٦"sv);

  res16 = nfAr->format(1234.56, 2999.89).unwrap().data();
  ASSERT_TRUE(res16 != nullptr);
  ASSERT_EQ(res16, u"١٬٢٣٤٫٥٦–٢٬٩٩٩٫٨٩"sv);
#endif
}

TEST(IntlNumberRangeFormat, Currency)
{
#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
  NumberRangeFormatOptions options;
  options.mCurrency = Some(
      std::make_pair("MXN", NumberRangeFormatOptions::CurrencyDisplay::Symbol));
  UniquePtr<NumberRangeFormat> nf =
      NumberRangeFormat::TryCreate("es-MX", options).unwrap();

  const char16_t* res = nf->format(123456.789, 299999.89).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"$123,456.79 - $299,999.89"sv);

  options.mCurrency = Some(
      std::make_pair("EUR", NumberRangeFormatOptions::CurrencyDisplay::Symbol));
  nf = std::move(NumberRangeFormat::TryCreate("fr", options).unwrap());

  res = nf->format(123456.789, 299999.89).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"123 456,79–299 999,89 €"sv);
#endif
}

TEST(IntlNumberRangeFormat, Unit)
{
#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
  NumberRangeFormatOptions options;
  options.mUnit = Some(std::make_pair(
      "meter-per-second", NumberRangeFormatOptions::UnitDisplay::Long));
  UniquePtr<NumberRangeFormat> nf =
      NumberRangeFormat::TryCreate("es-MX", options).unwrap();

  const char16_t* res = nf->format(12.34, 56.78).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"12.34-56.78 metros por segundo");
#endif
}

TEST(IntlNumberRangeFormat, FormatToParts)
{
#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT
  NumberRangeFormatOptions options;
  UniquePtr<NumberRangeFormat> nf =
      NumberRangeFormat::TryCreate("es-ES", options).unwrap();
  NumberPartVector parts;
  const char16_t* res =
      nf->formatToParts(123456.789, 299999.89, parts).unwrap().data();
  ASSERT_TRUE(res != nullptr);
  ASSERT_EQ(std::u16string_view(res), u"123.456,789-299.999,89"sv);
  ASSERT_EQ(parts.length(), 11U);
  ASSERT_EQ(parts[0],
            (NumberPart{NumberPartType::Integer, NumberPartSource::Start, 3}));
  ASSERT_EQ(parts[1],
            (NumberPart{NumberPartType::Group, NumberPartSource::Start, 4}));
  ASSERT_EQ(parts[2],
            (NumberPart{NumberPartType::Integer, NumberPartSource::Start, 7}));
  ASSERT_EQ(parts[3],
            (NumberPart{NumberPartType::Decimal, NumberPartSource::Start, 8}));
  ASSERT_EQ(parts[4], (NumberPart{NumberPartType::Fraction,
                                  NumberPartSource::Start, 11}));
  ASSERT_EQ(parts[5], (NumberPart{NumberPartType::Fraction,
                                  NumberPartSource::Shared, 12}));
  ASSERT_EQ(parts[6],
            (NumberPart{NumberPartType::Integer, NumberPartSource::End, 15}));
  ASSERT_EQ(parts[7],
            (NumberPart{NumberPartType::Group, NumberPartSource::End, 16}));
  ASSERT_EQ(parts[8],
            (NumberPart{NumberPartType::Integer, NumberPartSource::End, 19}));
  ASSERT_EQ(parts[9],
            (NumberPart{NumberPartType::Decimal, NumberPartSource::End, 20}));
  ASSERT_EQ(parts[10],
            (NumberPart{NumberPartType::Fraction, NumberPartSource::End, 23}));
#endif
}

}  // namespace intl
}  // namespace mozilla
