/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsString.h"

namespace testing::internal {

GTEST_API_ ::testing::AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                                     const char* s2_expression,
                                                     const nsAString& s1,
                                                     const nsAString& s2) {
  if (s1.Equals(s2)) {
    return ::testing::AssertionSuccess();
  }

  return ::testing::internal::EqFailure(
      s1_expression, s2_expression,
      std::string(NS_ConvertUTF16toUTF8(s1).get()),
      std::string(NS_ConvertUTF16toUTF8(s2).get()),
      /* ignore case */ false);
}

}  // namespace testing::internal
