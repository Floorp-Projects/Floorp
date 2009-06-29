// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/file_path.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/platform_test.h"

namespace {

// Returns true if PathService::Get returns true and sets the path parameter
// to non-empty for the given PathService::DirType enumeration value.
bool ReturnsValidPath(int dir_type) {
  FilePath path;
  bool result = PathService::Get(dir_type, &path);
  return result && !path.value().empty() && file_util::PathExists(path);
}

#if defined(OS_WIN)
// Function to test DIR_LOCAL_APP_DATA_LOW on Windows XP. Make sure it fails.
bool ReturnsInvalidPath(int dir_type) {
  std::wstring path;
  bool result = PathService::Get(base::DIR_LOCAL_APP_DATA_LOW, &path);
  return !result && path.empty();
}
#endif

}  // namespace

// On the Mac this winds up using some autoreleased objects, so we need to
// be a PlatformTest.
typedef PlatformTest PathServiceTest;

// Test that all PathService::Get calls return a value and a true result
// in the development environment.  (This test was created because a few
// later changes to Get broke the semantics of the function and yielded the
// correct value while returning false.)
TEST_F(PathServiceTest, Get) {
  for (int key = base::DIR_CURRENT; key < base::PATH_END; ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#ifdef OS_WIN
  for (int key = base::PATH_WIN_START + 1; key < base::PATH_WIN_END; ++key) {
    if (key == base::DIR_LOCAL_APP_DATA_LOW &&
        win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
      // DIR_LOCAL_APP_DATA_LOW is not supported prior Vista and is expected to
      // fail.
      EXPECT_TRUE(ReturnsInvalidPath(key)) << key;
    } else {
      EXPECT_TRUE(ReturnsValidPath(key)) << key;
    }
  }
#endif
}
