/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Locale.h"

#include <stdint.h>
#include <string_view>

namespace mozilla::intl {

// These tests are dependent on the machine that this test is being run on.
TEST(IntlLocale, SystemDependentTests)
{
  // e.g. "en_US"
  const char* locale = Locale::GetDefaultLocale();
  ASSERT_TRUE(locale != nullptr);
}

TEST(IntlLocale, GetAvailableLocales)
{
  using namespace std::literals;

  int32_t english = 0;
  int32_t german = 0;
  int32_t chinese = 0;

  // Since this list is dependent on ICU, and may change between upgrades, only
  // test a subset of the available locales.
  for (const char* locale : Locale::GetAvailableLocales()) {
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

}  // namespace mozilla::intl
