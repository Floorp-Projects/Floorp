/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Rob Arnold <robarnold@cs.cmu.edu>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestHarness.h"
#include "nsRegion.h"

class TestLargestRegion {
  static bool TestSingleRect(nsRect r) {
    nsRegion region(r);
    if (!region.GetLargestRectangle().IsEqualInterior(r)) {
      fail("largest rect of singleton %d %d %d %d", r.x, r.y, r.width, r.height);
      return PR_FALSE;
    }
    return PR_TRUE;
  }
  // Construct a rectangle, remove part of it, then check the remainder
  static bool TestNonRectangular() {
    nsRegion r(nsRect(0, 0, 30, 30));

    const int nTests = 19;
    struct {
      nsRect rect;
      PRInt64 expectedArea;
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
    for (PRInt32 i = 0; i < nTests; i++) {
      nsRegion r2;
      r2.Sub(r, tests[i].rect);

      if (!r2.IsComplex())
        fail("nsRegion code got unexpectedly smarter!");

      nsRect largest = r2.GetLargestRectangle();
      if (largest.width * largest.height != tests[i].expectedArea) {
        fail("Did not succesfully find largest rectangle in non-rectangular region on iteration %d", i);
        success = PR_FALSE;
      }
    }

    return success;
  }
  static bool TwoRectTest() {
    nsRegion r(nsRect(0, 0, 100, 100));
    const int nTests = 4;
    struct {
      nsRect rect1, rect2;
      PRInt64 expectedArea;
    } tests[nTests] = {
      { nsRect(0, 0, 75, 40),  nsRect(0, 60, 75, 40),  2500 },
      { nsRect(25, 0, 75, 40), nsRect(25, 60, 75, 40), 2500 },
      { nsRect(25, 0, 75, 40), nsRect(0, 60, 75, 40),  2000 },
      { nsRect(0, 0, 75, 40),  nsRect(25, 60, 75, 40), 2000 },
    };
    bool success = true;
    for (PRInt32 i = 0; i < nTests; i++) {
      nsRegion r2;

      r2.Sub(r, tests[i].rect1);
      r2.Sub(r2, tests[i].rect2);

      if (!r2.IsComplex())
        fail("nsRegion code got unexpectedly smarter!");

      nsRect largest = r2.GetLargestRectangle();
      if (largest.width * largest.height != tests[i].expectedArea) {
        fail("Did not succesfully find largest rectangle in two-rect-subtract region on iteration %d", i);
        success = PR_FALSE;
      }
    }
    return success;
  }
  static bool TestContainsSpecifiedRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    if (!r.GetLargestRectangle(nsRect(0, 300, 10, 10)).IsEqualInterior(nsRect(0, 300, 50, 50))) {
      fail("Chose wrong rectangle");
      return PR_FALSE;
    }
    return PR_TRUE;
  }
  static bool TestContainsSpecifiedOverflowingRect() {
    nsRegion r(nsRect(0, 0, 100, 100));
    r.Or(r, nsRect(0, 300, 50, 50));
    if (!r.GetLargestRectangle(nsRect(0, 290, 10, 20)).IsEqualInterior(nsRect(0, 300, 50, 50))) {
      fail("Chose wrong rectangle");
      return PR_FALSE;
    }
    return PR_TRUE;
  }
public:
  static bool Test() {
    if (!TestSingleRect(nsRect(0, 52, 720, 480)) ||
        !TestSingleRect(nsRect(-20, 40, 50, 20)) ||
        !TestSingleRect(nsRect(-20, 40, 10, 8)) ||
        !TestSingleRect(nsRect(-20, -40, 10, 8)) ||
        !TestSingleRect(nsRect(-10, -10, 20, 20)))
      return PR_FALSE;
    if (!TestNonRectangular())
      return PR_FALSE;
    if (!TwoRectTest())
      return PR_FALSE;
    if (!TestContainsSpecifiedRect())
      return PR_FALSE;
    if (!TestContainsSpecifiedOverflowingRect())
      return PR_FALSE;
    passed("TestLargestRegion");
    return PR_TRUE;
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
