// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test RectTest;

TEST(RectTest, Contains) {
  static const struct ContainsCase {
    int rect_x;
    int rect_y;
    int rect_width;
    int rect_height;
    int point_x;
    int point_y;
    bool contained;
  } contains_cases[] = {
    {0, 0, 10, 10, 0, 0, true},
    {0, 0, 10, 10, 5, 5, true},
    {0, 0, 10, 10, 9, 9, true},
    {0, 0, 10, 10, 5, 10, false},
    {0, 0, 10, 10, 10, 5, false},
    {0, 0, 10, 10, -1, -1, false},
    {0, 0, 10, 10, 50, 50, false},
  #ifdef NDEBUG
    {0, 0, -10, -10, 0, 0, false},
  #endif  // NDEBUG
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(contains_cases); ++i) {
    const ContainsCase& value = contains_cases[i];
    gfx::Rect rect(value.rect_x, value.rect_y,
                   value.rect_width, value.rect_height);
    EXPECT_EQ(value.contained, rect.Contains(value.point_x, value.point_y));
  }
}

TEST(RectTest, Intersects) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    bool intersects;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, false },
    { 0, 0, 10, 10, 0, 0, 10, 10, true },
    { 0, 0, 10, 10, 10, 10, 10, 10, false },
    { 10, 10, 10, 10, 0, 0, 10, 10, false },
    { 10, 10, 10, 10, 5, 5, 10, 10, true },
    { 10, 10, 10, 10, 15, 15, 10, 10, true },
    { 10, 10, 10, 10, 20, 15, 10, 10, false },
    { 10, 10, 10, 10, 21, 15, 10, 10, false }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    EXPECT_EQ(tests[i].intersects, r1.Intersects(r2));
  }
}

TEST(RectTest, Intersect) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: the union of rects 1 and 2
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 0, 0,   // zeros
      0, 0, 0, 0,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // equal
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 0, 0, 4, 4,   // neighboring
      4, 4, 4, 4,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // overlapping corners
      2, 2, 4, 4,
      2, 2, 2, 2 },
    { 0, 0, 4, 4,   // T junction
      3, 1, 4, 2,
      3, 1, 1, 2 },
    { 3, 0, 2, 2,   // gap
      0, 0, 2, 2,
      0, 0, 0, 0 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect ir = r1.Intersect(r2);
    EXPECT_EQ(r3.x(), ir.x());
    EXPECT_EQ(r3.y(), ir.y());
    EXPECT_EQ(r3.width(), ir.width());
    EXPECT_EQ(r3.height(), ir.height());
  }
}

TEST(RectTest, Union) {
  static const struct Test {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: the union of rects 1 and 2
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 0, 0, 4, 4,
      4, 4, 4, 4,
      0, 0, 8, 8 },
    { 0, 0, 4, 4,
      0, 5, 4, 4,
      0, 0, 4, 9 },
    { 0, 0, 2, 2,
      3, 3, 2, 2,
      0, 0, 5, 5 },
    { 3, 3, 2, 2,   // reverse r1 and r2 from previous test
      0, 0, 2, 2,
      0, 0, 5, 5 },
    { 0, 0, 0, 0,   // union with empty rect
      2, 2, 2, 2,
      2, 2, 2, 2 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect u = r1.Union(r2);
    EXPECT_EQ(r3.x(), u.x());
    EXPECT_EQ(r3.y(), u.y());
    EXPECT_EQ(r3.width(), u.width());
    EXPECT_EQ(r3.height(), u.height());
  }
}

TEST(RectTest, Equals) {
  ASSERT_TRUE(gfx::Rect(0, 0, 0, 0).Equals(gfx::Rect(0, 0, 0, 0)));
  ASSERT_TRUE(gfx::Rect(1, 2, 3, 4).Equals(gfx::Rect(1, 2, 3, 4)));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0).Equals(gfx::Rect(0, 0, 0, 1)));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0).Equals(gfx::Rect(0, 0, 1, 0)));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0).Equals(gfx::Rect(0, 1, 0, 0)));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0).Equals(gfx::Rect(1, 0, 0, 0)));
}

TEST(RectTest, AdjustToFit) {
  static const struct Test {
    int x1;  // source
    int y1;
    int w1;
    int h1;
    int x2;  // target
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: results of invoking AdjustToFit
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 2, 2,
      0, 0, 2, 2,
      0, 0, 2, 2 },
    { 2, 2, 3, 3,
      0, 0, 4, 4,
      1, 1, 3, 3 },
    { -1, -1, 5, 5,
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 2, 2, 4, 4,
      0, 0, 3, 3,
      0, 0, 3, 3 },
    { 2, 2, 1, 1,
      0, 0, 3, 3,
      2, 2, 1, 1 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect u(r1.AdjustToFit(r2));
    EXPECT_EQ(r3.x(), u.x());
    EXPECT_EQ(r3.y(), u.y());
    EXPECT_EQ(r3.width(), u.width());
    EXPECT_EQ(r3.height(), u.height());
  }
}

TEST(RectTest, Subtract) {
  // Matching
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(10, 10, 20, 20)).Equals(
      gfx::Rect(0, 0, 0, 0)));

  // Contains
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(5, 5, 30, 30)).Equals(
      gfx::Rect(0, 0, 0, 0)));

  // No intersection
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(30, 30, 20, 20)).Equals(
      gfx::Rect(10, 10, 20, 20)));

  // Not a complete intersection in either direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(15, 15, 20, 20)).Equals(
      gfx::Rect(10, 10, 20, 20)));

  // Complete intersection in the x-direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(10, 15, 20, 20)).Equals(
      gfx::Rect(10, 10, 20, 5)));

  // Complete intersection in the x-direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(5, 15, 30, 20)).Equals(
      gfx::Rect(10, 10, 20, 5)));

  // Complete intersection in the x-direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(5, 5, 30, 20)).Equals(
      gfx::Rect(10, 25, 20, 5)));

  // Complete intersection in the y-direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(10, 10, 10, 30)).Equals(
      gfx::Rect(20, 10, 10, 20)));

  // Complete intersection in the y-direction
  EXPECT_TRUE(
      gfx::Rect(10, 10, 20, 20).Subtract(
      gfx::Rect(5, 5, 20, 30)).Equals(
      gfx::Rect(25, 10, 5, 20)));
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 0).size().IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 10, 0).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 10, 0).size().IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 10).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 10).size().IsEmpty());
  EXPECT_FALSE(gfx::Rect(0, 0, 10, 10).IsEmpty());
  EXPECT_FALSE(gfx::Rect(0, 0, 10, 10).size().IsEmpty());
}
