/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/DateIntervalFormat.h"
#include "mozilla/intl/DateTimeFormat.h"
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

  UniquePtr<DateTimeFormat> dtFormat =
      DateTimeFormat::TryCreateFromSkeleton(
          MakeStringSpan("en-US"), MakeStringSpan(u"yMMddHHmm"),
          dateTimePatternGenerator.get(), Nothing(),
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

}  // namespace mozilla::intl
