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

// These tests are dependent on the machine that this test is being run on.
// Unwrap the results to ensure it doesn't fail, but don't check the values.
TEST(IntlTimeZone, SystemDependentTests)
{
  auto timeZone = TimeZone::TryCreate(Some(MakeStringSpan(u"GMT+3"))).unwrap();
  TestBuffer<char16_t> buffer;
  // e.g. For America/Chicago: 1000 * 60 * 60 * -6
  timeZone->GetRawOffsetMs().unwrap();

  // e.g. "America/Chicago"
  TimeZone::GetDefaultTimeZone(buffer).unwrap();
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
