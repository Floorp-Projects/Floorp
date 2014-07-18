/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "gtest/gtest.h"
#include "nsRegion.h"

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
    nsIntRegion result(nsIntRect(0,746,318,4));

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }


  { // the first rectangle gets adjusted
    nsRegion r(nsRect(0,44760,19096,264));
    r.Or(r, nsRect(0,45024,19360,1056));

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result(nsIntRect(0,746,318,5));
    result.Or(result, nsIntRect(0,751,322,17));

    EXPECT_TRUE(result.IsEqual(scaled)) <<
      "scaled result incorrect";
  }

  { // the second rectangle gets adjusted
    nsRegion r(nsRect(0,44760,19360,264));
    r.Or(r, nsRect(0,45024,19096,1056));

    nsIntRegion scaled = r.ScaleToInsidePixels(1, 1, 60);
    nsIntRegion result(nsIntRect(0,746,322,4));
    result.Or(result, nsIntRect(0,750,318,18));

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
    nsRegionRectIterator iter(region);
    for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
      for (int y = r->y; y < r->YMost(); y++) {
        for (int x = r->x; x < r->XMost(); x++) {
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

}
