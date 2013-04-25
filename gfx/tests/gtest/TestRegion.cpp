/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsRegion.h"

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

