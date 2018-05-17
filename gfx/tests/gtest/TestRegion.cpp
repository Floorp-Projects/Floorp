/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "gtest/gtest.h"
#include "nsRegion.h"
#include "RegionBuilder.h"
#include "mozilla/gfx/TiledRegion.h"
#include "mozilla/UniquePtr.h"

using namespace std;
using namespace mozilla::gfx;

//#define REGION_RANDOM_STRESS_TESTS

class TestLargestRegion {
public:
  static void TestSingleRect(nsRect r) {
    nsRegion region(r);
    EXPECT_TRUE(region.GetLargestRectangle().IsEqualInterior(r));
  }
  // Construct a rectangle, remove part of it, then check the remainder
  static void TestNonRectangular() {
    nsRegion r(nsRect(0, 0, 30, 30));

    const int nTests = 19;
    struct {
      nsRect rect;
      int64_t expectedArea;
    } tests[nTests] = {
      // Remove a 20x10 chunk from the square
      { nsRect(0, 0, 20, 10), 600 },
      { nsRect(10, 0, 20, 10), 600 },
      { nsRect(10, 20, 20, 10), 600 },
      { nsRect(0, 20, 20, 10), 600 },
      // Remove a 10x20 chunk from the square
      { nsRect(0, 0, 10, 20), 600 },
      { nsRect(20, 0, 10, 20), 600 },
      { nsRect(20, 10, 10, 20), 600 },
      { nsRect(0, 10, 10, 20), 600 },
      // Remove the center 10x10
      { nsRect(10, 10, 10, 10), 300 },
      // Remove the middle column
      { nsRect(10, 0, 10, 30), 300 },
      // Remove the middle row
      { nsRect(0, 10, 30, 10), 300 },
      // Remove the corners 10x10
      { nsRect(0, 0, 10, 10), 600 },
      { nsRect(20, 20, 10, 10), 600 },
      { nsRect(20, 0, 10, 10), 600 },
      { nsRect(0, 20, 10, 10), 600 },
      // Remove the corners 20x20
      { nsRect(0, 0, 20, 20), 300 },
      { nsRect(10, 10, 20, 20), 300 },
      { nsRect(10, 0, 20, 20), 300 },
      { nsRect(0, 10, 20, 20), 300 }
    };

    for (int32_t i = 0; i < nTests; i++) {
      nsRegion r2;
      r2.Sub(r, tests[i].rect);

      EXPECT_TRUE(r2.IsComplex()) << "nsRegion code got unexpectedly smarter!";

      nsRect largest = r2.GetLargestRectangle();
      EXPECT_TRUE(largest.Width() * largest.Height() == tests[i].expectedArea) <<
        "Did not successfully find largest rectangle in non-rectangular region on iteration " << i;
    }

  }
  static void TwoRectTest() {
    nsRegion r(nsRect(0, 0, 100, 100));
    const int nTests = 4;
    struct {
      nsRect rect1, rect2;
      int64_t expectedArea;
    } tests[nTests] = {
      { nsRect(0, 0, 75, 40),  nsRect(0, 60, 75, 40),  2500 },
      { nsRect(25, 0, 75, 40), nsRect(25, 60, 75, 40), 2500 },
      { nsRect(25, 0, 75, 40), nsRect(0, 60, 75, 40),  2000 },
      { nsRect(0, 0, 75, 40),  nsRect(25, 60, 75, 40), 2000 },
    };
    for (int32_t i = 0; i < nTests; i++) {
      nsRegion r2;

      r2.Sub(r, tests[i].rect1);
      r2.Sub(r2, tests[i].rect2);

      EXPECT_TRUE(r2.IsComplex()) << "nsRegion code got unexpectedly smarter!";

      nsRect largest = r2.GetLargestRectangle();
      EXPECT_TRUE(largest.Width() * largest.Height() == tests[i].expectedArea) <<
        "Did not successfully find largest rectangle in two-rect-subtract region on iteration " << i;
    }
  }
  static void TestContainsSpecifiedRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    EXPECT_TRUE(r.GetLargestRectangle(nsRect(0, 300, 10, 10)).IsEqualInterior(nsRect(0, 300, 50, 50))) <<
      "Chose wrong rectangle";
  }
  static void TestContainsSpecifiedOverflowingRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    EXPECT_TRUE(r.GetLargestRectangle(nsRect(0, 290, 10, 20)).IsEqualInterior(nsRect(0, 300, 50, 50))) <<
      "Chose wrong rectangle";
  }
};

TEST(Gfx, RegionSingleRect) {
  TestLargestRegion::TestSingleRect(nsRect(0, 52, 720, 480));
  TestLargestRegion::TestSingleRect(nsRect(-20, 40, 50, 20));
  TestLargestRegion::TestSingleRect(nsRect(-20, 40, 10, 8));
  TestLargestRegion::TestSingleRect(nsRect(-20, -40, 10, 8));
  TestLargestRegion::TestSingleRect(nsRect(-10, -10, 20, 20));
}

TEST(Gfx, RegionNonRectangular) {
  TestLargestRegion::TestNonRectangular();
}

TEST(Gfx, RegionTwoRectTest) {
  TestLargestRegion::TwoRectTest();
}

TEST(Gfx, RegionContainsSpecifiedRect) {
  TestLargestRegion::TestContainsSpecifiedRect();
}

TEST(Gfx, RegionTestContainsSpecifiedOverflowingRect) {
  TestLargestRegion::TestContainsSpecifiedOverflowingRect();
}

TEST(Gfx, RegionScaleToInside) {
  { // no rectangles
    nsRegion r;

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result;

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }

  { // one rectangle
    nsRegion r(nsRect(0,44760,19096,264));

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result(mozilla::gfx::IntRect(0,746,318,4));

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }


  { // the first rectangle gets adjusted
    nsRegion r(nsRect(0,44760,19096,264));
    r.Or(r, nsRect(0,45024,19360,1056));

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result(mozilla::gfx::IntRect(0,746,318,5));
    result.Or(result, mozilla::gfx::IntRect(0,751,322,17));

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }

  { // the second rectangle gets adjusted
    nsRegion r(nsRect(0,44760,19360,264));
    r.Or(r, nsRect(0,45024,19096,1056));

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result(mozilla::gfx::IntRect(0,746,322,4));
    result.Or(result, mozilla::gfx::IntRect(0,750,318,18));

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }

}

