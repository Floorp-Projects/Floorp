/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Calendar.h"
#include "mozilla/intl/DateTimeFormat.h"
#include "mozilla/Span.h"
#include "TestBuffer.h"

namespace mozilla::intl {

// Firefox 1.0 release date.
const double CALENDAR_DATE = 1032800850000.0;

TEST(IntlCalendar, GetLegacyKeywordValuesForLocale)
{
  bool hasGregorian = false;
  bool hasIslamic = false;
  auto gregorian = MakeStringSpan("gregorian");
  auto islamic = MakeStringSpan("islamic");
  auto keywords = Calendar::GetLegacyKeywordValuesForLocale("en-US").unwrap();
  for (auto name : keywords) {
    // Check a few keywords, as this list may not be stable between ICU updates.
    if (name.unwrap() == gregorian) {
      hasGregorian = true;
    }
    if (name.unwrap() == islamic) {
      hasIslamic = true;
    }
  }
  ASSERT_TRUE(hasGregorian);
  ASSERT_TRUE(hasIslamic);
}

TEST(IntlCalendar, GetBcp47KeywordValuesForLocale)
{
  bool hasGregory = false;
  bool hasIslamic = false;
  auto gregory = MakeStringSpan("gregory");
  auto islamic = MakeStringSpan("islamic");
  auto keywords = Calendar::GetBcp47KeywordValuesForLocale("en-US").unwrap();
  for (auto name : keywords) {
    // Check a few keywords, as this list may not be stable between ICU updates.
    if (name.unwrap() == gregory) {
      hasGregory = true;
    }
    if (name.unwrap() == islamic) {
      hasIslamic = true;
    }
  }
  ASSERT_TRUE(hasGregory);
  ASSERT_TRUE(hasIslamic);
}

TEST(IntlCalendar, GetBcp47KeywordValuesForLocaleCommonlyUsed)
{
  bool hasGregory = false;
  bool hasIslamic = false;
  auto gregory = MakeStringSpan("gregory");
  auto islamic = MakeStringSpan("islamic");
  auto keywords = Calendar::GetBcp47KeywordValuesForLocale(
                      "en-US", Calendar::CommonlyUsed::Yes)
                      .unwrap();
  for (auto name : keywords) {
    // Check a few keywords, as this list may not be stable between ICU updates.
    if (name.unwrap() == gregory) {
      hasGregory = true;
    }
    if (name.unwrap() == islamic) {
      hasIslamic = true;
    }
  }
  ASSERT_TRUE(hasGregory);
  ASSERT_FALSE(hasIslamic);
}

TEST(IntlCalendar, GetBcp47Type)
{
  auto calendar =
      Calendar::TryCreate("en-US", Some(MakeStringSpan(u"GMT+3"))).unwrap();
  ASSERT_STREQ(calendar->GetBcp47Type().unwrap(), "gregory");
}

TEST(IntlCalendar, SetTimeInMs)
{
  auto calendar =
      Calendar::TryCreate("en-US", Some(MakeStringSpan(u"GMT+3"))).unwrap();

  // There is no way to verify the results. Unwrap the results to ensure it
  // doesn't fail, but don't check the values.
  calendar->SetTimeInMs(CALENDAR_DATE).unwrap();
}

TEST(IntlCalendar, CloneFrom)
{
  DateTimeFormat::StyleBag style;
  style.date = Some(DateTimeFormat::Style::Medium);
  style.time = Some(DateTimeFormat::Style::Medium);
  auto dtFormat = DateTimeFormat::TryCreateFromStyle(
                      MakeStringSpan("en-US"), style,
                      // It's ok to pass nullptr here, as it will cause format
                      // operations to fail, but this test is only checking
                      // calendar cloning.
                      nullptr, Some(MakeStringSpan(u"America/Chicago")))
                      .unwrap();

  dtFormat->CloneCalendar(CALENDAR_DATE).unwrap();
}

}  // namespace mozilla::intl
