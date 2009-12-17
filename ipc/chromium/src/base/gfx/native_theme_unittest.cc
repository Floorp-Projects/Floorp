// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/native_theme.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(NativeThemeTest, Init) {
  ASSERT_TRUE(gfx::NativeTheme::instance() != NULL);
}