TEST(Gfx, RegionIsEqual)
{
  {
    nsRegion r(nsRect(0, 0, 50, 50));
    EXPECT_FALSE(nsRegion().IsEqual(r));
  }
  {
    nsRegion r1(nsRect(0, 0, 50, 50));
    nsRegion r2(nsRect(0, 0, 50, 50));
    EXPECT_TRUE(r1.IsEqual(r2));
  }
  {
    nsRegion r1(nsRect(0, 0, 50, 50));
    nsRegion r2(nsRect(0, 0, 60, 50));
    EXPECT_FALSE(r1.IsEqual(r2));
  }
  {
    nsRegion r1(nsRect(0, 0, 50, 50));
    r1.OrWith(nsRect(0, 60, 50, 50));
    nsRegion r2(nsRect(0, 0, 50, 50));
    r2.OrWith(nsRect(0, 60, 50, 50));
    EXPECT_TRUE(r1.IsEqual(r2));
  }
  {
    nsRegion r1(nsRect(0, 0, 50, 50));
    r1.OrWith(nsRect(0, 60, 50, 50));
    nsRegion r2(nsRect(0, 0, 50, 50));
    r2.OrWith(nsRect(0, 70, 50, 50));
    EXPECT_FALSE(r1.IsEqual(r2));
  }
  {
    nsRegion r1(nsRect(0, 0, 50, 50));
    r1.OrWith(nsRect(0, 60, 50, 50));
    r1.OrWith(nsRect(100, 60, 50, 50));
    nsRegion r2(nsRect(0, 0, 50, 50));
    r2.OrWith(nsRect(0, 60, 50, 50));
    EXPECT_FALSE(r1.IsEqual(r2));
  }
}

TEST(Gfx, RegionOrWith) {
  {
    nsRegion r(nsRect(79, 31, 75, 12));
    r.OrWith(nsRect(22, 43, 132, 5));
    r.OrWith(nsRect(22, 48, 125, 3));
    r.OrWith(nsRect(22, 51, 96, 20));
    r.OrWith(nsRect(34, 71, 1, 14));
    r.OrWith(nsRect(26, 85, 53, 1));
    r.OrWith(nsRect(26, 86, 53, 4));
    r.OrWith(nsRect(96, 86, 30, 4));
    r.OrWith(nsRect(34, 90, 1, 2));
    r.OrWith(nsRect(96, 90, 30, 2));
    r.OrWith(nsRect(34, 92, 1, 3));
    r.OrWith(nsRect(49, 92, 34, 3));
    r.OrWith(nsRect(96, 92, 30, 3));
    r.OrWith(nsRect(34, 95, 1, 17));
    r.OrWith(nsRect(49, 95, 77, 17));
    r.OrWith(nsRect(34, 112, 1, 12));
    r.OrWith(nsRect(75, 112, 51, 12));
    r.OrWith(nsRect(34, 124, 1, 10));
    r.OrWith(nsRect(75, 124, 44, 10));
    r.OrWith(nsRect(34, 134, 1, 19));
    r.OrWith(nsRect(22, 17, 96, 27));
  }
  {
    nsRegion r(nsRect(0, 8, 257, 32));
    r.OrWith(nsRect(3702, 8, 138, 32));
    r.OrWith(nsRect(0, 40, 225, 1));
    r.OrWith(nsRect(3702, 40, 138, 1));
    r.OrWith(nsRect(0, 41, 101, 40));
    r.OrWith(nsRect(69, 41, 32, 40));
  }
  {
    nsRegion r(nsRect(79, 56, 8, 32));
    r.OrWith(nsRect(5, 94, 23, 81));
    r.OrWith(nsRect(56, 29, 91, 81));
  }
  {
    nsRegion r(nsRect(0, 82, 3840, 2046));
    r.OrWith(nsRect(0, 0, 3840, 82));
  }
  {
    nsRegion r(nsRect(2, 5, 600, 28));
    r.OrWith(nsRect(2, 82, 600, 19));
    r.OrWith(nsRect(2, 33, 600, 49));
  }
  {
    nsRegion r(nsRect(3823, 0, 17, 17));
    r.OrWith(nsRect(3823, 2029, 17, 17));
    r.OrWith(nsRect(3823, 0, 17, 2046));
  }
  {
    nsRegion r(nsRect(1036, 4, 32, 21));
    r.OrWith(nsRect(1070, 4, 66, 21));
    r.OrWith(nsRect(40, 5, 0, 33));
  }
  {
    nsRegion r(nsRect(0, 0, 1024, 1152));
    r.OrWith(nsRect(-335802, -1073741824, 1318851, 1860043520));
  }
  {
    nsRegion r(nsRect(0, 0, 800, 1000));
    r.OrWith(nsRect(0, 0, 536870912, 1073741824));
  }
  {
    nsRegion r(nsRect(53, 2, 52, 3));
    r.OrWith(nsRect(45, 5, 60, 16));
    r.OrWith(nsRect(16, 21, 8, 1));
    r.OrWith(nsRect(45, 21, 12, 1));
    r.OrWith(nsRect(16, 22, 8, 5));
    r.OrWith(nsRect(33, 22, 52, 5));
    r.OrWith(nsRect(16, 27, 8, 7));
    r.OrWith(nsRect(33, 27, 66, 7));
    r.OrWith(nsRect(0, 34, 99, 1));
    r.OrWith(nsRect(0, 35, 159, 27));
    r.OrWith(nsRect(0, 62, 122, 3));
    r.OrWith(nsRect(0, 65, 85, 11));
    r.OrWith(nsRect(91, 65, 97, 11));
    r.OrWith(nsRect(11, 76, 74, 2));
    r.OrWith(nsRect(91, 76, 97, 2));
    r.OrWith(nsRect(11, 78, 74, 12));
    r.OrWith(nsRect(11, 90, 13, 3));
    r.OrWith(nsRect(33, 90, 108, 3));
    r.OrWith(nsRect(16, 93, 8, 22));
    r.OrWith(nsRect(33, 93, 108, 22));
    r.OrWith(nsRect(16, 115, 8, 1));
    r.OrWith(nsRect(58, 115, 83, 1));
    r.OrWith(nsRect(58, 116, 83, 25));
    r.OrWith(nsRect(59, 37, 88, 92));
  }
#ifdef REGION_RANDOM_STRESS_TESTS
  const uint32_t TestIterations = 100000;
  const uint32_t RectsPerTest = 100;

  nsRect rects[RectsPerTest];

  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r;
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      rects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    r.SetEmpty();
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      r.OrWith(rects[n]);
    }
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      EXPECT_TRUE(r.Contains(rects[n]));
    }
  }
#endif
}

