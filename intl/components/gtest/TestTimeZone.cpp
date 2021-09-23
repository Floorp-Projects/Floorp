/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/TimeZone.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "TestBuffer.h"

#include <string>

namespace mozilla::intl {

// Firefox 1.0 release date.
static constexpr int64_t RELEASE_DATE = 1'032'800'850'000;

// Date.UTC(2021, 11-1, 7, 2, 0, 0)
static constexpr int64_t DST_CHANGE_DATE = 1'636'250'400'000;

// These tests are dependent on the machine that this test is being run on.
// Unwrap the results to ensure it doesn't fail, but don't check the values.
TEST(IntlTimeZone, SystemDependentTests)
{
  // e.g. "America/Chicago"
  TestBuffer<char16_t> buffer;
  TimeZone::GetDefaultTimeZone(buffer).unwrap();
}

TEST(IntlTimeZone, GetRawOffsetMs)
{
  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetRawOffsetMs().unwrap(), 3 * 60 * 60 * 1000);

  timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"Etc/GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetRawOffsetMs().unwrap(), -(3 * 60 * 60 * 1000));

  timeZone =
      TimeZone::TryCreate(Some(MakeStringSpan(u"America/New_York"))).unwrap();
  ASSERT_EQ(timeZone->GetRawOffsetMs().unwrap(), -(5 * 60 * 60 * 1000));
}

TEST(IntlTimeZone, GetDSTOffsetMs)
{
  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetDSTOffsetMs(0).unwrap(), 0);

  timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"Etc/GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetDSTOffsetMs(0).unwrap(), 0);

  timeZone =
      TimeZone::TryCreate(Some(MakeStringSpan(u"America/New_York"))).unwrap();
  ASSERT_EQ(timeZone->GetDSTOffsetMs(0).unwrap(), 0);
  ASSERT_EQ(timeZone->GetDSTOffsetMs(RELEASE_DATE).unwrap(),
            1 * 60 * 60 * 1000);
  ASSERT_EQ(timeZone->GetDSTOffsetMs(DST_CHANGE_DATE).unwrap(),
            1 * 60 * 60 * 1000);
}

TEST(IntlTimeZone, GetOffsetMs)
{
  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetOffsetMs(0).unwrap(), 3 * 60 * 60 * 1000);

  timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"Etc/GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetOffsetMs(0).unwrap(), -(3 * 60 * 60 * 1000));

  timeZone =
      TimeZone::TryCreate(Some(MakeStringSpan(u"America/New_York"))).unwrap();
  ASSERT_EQ(timeZone->GetOffsetMs(0).unwrap(), -(5 * 60 * 60 * 1000));
  ASSERT_EQ(timeZone->GetOffsetMs(RELEASE_DATE).unwrap(),
            -(4 * 60 * 60 * 1000));
  ASSERT_EQ(timeZone->GetOffsetMs(DST_CHANGE_DATE).unwrap(),
            -(4 * 60 * 60 * 1000));
}

TEST(IntlTimeZone, GetUTCOffsetMs)
{
  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetUTCOffsetMs(0).unwrap(), 3 * 60 * 60 * 1000);

  timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"Etc/GMT+3"))).unwrap();
  ASSERT_EQ(timeZone->GetUTCOffsetMs(0).unwrap(), -(3 * 60 * 60 * 1000));

  timeZone =
      TimeZone::TryCreate(Some(MakeStringSpan(u"America/New_York"))).unwrap();
  ASSERT_EQ(timeZone->GetUTCOffsetMs(0).unwrap(), -(5 * 60 * 60 * 1000));
  ASSERT_EQ(timeZone->GetUTCOffsetMs(RELEASE_DATE).unwrap(),
            -(4 * 60 * 60 * 1000));
  ASSERT_EQ(timeZone->GetUTCOffsetMs(DST_CHANGE_DATE).unwrap(),
            -(5 * 60 * 60 * 1000));
}

TEST(IntlTimeZone, GetDisplayName)
{
  using DaylightSavings = TimeZone::DaylightSavings;

  TestBuffer<char16_t> buffer;

  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::No, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"GMT+03:00");

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::Yes, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"GMT+03:00");

  timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"Etc/GMT+3"))).unwrap();

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::No, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"GMT-03:00");

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::Yes, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"GMT-03:00");

  timeZone =
      TimeZone::TryCreate(Some(MakeStringSpan(u"America/New_York"))).unwrap();

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::No, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"Eastern Standard Time");

  buffer.clear();
  timeZone->GetDisplayName("en", DaylightSavings::Yes, buffer).unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"Eastern Daylight Time");
}

TEST(IntlTimeZone, GetCanonicalTimeZoneID)
{
  TestBuffer<char16_t> buffer;

  // Providing a canonical time zone results in the same string at the end.
  TimeZone::GetCanonicalTimeZoneID(MakeStringSpan(u"America/Chicago"), buffer)
      .unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"America/Chicago");

  // Providing an alias will result in the canonical representation.
  TimeZone::GetCanonicalTimeZoneID(MakeStringSpan(u"Europe/Belfast"), buffer)
      .unwrap();
  ASSERT_EQ(buffer.get_string_view(), u"Europe/London");

  // An unknown time zone results in an error.
  ASSERT_TRUE(TimeZone::GetCanonicalTimeZoneID(
                  MakeStringSpan(u"Not a time zone"), buffer)
                  .isErr());
}

TEST(IntlTimeZone, GetAvailableTimeZones)
{
  constexpr auto EuropeBerlin = MakeStringSpan("Europe/Berlin");
  constexpr auto EuropeBusingen = MakeStringSpan("Europe/Busingen");

  auto timeZones = TimeZone::GetAvailableTimeZones("DE").unwrap();

  bool hasEuropeBerlin = false;
  bool hasEuropeBusingen = false;

  for (auto timeZone : timeZones) {
    auto span = timeZone.unwrap();
    if (span == EuropeBerlin) {
      ASSERT_FALSE(hasEuropeBerlin);
      hasEuropeBerlin = true;
    } else if (span == EuropeBusingen) {
      ASSERT_FALSE(hasEuropeBusingen);
      hasEuropeBusingen = true;
    } else {
      std::string str(span.data(), span.size());
      ADD_FAILURE() << "Unexpected time zone: " << str;
    }
  }

  ASSERT_TRUE(hasEuropeBerlin);
  ASSERT_TRUE(hasEuropeBusingen);
}

}  // namespace mozilla::intl
