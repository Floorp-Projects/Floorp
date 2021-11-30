/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_TEST_GTEST_COMMON_H_
#define DOM_QUOTA_TEST_GTEST_COMMON_H_

#include <cstdint>
#include "gtest/gtest.h"
#include "mozilla/dom/quota/Config.h"

namespace mozilla::dom::quota {

class DOM_Quota_Test : public testing::Test {
#ifdef QM_ERROR_STACKS_ENABLED
 public:
  static void SetUpTestCase();

  static void IncreaseExpectedStackId();

  static uint64_t ExpectedStackId();

 private:
  static uint64_t sExpectedStackId;
#endif
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_TEST_GTEST_COMMON_H_
