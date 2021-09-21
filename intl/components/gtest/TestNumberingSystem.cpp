/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/NumberingSystem.h"
#include "mozilla/Span.h"

namespace mozilla::intl {

TEST(IntlNumberingSystem, GetName)
{
  auto numbers_en = NumberingSystem::TryCreate("en").unwrap();
  ASSERT_EQ(numbers_en->GetName().unwrap(), MakeStringSpan("latn"));

  auto numbers_ar = NumberingSystem::TryCreate("ar").unwrap();
  ASSERT_EQ(numbers_ar->GetName().unwrap(), MakeStringSpan("arab"));

  auto numbers_ff_Adlm = NumberingSystem::TryCreate("ff-Adlm").unwrap();
  ASSERT_EQ(numbers_ff_Adlm->GetName().unwrap(), MakeStringSpan("adlm"));
}

}  // namespace mozilla::intl
