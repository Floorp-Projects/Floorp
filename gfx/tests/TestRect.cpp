/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRect.h"
#include <stdio.h>
#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

static PRBool
TestConstructors()
{
  // Create a rectangle
  nsRect  rect1(10, 20, 30, 40);

  // Make sure the rectangle was properly initialized
  if ((rect1.x != 10) || (rect1.y != 20) ||
      (rect1.width != 30) || (rect1.height != 40)) {
    printf("rect initialization failed!\n");
    return PR_FALSE;
  }

  // Create a second rect using the copy constructor
  nsRect  rect2(rect1);

  // Make sure the rectangle was properly initialized
  if ((rect2.x != rect1.x) || (rect2.y != rect1.y) ||
      (rect2.width != rect1.width) || (rect2.height != rect1.height)) {
    printf("rect copy constructor failed!\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

static PRBool
TestEqualityOperator()
{
  nsRect  rect1(10, 20, 30, 40);
  nsRect  rect2(rect1);

  // Test the equality operator
  if (!(rect1 == rect2)) {
    printf("rect equality operator failed!\n");
    return PR_FALSE;
  }

  // Test the inequality operator
  if (rect1 != rect2) {
    printf("rect inequality operator failed!\n");
    return PR_FALSE;
  }

  // Make sure that two empty rects are equal
  rect1.Empty();
  rect2.Empty();
  if (!(rect1 == rect2)) {
    printf("rect equality operator failed for empty rects!\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

static PRBool
TestContainment()
{
  nsRect  rect1(10, 10, 50, 50);

  // Test the point containment methods
  //

  // Basic test of a point in the middle of the rect
  if (!rect1.Contains(rect1.x + rect1.width/2, rect1.y + rect1.height/2)) {
    printf("point containment test #1 failed!\n");
    return PR_FALSE;
  }

  // Test against a point at the left/top edges
  if (!rect1.Contains(rect1.x, rect1.y)) {
    printf("point containment test #2 failed!\n");
    return PR_FALSE;
  }

  // Test against a point at the right/bottom extents
  if (rect1.Contains(rect1.XMost(), rect1.YMost())) {
    printf("point containment test #3 failed!\n");
    return PR_FALSE;
  }

  // Test the rect containment methods
  //
  nsRect  rect2(rect1);

  // Test against a rect that's the same as rect1
  if (!rect1.Contains(rect2)) {
    printf("rect containment test #1 failed!\n");
    return PR_FALSE;
  }

  // Test against a rect whose left edge (only) is outside of rect1
  rect2.x--;
  if (rect1.Contains(rect2)) {
    printf("rect containment test #2 failed!\n");
    return PR_FALSE;
  }
  rect2.x++;

  // Test against a rect whose top edge (only) is outside of rect1
  rect2.y--;
  if (rect1.Contains(rect2)) {
    printf("rect containment test #3 failed!\n");
    return PR_FALSE;
  }
  rect2.y++;

  // Test against a rect whose right edge (only) is outside of rect1
  rect2.x++;
  if (rect1.Contains(rect2)) {
    printf("rect containment test #2 failed!\n");
    return PR_FALSE;
  }
  rect2.x--;

  // Test against a rect whose bottom edge (only) is outside of rect1
  rect2.y++;
  if (rect1.Contains(rect2)) {
    printf("rect containment test #3 failed!\n");
    return PR_FALSE;
  }
  rect2.y--;

  return PR_TRUE;
}

// Test the method that returns a boolean result but doesn't return a
// a rectangle
static PRBool
TestIntersects()
{
  nsRect  rect1(10, 10, 50, 50);
  nsRect  rect2(rect1);

  // Test against a rect that's the same as rect1
  if (!rect1.Intersects(rect2)) {
    printf("rect intersects test #1 failed!\n");
    return PR_FALSE;
  }

  // Test against a rect that's enclosed by rect1
  rect2.Deflate(1, 1);
  if (!rect1.Contains(rect2) || !rect1.Intersects(rect2)) {
    printf("rect intersects test #2 failed!\n");
    return PR_FALSE;
  }
  rect2.Inflate(1, 1);

  // Make sure inflate and deflate worked correctly
  if (rect1 != rect2) {
    printf("rect inflate or deflate failed!\n");
    return PR_FALSE;
  }

  // Test against a rect that overlaps the left edge of rect1
  rect2.x--;
  if (!rect1.Intersects(rect2)) {
    printf("rect containment test #3 failed!\n");
    return PR_FALSE;
  }
  rect2.x++;

  // Test against a rect that's outside of rect1 on the left
  rect2.x -= rect2.width;
  if (rect1.Intersects(rect2)) {
    printf("rect containment test #4 failed!\n");
    return PR_FALSE;
  }
  rect2.x += rect2.width;

  // Test against a rect that overlaps the top edge of rect1
  rect2.y--;
  if (!rect1.Intersects(rect2)) {
    printf("rect containment test #5 failed!\n");
    return PR_FALSE;
  }
  rect2.y++;

  // Test against a rect that's outside of rect1 on the top
  rect2.y -= rect2.height;
  if (rect1.Intersects(rect2)) {
    printf("rect containment test #6 failed!\n");
    return PR_FALSE;
  }
  rect2.y += rect2.height;

  // Test against a rect that overlaps the right edge of rect1
  rect2.x++;
  if (!rect1.Intersects(rect2)) {
    printf("rect containment test #7 failed!\n");
    return PR_FALSE;
  }
  rect2.x--;

  // Test against a rect that's outside of rect1 on the right
  rect2.x += rect2.width;
  if (rect1.Intersects(rect2)) {
    printf("rect containment test #8 failed!\n");
    return PR_FALSE;
  }
  rect2.x -= rect2.width;

  // Test against a rect that overlaps the bottom edge of rect1
  rect2.y++;
  if (!rect1.Intersects(rect2)) {
    printf("rect containment test #9 failed!\n");
    return PR_FALSE;
  }
  rect2.y--;

  // Test against a rect that's outside of rect1 on the bottom
  rect2.y += rect2.height;
  if (rect1.Intersects(rect2)) {
    printf("rect containment test #10 failed!\n");
    return PR_FALSE;
  }
  rect2.y -= rect2.height;

  return PR_TRUE;
}

// Test the method that returns a boolean result and an intersection rect
static PRBool
TestIntersection()
{
  nsRect  rect1(10, 10, 50, 50);
  nsRect  rect2(rect1);
  nsRect  dest;

  // Test against a rect that's the same as rect1
  if (!dest.IntersectRect(rect1, rect2) || (dest != rect1)) {
    printf("rect intersection test #1 failed!\n");
    return PR_FALSE;
  }

  // Test against a rect that's enclosed by rect1
  rect2.Deflate(1, 1);
  if (!dest.IntersectRect(rect1, rect2) || (dest != rect2)) {
    printf("rect intersection test #2 failed!\n");
    return PR_FALSE;
  }
  rect2.Inflate(1, 1);

  // Test against a rect that overlaps the left edge of rect1
  rect2.x--;
  if (!dest.IntersectRect(rect1, rect2) ||
     (dest != nsRect(rect1.x, rect1.y, rect1.width - 1, rect1.height))) {
    printf("rect intersection test #3 failed!\n");
    return PR_FALSE;
  }
  rect2.x++;

  // Test against a rect that's outside of rect1 on the left
  rect2.x -= rect2.width;
  if (dest.IntersectRect(rect1, rect2)) {
    printf("rect intersection test #4 failed!\n");
    return PR_FALSE;
  }
  // Make sure an empty rect is returned
  if (!dest.IsEmpty()) {
    printf("rect intersection test #4 no empty rect!\n");
    return PR_FALSE;
  }
  rect2.x += rect2.width;

  // Test against a rect that overlaps the top edge of rect1
  rect2.y--;
  if (!dest.IntersectRect(rect1, rect2) ||
     (dest != nsRect(rect1.x, rect1.y, rect1.width, rect1.height - 1))) {
    printf("rect intersection test #5 failed!\n");
    return PR_FALSE;
  }
  rect2.y++;

  // Test against a rect that's outside of rect1 on the top
  rect2.y -= rect2.height;
  if (dest.IntersectRect(rect1, rect2)) {
    printf("rect intersection test #6 failed!\n");
    return PR_FALSE;
  }
  // Make sure an empty rect is returned
  if (!dest.IsEmpty()) {
    printf("rect intersection test #6 no empty rect!\n");
    return PR_FALSE;
  }
  rect2.y += rect2.height;

  // Test against a rect that overlaps the right edge of rect1
  rect2.x++;
  if (!dest.IntersectRect(rect1, rect2) ||
     (dest != nsRect(rect1.x + 1, rect1.y, rect1.width - 1, rect1.height))) {
    printf("rect intersection test #7 failed!\n");
    return PR_FALSE;
  }
  rect2.x--;

  // Test against a rect that's outside of rect1 on the right
  rect2.x += rect2.width;
  if (dest.IntersectRect(rect1, rect2)) {
    printf("rect intersection test #8 failed!\n");
    return PR_FALSE;
  }
  // Make sure an empty rect is returned
  if (!dest.IsEmpty()) {
    printf("rect intersection test #8 no empty rect!\n");
    return PR_FALSE;
  }
  rect2.x -= rect2.width;

  // Test against a rect that overlaps the bottom edge of rect1
  rect2.y++;
  if (!dest.IntersectRect(rect1, rect2) ||
     (dest != nsRect(rect1.x, rect1.y + 1, rect1.width, rect1.height - 1))) {
    printf("rect intersection test #9 failed!\n");
    return PR_FALSE;
  }
  rect2.y--;

  // Test against a rect that's outside of rect1 on the bottom
  rect2.y += rect2.height;
  if (dest.IntersectRect(rect1, rect2)) {
    printf("rect intersection test #10 failed!\n");
    return PR_FALSE;
  }
  // Make sure an empty rect is returned
  if (!dest.IsEmpty()) {
    printf("rect intersection test #10 no empty rect!\n");
    return PR_FALSE;
  }
  rect2.y -= rect2.height;

  return PR_TRUE;
}

static PRBool
TestUnion()
{
  nsRect  rect1;
  nsRect  rect2(10, 10, 50, 50);
  nsRect  dest;

  // Check the case where the receiver is an empty rect
  rect1.Empty();
  if (!dest.UnionRect(rect1, rect2) || (dest != rect2)) {
    printf("rect union test #1 failed!\n");
    return PR_FALSE;
  }

  // Check the case where the source rect is an empty rect
  rect1 = rect2;
  rect2.Empty();
  if (!dest.UnionRect(rect1, rect2) || (dest != rect1)) {
    printf("rect union test #2 failed!\n");
    return PR_FALSE;
  }

  // Test the case where both rects are empty. This should fail
  rect1.Empty();
  rect2.Empty();
  if (dest.UnionRect(rect1, rect2)) {
    printf("rect union test #3 failed!\n");
    return PR_FALSE;
  }

  // Test union case where the two rects don't overlap at all
  rect1.SetRect(10, 10, 50, 50);
  rect2.SetRect(100, 100, 50, 50);
  if (!dest.UnionRect(rect1, rect2) ||
     (dest != nsRect(rect1.x, rect1.y, rect2.XMost() - rect1.x, rect2.YMost() - rect1.y))) {
    printf("rect union test #4 failed!\n");
    return PR_FALSE;
  }

  // Test union case where the two rects overlap
  rect1.SetRect(30, 30, 50, 50);
  rect2.SetRect(10, 10, 50, 50);
  if (!dest.UnionRect(rect1, rect2) ||
      (dest != nsRect(rect2.x, rect2.y, rect1.XMost() - rect2.x, rect1.YMost() - rect2.y))) {
    printf("rect union test #5 failed!\n");
    return PR_FALSE;
  }

  return PR_TRUE;
}

int main(int argc, char** argv)
{
  if (!TestConstructors())
    return -1;

  if (!TestEqualityOperator())
    return -1;

  if (!TestContainment())
    return -1;

  if (!TestIntersects())
    return -1;

  if (!TestIntersection())
    return -1;

  if (!TestUnion())
    return -1;

  return 0;
}