TEST(Gfx, RegionSubWith) {
  {
    nsRegion r1(nsRect(0, 0, 100, 50));
    r1.OrWith(nsRect(50, 50, 50, 50));
    nsRegion r2(nsRect(0, 0, 100, 50));
    r2.OrWith(nsRect(50, 50, 50, 50));
    r1.SubWith(r2);
    EXPECT_FALSE(r1.Contains(1, 1));
  }
  {
    nsRegion r1(nsRect(0, 0, 800, 1000));
    nsRegion r2(nsRect(8, 108, 22, 20));
    r2.OrWith(nsRect(91, 138, 17, 18));
    r1.SubWith(r2);
    EXPECT_TRUE(r1.Contains(400, 130));
  }
  {
    nsRegion r1(nsRect(392, 2, 28, 7));
    r1.OrWith(nsRect(115, 9, 305, 16));
    r1.OrWith(nsRect(392, 25, 28, 5));
    r1.OrWith(nsRect(0, 32, 1280, 41));
    nsRegion r2(nsRect(0, 0, 1280, 9));
    r2.OrWith(nsRect(0, 9, 115, 16));
    r2.OrWith(nsRect(331, 9, 949, 16));
    r2.OrWith(nsRect(0, 25, 1280, 7));
    r2.OrWith(nsRect(331, 32, 124, 1));
    r1.SubWith(r2);
    EXPECT_FALSE(r1.Contains(350, 15));
  }
  {
    nsRegion r1(nsRect(552, 0, 2, 2));
    r1.OrWith(nsRect(362, 2, 222, 28));
    r1.OrWith(nsRect(552, 30, 2, 2));
    r1.OrWith(nsRect(0, 32, 1280, 41));
    nsRegion r2(nsRect(512, 0, 146, 9));
    r2.OrWith(nsRect(340, 9, 318, 16));
    r2.OrWith(nsRect(512, 25, 146, 8));
    r1.SubWith(r2);
    EXPECT_FALSE(r1.Contains(350, 15));
  }
  {
    nsRegion r(nsRect(0, 0, 229380, 6780));
    r.OrWith(nsRect(76800, 6780, 76800, 4440));
    r.OrWith(nsRect(76800, 11220, 44082, 1800));
    r.OrWith(nsRect(122682, 11220, 30918, 1800));
    r.OrWith(nsRect(76800, 13020, 76800, 2340));
    r.OrWith(nsRect(85020, 15360, 59340, 75520));
    r.OrWith(nsRect(85020, 90880, 38622, 11332));
    r.OrWith(nsRect(143789, 90880, 571, 11332));
    r.OrWith(nsRect(85020, 102212, 59340, 960));
    r.OrWith(nsRect(85020, 103172, 38622, 1560));
    r.OrWith(nsRect(143789, 103172, 571, 1560));
    r.OrWith(nsRect(85020, 104732, 59340, 12292));
    r.OrWith(nsRect(85020, 117024, 38622, 1560));
    r.OrWith(nsRect(143789, 117024, 571, 1560));
    r.OrWith(nsRect(85020, 118584, 59340, 11976));
    r.SubWith(nsRect(123642, 89320, 20147, 1560));
  }
  {
    nsRegion r(nsRect(0, 0, 9480, 12900));
    r.OrWith(nsRect(0, 12900, 8460, 1020));
    r.SubWith(nsRect(8460, 0, 1020, 12900));
  }
  {
    nsRegion r1(nsRect(99, 1, 51, 2));
    r1.OrWith(nsRect(85, 3, 65, 1));
    r1.OrWith(nsRect(10, 4, 66, 5));
    r1.OrWith(nsRect(85, 4, 37, 5));
    r1.OrWith(nsRect(10, 9, 112, 3));
    r1.OrWith(nsRect(1, 12, 121, 1));
    r1.OrWith(nsRect(1, 13, 139, 3));
    r1.OrWith(nsRect(0, 16, 140, 3));
    r1.OrWith(nsRect(0, 19, 146, 3));
    r1.OrWith(nsRect(0, 22, 149, 2));
    r1.OrWith(nsRect(0, 24, 154, 2));
    r1.OrWith(nsRect(0, 26, 160, 23));
    r1.OrWith(nsRect(0, 49, 162, 31));
    r1.OrWith(nsRect(0, 80, 171, 19));
    r1.OrWith(nsRect(0, 99, 173, 11));
    r1.OrWith(nsRect(2, 110, 171, 6));
    r1.OrWith(nsRect(6, 116, 165, 5));
    r1.OrWith(nsRect(8, 121, 163, 1));
    r1.OrWith(nsRect(13, 122, 158, 11));
    r1.OrWith(nsRect(14, 133, 157, 23));
    r1.OrWith(nsRect(29, 156, 142, 10));
    r1.OrWith(nsRect(37, 166, 134, 6));
    r1.OrWith(nsRect(55, 172, 4, 4));
    r1.OrWith(nsRect(83, 172, 88, 4));
    r1.OrWith(nsRect(55, 176, 4, 2));
    r1.OrWith(nsRect(89, 176, 6, 2));
    r1.OrWith(nsRect(89, 178, 6, 4));
    nsRegion r2(nsRect(63, 11, 39, 11));
    r2.OrWith(nsRect(63, 22, 99, 16));
    r2.OrWith(nsRect(37, 38, 125, 61));
    r2.OrWith(nsRect(45, 99, 117, 8));
    r2.OrWith(nsRect(47, 107, 115, 7));
    r2.OrWith(nsRect(47, 114, 66, 1));
    r2.OrWith(nsRect(49, 115, 64, 2));
    r2.OrWith(nsRect(49, 117, 54, 30));
    r1.SubWith(r2);
  }
  {
    nsRegion r1(nsRect(95, 2, 47, 1));
    r1.OrWith(nsRect(62, 3, 80, 2));
    r1.OrWith(nsRect(1, 5, 18, 3));
    r1.OrWith(nsRect(48, 5, 94, 3));
    r1.OrWith(nsRect(1, 8, 18, 3));
    r1.OrWith(nsRect(23, 8, 119, 3));
    r1.OrWith(nsRect(1, 11, 172, 9));
    r1.OrWith(nsRect(1, 20, 18, 8));
    r1.OrWith(nsRect(20, 20, 153, 8));
    r1.OrWith(nsRect(1, 28, 172, 13));
    r1.OrWith(nsRect(1, 41, 164, 1));
    r1.OrWith(nsRect(1, 42, 168, 1));
    r1.OrWith(nsRect(0, 43, 169, 15));
    r1.OrWith(nsRect(1, 58, 168, 26));
    r1.OrWith(nsRect(1, 84, 162, 2));
    r1.OrWith(nsRect(1, 86, 165, 23));
    r1.OrWith(nsRect(1, 109, 162, 23));
    r1.OrWith(nsRect(1, 132, 152, 4));
    r1.OrWith(nsRect(1, 136, 150, 12));
    r1.OrWith(nsRect(12, 148, 139, 4));
    r1.OrWith(nsRect(12, 152, 113, 2));
    r1.OrWith(nsRect(14, 154, 31, 3));
    r1.OrWith(nsRect(82, 154, 43, 3));
    r1.OrWith(nsRect(17, 157, 13, 19));
    r1.OrWith(nsRect(82, 157, 43, 19));
    r1.OrWith(nsRect(17, 176, 13, 16));
    nsRegion r2(nsRect(97, 9, 6, 10));
    r2.OrWith(nsRect(71, 19, 32, 2));
    r2.OrWith(nsRect(20, 21, 83, 2));
    r2.OrWith(nsRect(2, 23, 101, 9));
    r2.OrWith(nsRect(2, 32, 98, 1));
    r2.OrWith(nsRect(2, 33, 104, 5));
    r2.OrWith(nsRect(2, 38, 118, 2));
    r2.OrWith(nsRect(15, 40, 9, 11));
    r2.OrWith(nsRect(36, 40, 84, 11));
    r2.OrWith(nsRect(4, 51, 116, 33));
    r2.OrWith(nsRect(4, 84, 159, 8));
    r2.OrWith(nsRect(4, 92, 116, 13));
    r2.OrWith(nsRect(15, 105, 9, 7));
    r2.OrWith(nsRect(36, 105, 84, 7));
    r2.OrWith(nsRect(36, 112, 84, 22));
    r2.OrWith(nsRect(71, 134, 39, 46));
    r1.SubWith(r2);
  }
#ifdef REGION_RANDOM_STRESS_TESTS
  const uint32_t TestIterations = 100000;
  const uint32_t RectsPerTest = 100;
  const uint32_t SubRectsPerTest = 10;

  nsRect rects[RectsPerTest];
  nsRect subRects[SubRectsPerTest];

  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r;
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      rects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    r.SetEmpty();
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      r.OrWith(rects[n]);
    }
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      EXPECT_TRUE(r.Contains(rects[n]));
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      r.SubWith(subRects[n]);
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
  }
  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r;
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      rects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    r.SetEmpty();
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      r.OrWith(rects[n]);
    }
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      EXPECT_TRUE(r.Contains(rects[n]));
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    nsRegion r2;
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      r2.OrWith(subRects[n]);
    }
    r.SubWith(r2);

    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
  }
  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r(nsRect(-1, -1, 202, 202));
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
      r.SubWith(subRects[n]);
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
    EXPECT_TRUE(r.Contains(-1, -1));
    EXPECT_TRUE(r.Contains(-1, 200));
    EXPECT_TRUE(r.Contains(200, -1));
    EXPECT_TRUE(r.Contains(200, 200));
  }
  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r(nsRect(-1, -1, 202, 202));
    nsRegion r2;
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
      r2.OrWith(subRects[n]);
    }
    r.SubWith(r2);
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
    EXPECT_TRUE(r.Contains(-1, -1));
    EXPECT_TRUE(r.Contains(-1, 200));
    EXPECT_TRUE(r.Contains(200, -1));
    EXPECT_TRUE(r.Contains(200, 200));
  }
