/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/DateIntervalFormat.h"
#include "mozilla/intl/DateTimeFormat.h"
#include "mozilla/intl/DateTimePart.h"
#include "mozilla/Span.h"

#include "unicode/uformattedvalue.h"

#include "TestBuffer.h"

namespace mozilla::intl {

const double DATE201901030000GMT = 1546473600000.0;
const double DATE201901050000GMT = 1546646400000.0;

TEST(IntlDateIntervalFormat, TryFormatDateTime)
{
  UniquePtr<DateIntervalFormat> dif =
      DateIntervalFormat::TryCreate(MakeStringSpan("en-US"),
                                    MakeStringSpan(u"MMddHHmm"),
                                    MakeStringSpan(u"GMT"))
          .unwrap();

  AutoFormattedDateInterval formatted;

  // Pass two same Date time, 'equal' should be true.
  bool equal;
  auto result = dif->TryFormatDateTime(DATE201901030000GMT, DATE201901030000GMT,
                                       formatted, &equal);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(equal);

  auto spanResult = formatted.ToSpan();
  ASSERT_TRUE(spanResult.isOk());

  ASSERT_EQ(spanResult.unwrap(), MakeStringSpan(u"01/03, 00:00"));

  result = dif->TryFormatDateTime(DATE201901030000GMT, DATE201901050000GMT,
                                  formatted, &equal);
  ASSERT_TRUE(result.isOk());
  ASSERT_FALSE(equal);

  spanResult = formatted.ToSpan();
  ASSERT_TRUE(spanResult.isOk());
  ASSERT_EQ(spanResult.unwrap(),
            MakeStringSpan(u"01/03, 00:00 – 01/05, 00:00"));
}

TEST(IntlDateIntervalFormat, TryFormatCalendar)
{
  auto dateTimePatternGenerator =
      DateTimePatternGenerator::TryCreate("en").unwrap();

  DateTimeFormat::ComponentsBag components;
  components.year = Some(DateTimeFormat::Numeric::Numeric);
  components.month = Some(DateTimeFormat::Month::TwoDigit);
  components.day = Some(DateTimeFormat::Numeric::TwoDigit);
  components.hour = Some(DateTimeFormat::Numeric::TwoDigit);
  components.minute = Some(DateTimeFormat::Numeric::TwoDigit);
  components.hour12 = Some(false);

  UniquePtr<DateTimeFormat> dtFormat =
      DateTimeFormat::TryCreateFromComponents(
          MakeStringSpan("en-US"), components, dateTimePatternGenerator.get(),
          Some(MakeStringSpan(u"GMT")))
          .unwrap();

  UniquePtr<DateIntervalFormat> dif =
      DateIntervalFormat::TryCreate(MakeStringSpan("en-US"),
                                    MakeStringSpan(u"MMddHHmm"),
                                    MakeStringSpan(u"GMT"))
          .unwrap();

  AutoFormattedDateInterval formatted;

  // Two Calendar objects with the same date time.
  auto sameCal = dtFormat->CloneCalendar(DATE201901030000GMT);
  ASSERT_TRUE(sameCal.isOk());

  auto cal = sameCal.unwrap();
  bool equal;
  auto result = dif->TryFormatCalendar(*cal, *cal, formatted, &equal);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(equal);

  auto spanResult = formatted.ToSpan();
  ASSERT_TRUE(spanResult.isOk());
  ASSERT_EQ(spanResult.unwrap(), MakeStringSpan(u"01/03, 00:00"));

  auto startCal = dtFormat->CloneCalendar(DATE201901030000GMT);
  ASSERT_TRUE(startCal.isOk());
  auto endCal = dtFormat->CloneCalendar(DATE201901050000GMT);
  ASSERT_TRUE(endCal.isOk());

  result = dif->TryFormatCalendar(*startCal.unwrap(), *endCal.unwrap(),
                                  formatted, &equal);
  ASSERT_TRUE(result.isOk());
  ASSERT_FALSE(equal);

  spanResult = formatted.ToSpan();
  ASSERT_TRUE(spanResult.isOk());
  ASSERT_EQ(spanResult.unwrap(),
            MakeStringSpan(u"01/03, 00:00 – 01/05, 00:00"));
}

TEST(IntlDateIntervalFormat, TryFormattedToParts)
{
  UniquePtr<DateIntervalFormat> dif =
      DateIntervalFormat::TryCreate(MakeStringSpan("en-US"),
                                    MakeStringSpan(u"MMddHHmm"),
                                    MakeStringSpan(u"GMT"))
          .unwrap();

  AutoFormattedDateInterval formatted;
  bool equal;
  auto result = dif->TryFormatDateTime(DATE201901030000GMT, DATE201901050000GMT,
                                       formatted, &equal);
  ASSERT_TRUE(result.isOk());
  ASSERT_FALSE(equal);

  Span<const char16_t> formattedSpan = formatted.ToSpan().unwrap();
  ASSERT_EQ(formattedSpan, MakeStringSpan(u"01/03, 00:00 – 01/05, 00:00"));

  mozilla::intl::DateTimePartVector parts;
  result = dif->TryFormattedToParts(formatted, parts);
  ASSERT_TRUE(result.isOk());

  auto getSubSpan = [formattedSpan, &parts](size_t index) {
    size_t start = index == 0 ? 0 : parts[index - 1].mEndIndex;
    size_t end = parts[index].mEndIndex;
    return formattedSpan.FromTo(start, end);
  };

  ASSERT_EQ(parts[0].mType, DateTimePartType::Month);
  ASSERT_EQ(getSubSpan(0), MakeStringSpan(u"01"));
  ASSERT_EQ(parts[0].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[1].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(1), MakeStringSpan(u"/"));
  ASSERT_EQ(parts[1].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[2].mType, DateTimePartType::Day);
  ASSERT_EQ(getSubSpan(2), MakeStringSpan(u"03"));
  ASSERT_EQ(parts[2].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[3].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(3), MakeStringSpan(u", "));
  ASSERT_EQ(parts[3].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[4].mType, DateTimePartType::Hour);
  ASSERT_EQ(getSubSpan(4), MakeStringSpan(u"00"));
  ASSERT_EQ(parts[4].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[5].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(5), MakeStringSpan(u":"));
  ASSERT_EQ(parts[5].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[6].mType, DateTimePartType::Minute);
  ASSERT_EQ(getSubSpan(6), MakeStringSpan(u"00"));
  ASSERT_EQ(parts[6].mSource, DateTimePartSource::StartRange);

  ASSERT_EQ(parts[7].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(7), MakeStringSpan(u" – "));
  ASSERT_EQ(parts[7].mSource, DateTimePartSource::Shared);

  ASSERT_EQ(parts[8].mType, DateTimePartType::Month);
  ASSERT_EQ(getSubSpan(8), MakeStringSpan(u"01"));
  ASSERT_EQ(parts[8].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[9].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(9), MakeStringSpan(u"/"));
  ASSERT_EQ(parts[9].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[10].mType, DateTimePartType::Day);
  ASSERT_EQ(getSubSpan(10), MakeStringSpan(u"05"));
  ASSERT_EQ(parts[10].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[11].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(11), MakeStringSpan(u", "));
  ASSERT_EQ(parts[11].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[12].mType, DateTimePartType::Hour);
  ASSERT_EQ(getSubSpan(12), MakeStringSpan(u"00"));
  ASSERT_EQ(parts[12].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[13].mType, DateTimePartType::Literal);
  ASSERT_EQ(getSubSpan(13), MakeStringSpan(u":"));
  ASSERT_EQ(parts[13].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts[14].mType, DateTimePartType::Minute);
  ASSERT_EQ(getSubSpan(14), MakeStringSpan(u"00"));
  ASSERT_EQ(parts[14].mSource, DateTimePartSource::EndRange);

  ASSERT_EQ(parts.length(), 15u);
}
}  // namespace mozilla::intl
