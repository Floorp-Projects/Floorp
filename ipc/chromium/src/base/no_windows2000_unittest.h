// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NO_WINDOWS2000_UNITTEST_H_
#define BASE_NO_WINDOWS2000_UNITTEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "base/win_util.h"

// Disable the whole test case when executing on Windows 2000 or lower.
// Note: Parent should be testing::Test or UITest.
template<typename Parent>
class NoWindows2000Test : public Parent {
 public:
  static bool IsTestCaseDisabled() {
    return win_util::GetWinVersion() <= win_util::WINVERSION_2000;
  }
};

#endif  // BASE_NO_WINDOWS2000_UNITTEST_H_