#endif
}
TEST(Gfx, RegionSub) {
  {
    nsRegion r1(nsRect(0, 0, 100, 50));
    r1.OrWith(nsRect(50, 50, 50, 50));
    nsRegion r2(nsRect(0, 0, 100, 50));
    r2.OrWith(nsRect(50, 50, 50, 50));
    nsRegion r3;
    r3.Sub(r1, r2);
    EXPECT_FALSE(r3.Contains(1, 1));
  }
  {
    nsRegion r1(nsRect(0, 0, 800, 1000));
    nsRegion r2(nsRect(8, 108, 22, 20));
    r2.OrWith(nsRect(91, 138, 17, 18));
    nsRegion r3;
    r3.Sub(r1, r2);
    EXPECT_TRUE(r3.Contains(400, 130));
  }
  {
    nsRegion r1(nsRect(392, 2, 28, 7));
    r1.OrWith(nsRect(115, 9, 305, 16));
    r1.OrWith(nsRect(392, 25, 28, 5));
    r1.OrWith(nsRect(0, 32, 1280, 41));
    nsRegion r2(nsRect(0, 0, 1280, 9));
    r2.OrWith(nsRect(0, 9, 115, 16));
    r2.OrWith(nsRect(331, 9, 949, 16));
    r2.OrWith(nsRect(0, 25, 1280, 7));
    r2.OrWith(nsRect(331, 32, 124, 1));
    nsRegion r3;
    r3.Sub(r1, r2);
    EXPECT_FALSE(r3.Contains(350, 15));
  }
  {
    nsRegion r1(nsRect(552, 0, 2, 2));
    r1.OrWith(nsRect(362, 2, 222, 28));
    r1.OrWith(nsRect(552, 30, 2, 2));
    r1.OrWith(nsRect(0, 32, 1280, 41));
    nsRegion r2(nsRect(512, 0, 146, 9));
    r2.OrWith(nsRect(340, 9, 318, 16));
    r2.OrWith(nsRect(512, 25, 146, 8));
    nsRegion r3;
    r3.Sub(r1, r2);
    EXPECT_FALSE(r3.Contains(350, 15));
  }
  {
    nsRegion r1(nsRect(0, 0, 1265, 1024));
    nsRegion r2(nsRect(1265, 0, 15, 685));
    r2.OrWith(nsRect(0, 714, 1280, 221));
    nsRegion r3;
    r3.Sub(r1, r2);
  }
  {
    nsRegion r1(nsRect(6, 0, 64, 1));
    r1.OrWith(nsRect(6, 1, 67, 1));
    r1.OrWith(nsRect(6, 2, 67, 2));
    r1.OrWith(nsRect(79, 2, 67, 2));
    r1.OrWith(nsRect(6, 4, 67, 1));
    r1.OrWith(nsRect(79, 4, 98, 1));
    r1.OrWith(nsRect(6, 5, 171, 18));
    r1.OrWith(nsRect(1, 23, 176, 3));
    r1.OrWith(nsRect(1, 26, 178, 5));
    r1.OrWith(nsRect(1, 31, 176, 9));
    r1.OrWith(nsRect(0, 40, 177, 57));
    r1.OrWith(nsRect(0, 97, 176, 33));
    r1.OrWith(nsRect(0, 130, 12, 17));
    r1.OrWith(nsRect(15, 130, 161, 17));
    r1.OrWith(nsRect(0, 147, 12, 5));
    r1.OrWith(nsRect(15, 147, 111, 5));
    r1.OrWith(nsRect(0, 152, 12, 7));
    r1.OrWith(nsRect(17, 152, 109, 7));
    r1.OrWith(nsRect(0, 159, 12, 2));
    r1.OrWith(nsRect(17, 159, 98, 2));
    r1.OrWith(nsRect(17, 161, 98, 9));
    r1.OrWith(nsRect(27, 170, 63, 21));
    nsRegion r2(nsRect(9, 9, 37, 17));
    r2.OrWith(nsRect(92, 9, 26, 17));
    r2.OrWith(nsRect(9, 26, 37, 9));
    r2.OrWith(nsRect(84, 26, 65, 9));
    r2.OrWith(nsRect(9, 35, 37, 2));
    r2.OrWith(nsRect(51, 35, 98, 2));
    r2.OrWith(nsRect(51, 37, 98, 11));
    r2.OrWith(nsRect(51, 48, 78, 4));
    r2.OrWith(nsRect(87, 52, 42, 7));
    r2.OrWith(nsRect(19, 59, 12, 5));
    r2.OrWith(nsRect(87, 59, 42, 5));
    r2.OrWith(nsRect(19, 64, 12, 9));
    r2.OrWith(nsRect(32, 64, 97, 9));
    r2.OrWith(nsRect(19, 73, 12, 2));
    r2.OrWith(nsRect(32, 73, 104, 2));
    r2.OrWith(nsRect(19, 75, 117, 5));
    r2.OrWith(nsRect(18, 80, 118, 5));
    r2.OrWith(nsRect(18, 85, 111, 38));
    r2.OrWith(nsRect(87, 123, 42, 11));
    nsRegion r3;
    r3.Sub(r1, r2);
  }
  {
    nsRegion r1(nsRect(27, 0, 39, 1));
    r1.OrWith(nsRect(86, 0, 22, 1));
    r1.OrWith(nsRect(27, 1, 43, 1));
    r1.OrWith(nsRect(86, 1, 22, 1));
    r1.OrWith(nsRect(27, 2, 43, 1));
    r1.OrWith(nsRect(86, 2, 75, 1));
    r1.OrWith(nsRect(12, 3, 58, 1));
    r1.OrWith(nsRect(86, 3, 75, 1));
    r1.OrWith(nsRect(12, 4, 149, 5));
    r1.OrWith(nsRect(0, 9, 161, 9));
    r1.OrWith(nsRect(0, 18, 167, 17));
    r1.OrWith(nsRect(0, 35, 171, 5));
    r1.OrWith(nsRect(0, 40, 189, 28));
    r1.OrWith(nsRect(0, 68, 171, 16));
    r1.OrWith(nsRect(4, 84, 167, 5));
    r1.OrWith(nsRect(4, 89, 177, 9));
    r1.OrWith(nsRect(1, 98, 180, 59));
    r1.OrWith(nsRect(4, 157, 177, 1));
    r1.OrWith(nsRect(4, 158, 139, 15));
    r1.OrWith(nsRect(17, 173, 126, 2));
    r1.OrWith(nsRect(20, 175, 123, 2));
    r1.OrWith(nsRect(20, 177, 118, 6));
    r1.OrWith(nsRect(20, 183, 84, 2));
    nsRegion r2(nsRect(64, 2, 30, 6));
    r2.OrWith(nsRect(26, 11, 41, 17));
    r2.OrWith(nsRect(19, 28, 48, 23));
    r2.OrWith(nsRect(19, 51, 76, 8));
    r2.OrWith(nsRect(4, 59, 91, 31));
    r2.OrWith(nsRect(19, 90, 76, 29));
    r2.OrWith(nsRect(33, 119, 62, 25));
    r2.OrWith(nsRect(33, 144, 4, 21));
    nsRegion r3;
    r3.Sub(r1, r2);
  }
#ifdef REGION_RANDOM_STRESS_TESTS
  const uint32_t TestIterations = 100000;
  const uint32_t RectsPerTest = 100;
  const uint32_t SubRectsPerTest = 10;

  nsRect rects[RectsPerTest];
  nsRect subRects[SubRectsPerTest];

  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r;
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      rects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    r.SetEmpty();
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      r.OrWith(rects[n]);
    }
    for (uint32_t n = 0; n < RectsPerTest; n++) {
      EXPECT_TRUE(r.Contains(rects[n]));
    }
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
    }
    nsRegion r2;
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      r2.OrWith(subRects[n]);
    }
    nsRegion r3;
    r3.Sub(r, r2);

    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r3.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r3.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r3.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r3.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
  }
  for (uint32_t i = 0; i < TestIterations; i++) {
    nsRegion r(nsRect(-1, -1, 202, 202));
    nsRegion r2;
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      subRects[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
      r2.OrWith(subRects[n]);
    }
    nsRegion r3;
    r3.Sub(r, r2);
    for (uint32_t n = 0; n < SubRectsPerTest; n++) {
      EXPECT_FALSE(r3.Contains(subRects[n].x, subRects[n].y));
      EXPECT_FALSE(r3.Contains(subRects[n].x, subRects[n].YMost() - 1));
      EXPECT_FALSE(r3.Contains(subRects[n].XMost() - 1, subRects[n].YMost() - 1));
      EXPECT_FALSE(r3.Contains(subRects[n].XMost() - 1, subRects[n].Y()));
    }
    EXPECT_TRUE(r3.Contains(-1, -1));
    EXPECT_TRUE(r3.Contains(-1, 200));
    EXPECT_TRUE(r3.Contains(200, -1));
    EXPECT_TRUE(r3.Contains(200, 200));
  }
