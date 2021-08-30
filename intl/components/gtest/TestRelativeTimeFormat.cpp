/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/RelativeTimeFormat.h"
#include "TestBuffer.h"

namespace mozilla::intl {

TEST(IntlRelativeTimeFormat, Basic)
{
  RelativeTimeFormatOptions options = {};
  Result<UniquePtr<RelativeTimeFormat>, ICUError> res =
      RelativeTimeFormat::TryCreate("en-US", options);
  ASSERT_TRUE(res.isOk());
  UniquePtr<RelativeTimeFormat> rtf = res.unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 days");

  TestBuffer<char16_t> buf16;
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"in 1.2 days");

  res = RelativeTimeFormat::TryCreate("es-AR", options);
  ASSERT_TRUE(res.isOk());
  rtf = res.unwrap();
  buf8.clear();
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "dentro de 1,2 días");

  buf16.clear();
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"dentro de 1,2 días");

  res = RelativeTimeFormat::TryCreate("ar", options);
  ASSERT_TRUE(res.isOk());
  rtf = res.unwrap();
  buf8.clear();
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "خلال ١٫٢ يوم");

  buf16.clear();
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf16).isOk());
  ASSERT_EQ(buf16.get_string_view(), u"خلال ١٫٢ يوم");
}

TEST(IntlRelativeTimeFormat, Options)
{
  RelativeTimeFormatOptions options = {
      RelativeTimeFormatOptions::Style::Short,
      RelativeTimeFormatOptions::Numeric::Auto};
  Result<UniquePtr<RelativeTimeFormat>, ICUError> res =
      RelativeTimeFormat::TryCreate("fr", options);
  ASSERT_TRUE(res.isOk());
  UniquePtr<RelativeTimeFormat> rtf = res.unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(
      rtf->format(-3.14, RelativeTimeFormat::FormatUnit::Year, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "il y a 3,14 a");

  options = {RelativeTimeFormatOptions::Style::Narrow,
             RelativeTimeFormatOptions::Numeric::Auto};
  res = RelativeTimeFormat::TryCreate("fr", options);
  ASSERT_TRUE(res.isOk());
  rtf = res.unwrap();
  buf8.clear();
  ASSERT_TRUE(
      rtf->format(-3.14, RelativeTimeFormat::FormatUnit::Year, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "-3,14 a");

  options = {RelativeTimeFormatOptions::Style::Long,
             RelativeTimeFormatOptions::Numeric::Auto};
  res = RelativeTimeFormat::TryCreate("fr", options);
  ASSERT_TRUE(res.isOk());
  rtf = res.unwrap();
  buf8.clear();
  ASSERT_TRUE(
      rtf->format(-3.14, RelativeTimeFormat::FormatUnit::Year, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "il y a 3,14 ans");

  options = {RelativeTimeFormatOptions::Style::Long,
             RelativeTimeFormatOptions::Numeric::Auto};
  res = RelativeTimeFormat::TryCreate("fr", options);
  ASSERT_TRUE(res.isOk());
  rtf = res.unwrap();
  buf8.clear();
  ASSERT_TRUE(
      rtf->format(-1, RelativeTimeFormat::FormatUnit::Year, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "l’année dernière");
}

TEST(IntlRelativeTimeFormat, Units)
{
  RelativeTimeFormatOptions options = {};
  Result<UniquePtr<RelativeTimeFormat>, ICUError> res =
      RelativeTimeFormat::TryCreate("en-US", options);
  ASSERT_TRUE(res.isOk());
  UniquePtr<RelativeTimeFormat> rtf = res.unwrap();
  TestBuffer<char> buf8;
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Second, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 seconds");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Minute, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 minutes");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Hour, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 hours");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Day, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 days");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Week, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 weeks");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Month, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 months");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Quarter, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 quarters");
  ASSERT_TRUE(
      rtf->format(1.2, RelativeTimeFormat::FormatUnit::Year, buf8).isOk());
  ASSERT_EQ(buf8.get_string_view(), "in 1.2 years");
}

TEST(IntlRelativeTimeFormat, FormatToParts)
{
  RelativeTimeFormatOptions options = {
      RelativeTimeFormatOptions::Style::Long,
      RelativeTimeFormatOptions::Numeric::Auto};
  Result<UniquePtr<RelativeTimeFormat>, ICUError> res =
      RelativeTimeFormat::TryCreate("es-AR", options);
  ASSERT_TRUE(res.isOk());
  UniquePtr<RelativeTimeFormat> rtf = res.unwrap();
  NumberPartVector parts;
  Result<Span<const char16_t>, ICUError> strRes =
      rtf->formatToParts(-1.2, RelativeTimeFormat::FormatUnit::Year, parts);
  ASSERT_TRUE(strRes.isOk());
  ASSERT_EQ(strRes.unwrap(), MakeStringSpan(u"hace 1,2 años"));
  ASSERT_EQ(parts.length(), 5U);
  ASSERT_EQ(parts[0],
            (NumberPart{NumberPartType::Literal, NumberPartSource::Shared, 5}));
  ASSERT_EQ(parts[1],
            (NumberPart{NumberPartType::Integer, NumberPartSource::Shared, 6}));
  ASSERT_EQ(parts[2],
            (NumberPart{NumberPartType::Decimal, NumberPartSource::Shared, 7}));
  ASSERT_EQ(parts[3], (NumberPart{NumberPartType::Fraction,
                                  NumberPartSource::Shared, 8}));
  ASSERT_EQ(parts[4], (NumberPart{NumberPartType::Literal,
                                  NumberPartSource::Shared, 13}));
}

}  // namespace mozilla::intl
