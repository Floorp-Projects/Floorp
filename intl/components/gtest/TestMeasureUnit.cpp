/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/MeasureUnit.h"
#include "mozilla/Span.h"

namespace mozilla::intl {

TEST(IntlMeasureUnit, GetAvailable)
{
  auto units = MeasureUnit::GetAvailable();
  ASSERT_TRUE(units.isOk());

  // Test a subset of the installed measurement units.
  auto gigabyte = MakeStringSpan("gigabyte");
  auto liter = MakeStringSpan("liter");
  auto meter = MakeStringSpan("meter");
  auto meters = MakeStringSpan("meters");  // Plural "meters" is invalid.

  bool hasGigabyte = false;
  bool hasLiter = false;
  bool hasMeter = false;
  bool hasMeters = false;

  for (auto unit : units.unwrap()) {
    ASSERT_TRUE(unit.isOk());
    auto span = unit.unwrap();

    hasGigabyte |= span == gigabyte;
    hasLiter |= span == liter;
    hasMeter |= span == meter;
    hasMeters |= span == meters;
  }

  ASSERT_TRUE(hasGigabyte);
  ASSERT_TRUE(hasLiter);
  ASSERT_TRUE(hasMeter);
  ASSERT_FALSE(hasMeters);
}

}  // namespace mozilla::intl