#endif
}

TEST(Gfx, RegionAndWith) {
  {
    nsRegion r(nsRect(20, 0, 20, 20));
    r.OrWith(nsRect(0, 20, 40, 20));
    r.AndWith(nsRect(0, 0, 5, 5));
    EXPECT_FALSE(r.Contains(1, 1));
  }
  {
    nsRegion r1(nsRect(512, 1792, 256, 256));
    nsRegion r2(nsRect(17, 1860, 239, 35));
    r2.OrWith(nsRect(17, 1895, 239, 7));
    r2.OrWith(nsRect(768, 1895, 154, 7));
    r2.OrWith(nsRect(17, 1902, 905, 483));
    r1.AndWith(r2);
  }
#ifdef REGION_RANDOM_STRESS_TESTS
  const uint32_t TestIterations = 100000;
  const uint32_t RectsPerTest = 50;
  const uint32_t pointsTested = 100;

  {
    nsRect rectsSet1[RectsPerTest];
    nsRect rectsSet2[RectsPerTest];

    for (uint32_t i = 0; i < TestIterations; i++) {
      nsRegion r1;
      nsRegion r2;
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        rectsSet1[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
        rectsSet2[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
        r1.OrWith(rectsSet1[n]);
        r2.OrWith(rectsSet1[n]);
      }

      nsRegion r3 = r1;
      r3.AndWith(r2);

      for (uint32_t n = 0; n < pointsTested; n++) {
        nsPoint p(rand() % 200, rand() % 200);
        if (r1.Contains(p.x, p.y) && r2.Contains(p.x, p.y)) {
          EXPECT_TRUE(r3.Contains(p.x, p.y));
        } else {
          EXPECT_FALSE(r3.Contains(p.x, p.y));
        }
      }
    }
  }

  {
    nsRect rectsSet[RectsPerTest];
    nsRect testRect;
    for (uint32_t i = 0; i < TestIterations; i++) {
      nsRegion r;
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        rectsSet[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
      }
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        r.OrWith(rectsSet[n]);
      }
      testRect.SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);

      nsRegion r2 = r;
      r2.AndWith(testRect);

      for (uint32_t n = 0; n < pointsTested; n++) {
        nsPoint p(rand() % 200, rand() % 200);
        if (r.Contains(p.x, p.y) && testRect.Contains(p.x, p.y)) {
          EXPECT_TRUE(r2.Contains(p.x, p.y));
        } else {
          EXPECT_FALSE(r2.Contains(p.x, p.y));
        }
      }
    }
  }
#endif
}

