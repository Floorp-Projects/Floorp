/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "PingPongRegion.h"
#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "RegionBuilder.h"

using namespace std;

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
      EXPECT_TRUE(largest.width * largest.height == tests[i].expectedArea) <<
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
      EXPECT_TRUE(largest.width * largest.height == tests[i].expectedArea) <<
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
      for (int y = r.y; y < r.YMost(); y++) {
        for (int x = r.x; x < r.XMost(); x++) {
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
  unsigned char reference[600 * 600];
  unsigned char result[600 * 600];
  RegionBitmap ref(reference, 600, 600);
  RegionBitmap res(result, 600, 600);

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

TEST(Gfx, PingPongRegion) {
  nsRect rects[] = {
    nsRect(4, 1, 61, 49),
    nsRect(115, 1, 99, 49),
    nsRect(115, 49, 99, 1),
    nsRect(12, 50, 11, 5),
    nsRect(25, 50, 28, 5),
    nsRect(115, 50, 99, 5),
    nsRect(115, 55, 99, 12),
  };

  // Test accumulations of various sizes to make sure
  // the ping-pong behavior of PingPongRegion is working.
  for (size_t size = 0; size < mozilla::ArrayLength(rects); size++) {
    // bug 1130978.
    nsRegion r;
    PingPongRegion<nsRegion> ar;
    for (size_t i = 0; i < size; i++) {
      r.Or(r, rects[i]);
      ar.OrWith(rects[i]);
      EXPECT_TRUE(ar.Region().IsEqual(r));
    }

    for (size_t i = 0; i < size; i++) {
      ar.SubOut(rects[i]);
      r.SubOut(rects[i]);
      EXPECT_TRUE(ar.Region().IsEqual(r));
    }
  }
}

MOZ_GTEST_BENCH(GfxBench, RegionOr, []{
  const int size = 5000;

  nsRegion r;
  for (int i = 0; i < size; i++) {
    r = r.Or(r, nsRect(i, i, i + 10, i + 10));
  }

  nsIntRegion rInt;
  for (int i = 0; i < size; i++) {
    rInt = rInt.Or(rInt, nsIntRect(i, i, i + 10, i + 10));
  }
});

MOZ_GTEST_BENCH(GfxBench, RegionAnd, []{
  const int size = 5000;
  nsRegion r(nsRect(0, 0, size, size));
  for (int i = 0; i < size; i++) {
    nsRegion rMissingPixel(nsRect(0, 0, size, size));
    rMissingPixel = rMissingPixel.Sub(rMissingPixel, nsRect(i, i, 1, 1));
    r = r.And(r, rMissingPixel);
  }
});

void BenchRegionBuilderOr() {
  const int size = 5000;

  RegionBuilder<nsRegion> r;
  for (int i = 0; i < size; i++) {
    r.Or(nsRect(i, i, i + 10, i + 10));
  }
  r.ToRegion();

  RegionBuilder<nsIntRegion> rInt;
  for (int i = 0; i < size; i++) {
    rInt.Or(nsIntRect(i, i, i + 10, i + 10));
  }
  rInt.ToRegion();
}

MOZ_GTEST_BENCH(GfxBench, RegionBuilderOr, []{
  BenchRegionBuilderOr();
});

void BenchPingPongRegionOr() {
  const int size = 5000;

  PingPongRegion<nsRegion> r;
  for (int i = 0; i < size; i++) {
    r.OrWith(nsRect(i, i, i + 10, i + 10));
  }
  r.Region();

  PingPongRegion<nsIntRegion> rInt;
  for (int i = 0; i < size; i++) {
    rInt.OrWith(nsIntRect(i, i, i + 10, i + 10));
  }
  rInt.Region();
}

MOZ_GTEST_BENCH(GfxBench, PingPongRegionOr, []{
  BenchPingPongRegionOr();
});

