// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WinUtilTest, EnsureRectIsVisibleInRect) {
  gfx::Rect parent_rect(0, 0, 500, 400);

  {
    // Child rect x < 0
    gfx::Rect child_rect(-50, 20, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(10, 20, 100, 100), child_rect);
  }

  {
    // Child rect y < 0
    gfx::Rect child_rect(20, -50, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 10, 100, 100), child_rect);
  }

  {
    // Child rect right > parent_rect.right
    gfx::Rect child_rect(450, 20, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(390, 20, 100, 100), child_rect);
  }

  {
    // Child rect bottom > parent_rect.bottom
    gfx::Rect child_rect(20, 350, 100, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 290, 100, 100), child_rect);
  }

  {
    // Child rect width > parent_rect.width
    gfx::Rect child_rect(20, 20, 700, 100);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 480, 100), child_rect);
  }

  {
    // Child rect height > parent_rect.height
    gfx::Rect child_rect(20, 20, 100, 700);
    win_util::EnsureRectIsVisibleInRect(parent_rect, &child_rect, 10);
    EXPECT_EQ(gfx::Rect(20, 20, 100, 380), child_rect);
  }
}

static const struct filename_case {
  const wchar_t* filename;
  const wchar_t* filter_selected;
  const wchar_t* suggested_ext;
  const wchar_t* result;
} filename_cases[] = {
  // Test a specific filter (*.jpg).
  {L"f",         L"*.jpg", L"jpg", L"f.jpg"},
  {L"f.",        L"*.jpg", L"jpg", L"f..jpg"},
  {L"f..",       L"*.jpg", L"jpg", L"f...jpg"},
  {L"f.jpeg",    L"*.jpg", L"jpg", L"f.jpeg"},
  // Further guarantees.
  {L"f.jpg.jpg", L"*.jpg", L"jpg", L"f.jpg.jpg"},
  {L"f.exe.jpg", L"*.jpg", L"jpg", L"f.exe.jpg"},
  {L"f.jpg.exe", L"*.jpg", L"jpg", L"f.jpg.exe.jpg"},
  {L"f.exe..",   L"*.jpg", L"jpg", L"f.exe...jpg"},
  {L"f.jpg..",   L"*.jpg", L"jpg", L"f.jpg...jpg"},
  // Test the All Files filter (*.jpg).
  {L"f",         L"*.*",   L"jpg", L"f"},
  {L"f.",        L"*.*",   L"jpg", L"f"},
  {L"f..",       L"*.*",   L"jpg", L"f"},
  {L"f.jpg",     L"*.*",   L"jpg", L"f.jpg"},
  {L"f.jpeg",    L"*.*",   L"jpg", L"f.jpeg"},  // Same MIME type (diff. ext).
  // Test the empty filter, which should behave identically to the
  // All Files filter.
  {L"f",         L"",      L"jpg", L"f"},
  {L"f.",        L"",      L"jpg", L"f"},
  {L"f..",       L"",      L"jpg", L"f"},
  {L"f.jpg",     L"",      L"jpg", L"f.jpg"},
  {L"f.jpeg",    L"",      L"jpg", L"f.jpeg"},
};

TEST(WinUtilTest, AppendingExtensions) {
  for (unsigned int i = 0; i < arraysize(filename_cases); ++i) {
    const filename_case& value = filename_cases[i];
    std::wstring result =
        win_util::AppendExtensionIfNeeded(value.filename, value.filter_selected,
                                          value.suggested_ext);
    EXPECT_EQ(value.result, result);
  }
}