TEST(Gfx, RegionAnd) {
  {
    nsRegion r(nsRect(20, 0, 20, 20));
    r.OrWith(nsRect(0, 20, 40, 20));
    nsRegion r2;
    r2.And(r, nsRect(0, 0, 5, 5));
    EXPECT_FALSE(r.Contains(1, 1));
  }
  {
    nsRegion r(nsRect(51, 2, 57, 5));
    r.OrWith(nsRect(36, 7, 72, 4));
    r.OrWith(nsRect(36, 11, 25, 1));
    r.OrWith(nsRect(69, 12, 6, 4));
    r.OrWith(nsRect(37, 16, 54, 2));
    r.OrWith(nsRect(37, 18, 82, 2));
    r.OrWith(nsRect(10, 20, 109, 3));
    r.OrWith(nsRect(1, 23, 136, 21));
    r.OrWith(nsRect(1, 44, 148, 2));
    r.OrWith(nsRect(1, 46, 176, 31));
    r.OrWith(nsRect(6, 77, 171, 1));
    r.OrWith(nsRect(5, 78, 172, 30));
    r.OrWith(nsRect(5, 108, 165, 45));
    r.OrWith(nsRect(5, 153, 61, 5));
    r.OrWith(nsRect(72, 153, 98, 5));
    r.OrWith(nsRect(38, 158, 25, 4));
    r.OrWith(nsRect(72, 158, 98, 4));
    r.OrWith(nsRect(58, 162, 5, 8));
    r.OrWith(nsRect(72, 162, 98, 8));
    r.OrWith(nsRect(72, 170, 98, 5));
    nsRegion r2;
    r.And(r2, nsRect(18, 78, 53, 45));
  }
#ifdef REGION_RANDOM_STRESS_TESTS
  const uint32_t TestIterations = 100000;
  const uint32_t RectsPerTest = 50;
  const uint32_t pointsTested = 100;

  {
    nsRect rectsSet1[RectsPerTest];
    nsRect rectsSet2[RectsPerTest];

    for (uint32_t i = 0; i < TestIterations; i++) {
      nsRegion r1;
      nsRegion r2;
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        rectsSet1[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
        rectsSet2[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
        r1.OrWith(rectsSet1[n]);
        r2.OrWith(rectsSet1[n]);
      }

      nsRegion r3;
      r3.And(r1, r2);

      for (uint32_t n = 0; n < pointsTested; n++) {
        nsPoint p(rand() % 200, rand() % 200);
        if (r1.Contains(p.x, p.y) && r2.Contains(p.x, p.y)) {
          EXPECT_TRUE(r3.Contains(p.x, p.y));
        } else {
          EXPECT_FALSE(r3.Contains(p.x, p.y));
        }
      }
    }
  }

  {
    nsRect rectsSet[RectsPerTest];
    nsRect testRect;
    for (uint32_t i = 0; i < TestIterations; i++) {
      nsRegion r;
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        rectsSet[n].SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);
      }
      for (uint32_t n = 0; n < RectsPerTest; n++) {
        r.OrWith(rectsSet[n]);
      }
      testRect.SetRect(rand() % 100, rand() % 100, rand() % 99 + 1, rand() % 99 + 1);

      nsRegion r2;
      r2.And(r, testRect);

      for (uint32_t n = 0; n < pointsTested; n++) {
        nsPoint p(rand() % 200, rand() % 200);
        if (r.Contains(p.x, p.y) && testRect.Contains(p.x, p.y)) {
          EXPECT_TRUE(r2.Contains(p.x, p.y));
        } else {
          EXPECT_FALSE(r2.Contains(p.x, p.y));
        }
      }
    }
  }
#endif
}

TEST(Gfx, RegionSimplify) {
  { // ensure simplify works on a single rect
    nsRegion r(nsRect(0,100,200,100));

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,200,100));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not the same";
  }

  { // the rectangles will be merged
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,300,300));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // two rectangle on the first span
    // one on the second
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));
    r.Or(r, nsRect(250,100,50,100));

    EXPECT_TRUE(r.GetNumRects() == 3) <<
      "wrong number of rects";

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,300,300));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // the rectangles will be merged
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));
    r.Or(r, nsRect(250,100,50,100));
    r.Sub(r, nsRect(200,200,40,200));

    EXPECT_TRUE(r.GetNumRects() == 4) <<
      "wrong number of rects";

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,300,300));
    result.Sub(result, nsRect(200,100,40,300));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // three spans of rectangles
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));
    r.Or(r, nsRect(250,100,50,50));
    r.Sub(r, nsRect(200,200,40,200));

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,300,300));
    result.Sub(result, nsRect(200,100,40,300));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // three spans of rectangles and an unmerged rectangle
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));
    r.Or(r, nsRect(250,100,50,50));
    r.Sub(r, nsRect(200,200,40,200));
    r.Or(r, nsRect(250,900,150,50));

    r.SimplifyOutwardByArea(100*100);

    nsRegion result(nsRect(0,100,300,300));
    result.Sub(result, nsRect(200,100,40,300));
    result.Or(result, nsRect(250,900,150,50));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // unmerged regions
    nsRegion r(nsRect(0,100,200,100));
    r.Or(r, nsRect(0,200,300,200));

    r.SimplifyOutwardByArea(100);

    nsRegion result(nsRect(0,100,200,100));
    result.Or(result, nsRect(0,200,300,200));

    EXPECT_TRUE(r.IsEqual(result)) <<
      "regions not merged";
  }

  { // empty region
    // just make sure this doesn't crash.
    nsRegion r;
    r.SimplifyOutwardByArea(100);
  }
}

TEST(Gfx, RegionContains)
{
  { // ensure Contains works on a simple region
    nsRegion r(nsRect(0, 0, 100, 100));

    EXPECT_TRUE(r.Contains(0, 0));
    EXPECT_TRUE(r.Contains(0, 99));
    EXPECT_TRUE(r.Contains(99, 0));
    EXPECT_TRUE(r.Contains(99, 99));

    EXPECT_FALSE(r.Contains(-1, 50));
    EXPECT_FALSE(r.Contains(100, 50));
    EXPECT_FALSE(r.Contains(50, -1));
    EXPECT_FALSE(r.Contains(50, 100));

    EXPECT_TRUE(r.Contains(nsRect(0, 0, 100, 100)));
    EXPECT_TRUE(r.Contains(nsRect(99, 99, 1, 1)));

    EXPECT_FALSE(r.Contains(nsRect(100, 100, 1, 1)));
    EXPECT_FALSE(r.Contains(nsRect(100, 100, 0, 0)));
  }

  { // empty regions contain nothing
    nsRegion r(nsRect(100, 100, 0, 0));

    EXPECT_FALSE(r.Contains(0, 0));
    EXPECT_FALSE(r.Contains(100, 100));
    EXPECT_FALSE(r.Contains(nsRect(100, 100, 0, 0)));
    EXPECT_FALSE(r.Contains(nsRect(100, 100, 1, 1)));
  }

  { // complex region contain tests
    // The region looks like this, with two squares that overlap.
    // (hard to do accurately with ASCII art)
    // +------+
    // |      |
    // |      +--+
    // |         |
    // +--+      |
    //    |      |
    //    +------+
    nsRegion r(nsRect(0, 0, 100, 100));
    r.OrWith(nsRect(50, 50, 100, 100));

    EXPECT_TRUE(r.Contains(0, 0));
    EXPECT_TRUE(r.Contains(99, 99));
    EXPECT_TRUE(r.Contains(50, 100));
    EXPECT_TRUE(r.Contains(100, 50));
    EXPECT_TRUE(r.Contains(149, 149));

    EXPECT_FALSE(r.Contains(49, 100));
    EXPECT_FALSE(r.Contains(100, 49));
    EXPECT_FALSE(r.Contains(150, 150));

    EXPECT_TRUE(r.Contains(nsRect(100, 100, 1, 1)));
    EXPECT_FALSE(r.Contains(nsRect(49, 99, 2, 2)));
  }

  { // region with a hole
    nsRegion r(nsRect(0, 0, 100, 100));
    r.SubOut(nsRect(40, 40, 10, 10));

    EXPECT_TRUE(r.Contains(0, 0));
    EXPECT_TRUE(r.Contains(39, 39));
    EXPECT_FALSE(r.Contains(40, 40));
    EXPECT_FALSE(r.Contains(49, 49));
    EXPECT_TRUE(r.Contains(50, 50));

    EXPECT_FALSE(r.Contains(nsRect(40, 40, 10, 10)));
    EXPECT_FALSE(r.Contains(nsRect(39, 39, 2, 2)));
  }
}

