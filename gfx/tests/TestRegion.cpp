/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsRegion.h"

class TestLargestRegion {
  static bool TestSingleRect(nsRect r) {
    nsRegion region(r);
    if (!region.GetLargestRectangle().IsEqualInterior(r)) {
      fail("largest rect of singleton %d %d %d %d", r.x, r.y, r.width, r.height);
      return false;
    }
    return true;
  }
  // Construct a rectangle, remove part of it, then check the remainder
  static bool TestNonRectangular() {
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

    bool success = true;
    for (int32_t i = 0; i < nTests; i++) {
      nsRegion r2;
      r2.Sub(r, tests[i].rect);

      if (!r2.IsComplex())
        fail("nsRegion code got unexpectedly smarter!");

      nsRect largest = r2.GetLargestRectangle();
      if (largest.width * largest.height != tests[i].expectedArea) {
        fail("Did not successfully find largest rectangle in non-rectangular region on iteration %d", i);
        success = false;
      }
    }

    return success;
  }
  static bool TwoRectTest() {
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
    bool success = true;
    for (int32_t i = 0; i < nTests; i++) {
      nsRegion r2;

      r2.Sub(r, tests[i].rect1);
      r2.Sub(r2, tests[i].rect2);

      if (!r2.IsComplex())
        fail("nsRegion code got unexpectedly smarter!");

      nsRect largest = r2.GetLargestRectangle();
      if (largest.width * largest.height != tests[i].expectedArea) {
        fail("Did not successfully find largest rectangle in two-rect-subtract region on iteration %d", i);
        success = false;
      }
    }
    return success;
  }
  static bool TestContainsSpecifiedRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    if (!r.GetLargestRectangle(nsRect(0, 300, 10, 10)).IsEqualInterior(nsRect(0, 300, 50, 50))) {
      fail("Chose wrong rectangle");
      return false;
    }
    return true;
  }
  static bool TestContainsSpecifiedOverflowingRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    if (!r.GetLargestRectangle(nsRect(0, 290, 10, 20)).IsEqualInterior(nsRect(0, 300, 50, 50))) {
      fail("Chose wrong rectangle");
      return false;
    }
    return true;
  }
public:
  static bool Test() {
    if (!TestSingleRect(nsRect(0, 52, 720, 480)) ||
        !TestSingleRect(nsRect(-20, 40, 50, 20)) ||
        !TestSingleRect(nsRect(-20, 40, 10, 8)) ||
        !TestSingleRect(nsRect(-20, -40, 10, 8)) ||
        !TestSingleRect(nsRect(-10, -10, 20, 20)))
      return false;
    if (!TestNonRectangular())
      return false;
    if (!TwoRectTest())
      return false;
    if (!TestContainsSpecifiedRect())
      return false;
    if (!TestContainsSpecifiedOverflowingRect())
      return false;
    passed("TestLargestRegion");
    return true;
  }
};

int main(int argc, char** argv) {
  ScopedXPCOM xpcom("TestRegion");
  if (xpcom.failed())
    return -1;
  if (!TestLargestRegion::Test())
    return -1;
  return 0;
}
