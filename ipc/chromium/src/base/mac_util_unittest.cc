// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac_util.h"

#include <ApplicationServices/ApplicationServices.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest MacUtilTest;

TEST_F(MacUtilTest, TestFSRef) {
  FSRef ref;
  std::string path("/System/Library");

  ASSERT_TRUE(mac_util::FSRefFromPath(path, &ref));
  EXPECT_EQ(path, mac_util::PathFromFSRef(ref));
}