#define DILATE_VALUE 0x88
#define REGION_VALUE 0xff

struct RegionBitmap {
  RegionBitmap(unsigned char *bitmap, int width, int height) : bitmap(bitmap), width(width), height(height) {}

  void clear() {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        bitmap[x + y * width] = 0;
      }
    }
  }

  void set(nsRegion &region) {
    clear();
    for (auto iter = region.RectIter(); !iter.Done(); iter.Next()) {
      const nsRect& r = iter.Get();
      for (int y = r.Y(); y < r.YMost(); y++) {
        for (int x = r.X(); x < r.XMost(); x++) {
          bitmap[x + y * width] = REGION_VALUE;
        }
      }
    }
  }

  void dilate() {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        if (bitmap[x + y * width] == REGION_VALUE) {
          for (int yn = max(y - 1, 0); yn <= min(y + 1, height - 1); yn++) {
            for (int xn = max(x - 1, 0); xn <= min(x + 1, width - 1); xn++) {
              if (bitmap[xn + yn * width] == 0)
                bitmap[xn + yn * width] = DILATE_VALUE;
            }
          }
        }
      }
    }
  }
  void compare(RegionBitmap &reference) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        EXPECT_EQ(bitmap[x + y * width], reference.bitmap[x + y * width]);
      }
    }
  }

  unsigned char *bitmap;
  int width;
  int height;
};

void VisitEdge(void *closure, VisitSide side, int x1, int y1, int x2, int y2)
{
  EXPECT_GE(x2, x1);
  RegionBitmap *visitor = static_cast<RegionBitmap*>(closure);
  unsigned char *bitmap = visitor->bitmap;
  const int width = visitor->width;

  if (side == VisitSide::TOP) {
    while (x1 != x2) {
      bitmap[x1 + (y1 - 1) * width] = DILATE_VALUE;
      x1++;
    }
  } else if (side == VisitSide::BOTTOM) {
    while (x1 != x2) {
      bitmap[x1 + y1 * width] = DILATE_VALUE;
      x1++;
    }
  } else if (side == VisitSide::LEFT) {
    while (y1 != y2) {
      bitmap[x1 - 1 + y1 *width] = DILATE_VALUE;
      y1++;
    }
  } else if (side == VisitSide::RIGHT) {
    while (y1 != y2) {
      bitmap[x1 + y1 * width] = DILATE_VALUE;
      y1++;
    }
  }
}

void TestVisit(nsRegion &r)
{
  auto reference = mozilla::MakeUnique<unsigned char[]>(600 * 600);
  auto result = mozilla::MakeUnique<unsigned char[]>(600 * 600);
  RegionBitmap ref(reference.get(), 600, 600);
  RegionBitmap res(result.get(), 600, 600);

  ref.set(r);
  ref.dilate();

  res.set(r);
  r.VisitEdges(VisitEdge, &res);
  res.compare(ref);
}

TEST(Gfx, RegionVisitEdges) {
  { // visit edges
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(20, 120, 200, 100));
    TestVisit(r);
  }

  { // two rects side by side - 1 pixel inbetween
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(121, 20, 100, 100));
    TestVisit(r);
  }

  { // two rects side by side - 2 pixels inbetween
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(122, 20, 100, 100));
    TestVisit(r);
  }

  {
    // only corner of the rects are touching
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(120, 120, 100, 100));

    TestVisit(r);
  }

  {
    // corners are 1 pixel away
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(121, 120, 100, 100));

    TestVisit(r);
  }

  {
    // vertically separated
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(120, 125, 100, 100));

    TestVisit(r);
  }

  {
    // not touching
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(130, 120, 100, 100));
    r.Or(r, nsRect(240, 20, 100, 100));

    TestVisit(r);
  }

  { // rect with a hole in it
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Sub(r, nsRect(40, 40, 10, 10));

    TestVisit(r);
  }
  {
    // left overs
    nsRegion r(nsRect(20, 20, 10, 10));
    r.Or(r, nsRect(50, 20, 10, 10));
    r.Or(r, nsRect(90, 20, 10, 10));
    r.Or(r, nsRect(24, 30, 10, 10));
    r.Or(r, nsRect(20, 40, 15, 10));
    r.Or(r, nsRect(50, 40, 15, 10));
    r.Or(r, nsRect(90, 40, 15, 10));

    TestVisit(r);
  }

  {
    // vertically separated
    nsRegion r(nsRect(20, 20, 100, 100));
    r.Or(r, nsRect(120, 125, 100, 100));

    TestVisit(r);
  }

  {
    // two upper rects followed by a lower one
    // on the same line
    nsRegion r(nsRect(5, 5, 50, 50));
    r.Or(r, nsRect(100, 5, 50, 50));
    r.Or(r, nsRect(200, 50, 50, 50));

    TestVisit(r);
  }

  {
    // bug 1130978.
    nsRegion r(nsRect(4, 1, 61, 49));
    r.Or(r, nsRect(115, 1, 99, 49));
    r.Or(r, nsRect(115, 49, 99, 1));
    r.Or(r, nsRect(12, 50, 11, 5));
    r.Or(r, nsRect(25, 50, 28, 5));
    r.Or(r, nsRect(115, 50, 99, 5));
    r.Or(r, nsRect(115, 55, 99, 12));

    TestVisit(r);
  }
}

// The TiledRegion tests use nsIntRect / IntRegion because nsRect doesn't have
// InflateToMultiple which is required by TiledRegion.
TEST(Gfx, TiledRegionNoSimplification2Rects) {
  // Add two rectangles, both rectangles are completely inside
  // different tiles.
  nsIntRegion region;
  region.OrWith(nsIntRect(50, 50, 50, 50));
  region.OrWith(nsIntRect(300, 50, 50, 50));

  TiledIntRegion tiledRegion;
  tiledRegion.Add(nsIntRect(50, 50, 50, 50));
  tiledRegion.Add(nsIntRect(300, 50, 50, 50));

  // No simplification should have happened.
  EXPECT_TRUE(region.IsEqual(tiledRegion.GetRegion()));
}

TEST(Gfx, TiledRegionNoSimplification1Region) {
  // Add two rectangles, both rectangles are completely inside
  // different tiles.
  nsIntRegion region;
  region.OrWith(nsIntRect(50, 50, 50, 50));
  region.OrWith(nsIntRect(300, 50, 50, 50));

  TiledIntRegion tiledRegion;
  tiledRegion.Add(region);

  // No simplification should have happened.
  EXPECT_TRUE(region.IsEqual(tiledRegion.GetRegion()));
}

TEST(Gfx, TiledRegionWithSimplification3Rects) {
  // Add three rectangles. The first two rectangles are completely inside
  // different tiles, but the third rectangle intersects both tiles.
  TiledIntRegion tiledRegion;
  tiledRegion.Add(nsIntRect(50, 50, 50, 50));
  tiledRegion.Add(nsIntRect(300, 50, 50, 50));
  tiledRegion.Add(nsIntRect(250, 70, 10, 10));

  // Both tiles should have simplified their rectangles, and those two
  // rectangles are adjacent to each other, so they just build up one rect.
  EXPECT_TRUE(tiledRegion.GetRegion().IsEqual(nsIntRect(50, 50, 300, 50)));
}

TEST(Gfx, TiledRegionWithSimplification1Region) {
  // Add three rectangles. The first two rectangles are completely inside
  // different tiles, but the third rectangle intersects both tiles.
  nsIntRegion region;
  region.OrWith(nsIntRect(50, 50, 50, 50));
  region.OrWith(nsIntRect(300, 50, 50, 50));
  region.OrWith(nsIntRect(250, 70, 10, 10));

  TiledIntRegion tiledRegion;
  tiledRegion.Add(region);

  // Both tiles should have simplified their rectangles, and those two
  // rectangles are adjacent to each other, so they just build up one rect.
  EXPECT_TRUE(tiledRegion.GetRegion().IsEqual(nsIntRect(50, 50, 300, 50)));
}

TEST(Gfx, TiledRegionContains) {
  // Add three rectangles. The first two rectangles are completely inside
  // different tiles, but the third rectangle intersects both tiles.
  TiledIntRegion tiledRegion;
  tiledRegion.Add(nsIntRect(50, 50, 50, 50));
  tiledRegion.Add(nsIntRect(300, 50, 50, 50));
  tiledRegion.Add(nsIntRect(250, 70, 10, 10));

  // Both tiles should have simplified their rectangles, and those two
  // rectangles are adjacent to each other, so they just build up one rect.
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(50, 50, 300, 50)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(50, 50, 50, 50)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(50, 50, 301, 50)));
}

TEST(Gfx, TiledRegionIntersects) {
  // Add three rectangles. The first two rectangles are completely inside
  // different tiles, but the third rectangle intersects both tiles.
  TiledIntRegion tiledRegion;
  tiledRegion.Add(nsIntRect(50, 50, 50, 50));
  tiledRegion.Add(nsIntRect(300, 50, 50, 50));
  tiledRegion.Add(nsIntRect(250, 70, 10, 10));

  // Both tiles should have simplified their rectangles, and those two
  // rectangles are adjacent to each other, so they just build up one rect.
  EXPECT_TRUE(tiledRegion.Intersects(nsIntRect(50, 50, 300, 50)));
  EXPECT_TRUE(tiledRegion.Intersects(nsIntRect(200, 10, 10, 50)));
  EXPECT_TRUE(tiledRegion.Intersects(nsIntRect(50, 50, 301, 50)));
  EXPECT_FALSE(tiledRegion.Intersects(nsIntRect(0, 0, 50, 500)));
}

TEST(Gfx, TiledRegionBoundaryConditions1) {
  TiledIntRegion tiledRegion;
  // This one works fine
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MIN, INT_MIN, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(INT_MIN, INT_MIN, 1, 1)));

  // This causes the tiledRegion.mBounds to overflow, so it is ignored
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MAX - 1, INT_MAX - 1, 1, 1)));

  // Verify that the tiledRegion contains only things we expect
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(INT_MIN, INT_MIN, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MAX - 1, INT_MAX - 1, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(0, 0, 1, 1)));
}

TEST(Gfx, TiledRegionBoundaryConditions2) {
  TiledIntRegion tiledRegion;
  // This one works fine
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MAX - 1, INT_MIN, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(INT_MAX - 1, INT_MIN, 1, 1)));

  // As with TiledRegionBoundaryConditions1, this overflows, so it is ignored
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MIN, INT_MAX - 1, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(INT_MAX - 1, INT_MIN, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MIN, INT_MAX - 1, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(0, 0, 1, 1)));
}

TEST(Gfx, TiledRegionBigRects) {
  TiledIntRegion tiledRegion;
  // Super wide region, forces simplification into bounds mode
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MIN, INT_MIN, INT_MAX, 100)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(INT_MIN, INT_MIN, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(-2, INT_MIN + 99, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(-2, INT_MIN + 100, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(-1, INT_MIN + 99, 1, 1)));

  // Add another rect, verify that simplification caused the entire bounds
  // to expand by a lot more.
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MIN, INT_MIN + 200, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(-2, INT_MIN + 100, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(-2, INT_MIN + 200, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(-2, INT_MIN + 201, 1, 1)));
}

TEST(Gfx, TiledRegionBoundaryOverflow) {
  TiledIntRegion tiledRegion;
  tiledRegion.Add(nsIntRegion(nsIntRect(100, 100, 1, 1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(100, 100, 1, 1)));

  // The next region is invalid, so it gets ignored
  tiledRegion.Add(nsIntRegion(nsIntRect(INT_MAX, INT_MAX, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MAX, INT_MAX, 1, 1)));

  // Try that again as a rect, it will also get ignored
  tiledRegion.Add(nsIntRect(INT_MAX, INT_MAX, 1, 1));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MAX, INT_MAX, 1, 1)));

  // Try with a bigger overflowing rect
  tiledRegion.Add(nsIntRect(INT_MAX, INT_MAX, 500, 500));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MIN, INT_MIN, 10, 10)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(INT_MAX, INT_MAX, 100, 100)));

  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(0, 0, 1, 1)));
}

TEST(Gfx, TiledRegionNegativeRect) {
  TiledIntRegion tiledRegion;
  // The next region is invalid, so it gets ignored
  tiledRegion.Add(nsIntRegion(nsIntRect(0, 0, -500, -500)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(-50, -50, 1, 1)));
  // Rects with negative widths/heights are treated as empty and ignored
  tiledRegion.Add(nsIntRect(0, 0, -500, -500));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(-1, -1, 1, 1)));
  EXPECT_FALSE(tiledRegion.Contains(nsIntRect(0, 0, 1, 1)));
  // Empty rects are always contained
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(0, 0, -1, -1)));
  EXPECT_TRUE(tiledRegion.Contains(nsIntRect(100, 100, -1, -1)));
}
