/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsRect.h"
#include "gfxRect.h"
#ifdef XP_WIN
#include <windows.h>
#endif

template <class RectType>
static bool
TestConstructors()
{
  // Create a rectangle
  RectType  rect1(10, 20, 30, 40);

  // Make sure the rectangle was properly initialized
  EXPECT_TRUE(rect1.x == 10 && rect1.y == 20 &&
      rect1.width == 30 && rect1.height == 40) <<
    "[1] Make sure the rectangle was properly initialized with constructor";

  // Create a second rect using the copy constructor
  RectType  rect2(rect1);

  // Make sure the rectangle was properly initialized
  EXPECT_TRUE(rect2.x == rect1.x && rect2.y == rect2.y &&
      rect2.width == rect2.width && rect2.height == rect2.height) <<
    "[2] Make sure the rectangle was properly initialized with copy constructor";


  EXPECT_TRUE(!rect1.IsEmpty() && rect1.IsFinite() &&
      !rect2.IsEmpty() && rect2.IsFinite()) <<
    "[3] These rectangles are not empty and are finite";

  return true;
}

template <class RectType>
static bool
TestEqualityOperator()
{
  RectType  rect1(10, 20, 30, 40);
  RectType  rect2(rect1);

  // Test the equality operator
  EXPECT_TRUE(rect1 == rect2) <<
    "[1] Test the equality operator";

  EXPECT_FALSE(!rect1.IsEqualInterior(rect2)) <<
    "[2] Test the inequality operator";

  // Make sure that two empty rects are equal
  rect1.SetEmpty();
  rect2.SetEmpty();
  EXPECT_TRUE(rect1 == rect2) <<
    "[3] Make sure that two empty rects are equal";

  return true;
}

template <class RectType>
static bool
TestContainment()
{
  RectType  rect1(10, 10, 50, 50);

  // Test the point containment methods
  //

  // Basic test of a point in the middle of the rect
  EXPECT_FALSE(!rect1.Contains(rect1.x + rect1.width/2, rect1.y + rect1.height/2)) <<
    "[1] Basic test of a point in the middle of the rect";

  // Test against a point at the left/top edges
  EXPECT_FALSE(!rect1.Contains(rect1.x, rect1.y)) <<
    "[2] Test against a point at the left/top edges";

  // Test against a point at the right/bottom extents
  EXPECT_FALSE(rect1.Contains(rect1.XMost(), rect1.YMost())) <<
    "[3] Test against a point at the right/bottom extents";

  // Test the rect containment methods
  //
  RectType  rect2(rect1);

  // Test against a rect that's the same as rect1
  EXPECT_FALSE(!rect1.Contains(rect2)) <<
    "[4] Test against a rect that's the same as rect1";

  // Test against a rect whose left edge (only) is outside of rect1
  rect2.x--;
  EXPECT_FALSE(rect1.Contains(rect2)) <<
    "[5] Test against a rect whose left edge (only) is outside of rect1";
  rect2.x++;

  // Test against a rect whose top edge (only) is outside of rect1
  rect2.y--;
  EXPECT_FALSE(rect1.Contains(rect2)) <<
    "[6] Test against a rect whose top edge (only) is outside of rect1";
  rect2.y++;

  // Test against a rect whose right edge (only) is outside of rect1
  rect2.x++;
  EXPECT_FALSE(rect1.Contains(rect2)) <<
    "[7] Test against a rect whose right edge (only) is outside of rect1";
  rect2.x--;

  // Test against a rect whose bottom edge (only) is outside of rect1
  rect2.y++;
  EXPECT_FALSE(rect1.Contains(rect2)) <<
    "[8] Test against a rect whose bottom edge (only) is outside of rect1";
  rect2.y--;

  return true;
}

// Test the method that returns a boolean result but doesn't return a
// a rectangle
template <class RectType>
static bool
TestIntersects()
{
  RectType  rect1(10, 10, 50, 50);
  RectType  rect2(rect1);

  // Test against a rect that's the same as rect1
  EXPECT_FALSE(!rect1.Intersects(rect2)) <<
    "[1] Test against a rect that's the same as rect1";

  // Test against a rect that's enclosed by rect1
  rect2.Inflate(-1, -1);
  EXPECT_FALSE(!rect1.Contains(rect2) || !rect1.Intersects(rect2)) <<
    "[2] Test against a rect that's enclosed by rect1";
  rect2.Inflate(1, 1);

  // Make sure inflate and deflate worked correctly
  EXPECT_TRUE(rect1.IsEqualInterior(rect2)) <<
    "[3] Make sure inflate and deflate worked correctly";

  // Test against a rect that overlaps the left edge of rect1
  rect2.x--;
  EXPECT_FALSE(!rect1.Intersects(rect2)) <<
    "[4] Test against a rect that overlaps the left edge of rect1";
  rect2.x++;

  // Test against a rect that's outside of rect1 on the left
  rect2.x -= rect2.width;
  EXPECT_FALSE(rect1.Intersects(rect2)) <<
    "[5] Test against a rect that's outside of rect1 on the left";
  rect2.x += rect2.width;

  // Test against a rect that overlaps the top edge of rect1
  rect2.y--;
  EXPECT_FALSE(!rect1.Intersects(rect2)) <<
    "[6] Test against a rect that overlaps the top edge of rect1";
  rect2.y++;

  // Test against a rect that's outside of rect1 on the top
  rect2.y -= rect2.height;
  EXPECT_FALSE(rect1.Intersects(rect2)) <<
    "[7] Test against a rect that's outside of rect1 on the top";
  rect2.y += rect2.height;

  // Test against a rect that overlaps the right edge of rect1
  rect2.x++;
  EXPECT_FALSE(!rect1.Intersects(rect2)) <<
    "[8] Test against a rect that overlaps the right edge of rect1";
  rect2.x--;

  // Test against a rect that's outside of rect1 on the right
  rect2.x += rect2.width;
  EXPECT_FALSE(rect1.Intersects(rect2)) <<
    "[9] Test against a rect that's outside of rect1 on the right";
  rect2.x -= rect2.width;

  // Test against a rect that overlaps the bottom edge of rect1
  rect2.y++;
  EXPECT_FALSE(!rect1.Intersects(rect2)) <<
    "[10] Test against a rect that overlaps the bottom edge of rect1";
  rect2.y--;

  // Test against a rect that's outside of rect1 on the bottom
  rect2.y += rect2.height;
  EXPECT_FALSE(rect1.Intersects(rect2)) <<
    "[11] Test against a rect that's outside of rect1 on the bottom";
  rect2.y -= rect2.height;

  return true;
}

// Test the method that returns a boolean result and an intersection rect
template <class RectType>
static bool
TestIntersection()
{
  RectType  rect1(10, 10, 50, 50);
  RectType  rect2(rect1);
  RectType  dest;

  // Test against a rect that's the same as rect1
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) || !(dest.IsEqualInterior(rect1))) <<
    "[1] Test against a rect that's the same as rect1";

  // Test against a rect that's enclosed by rect1
  rect2.Inflate(-1, -1);
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) || !(dest.IsEqualInterior(rect2))) <<
    "[2] Test against a rect that's enclosed by rect1";
  rect2.Inflate(1, 1);

  // Test against a rect that overlaps the left edge of rect1
  rect2.x--;
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) ||
     !(dest.IsEqualInterior(RectType(rect1.x, rect1.y, rect1.width - 1, rect1.height)))) <<
    "[3] Test against a rect that overlaps the left edge of rect1";
  rect2.x++;

  // Test against a rect that's outside of rect1 on the left
  rect2.x -= rect2.width;
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2)) <<
    "[4] Test against a rect that's outside of rect1 on the left";
  // Make sure an empty rect is returned
  EXPECT_FALSE(!dest.IsEmpty()) <<
    "[4] Make sure an empty rect is returned";
  EXPECT_TRUE(dest.IsFinite()) << "[4b] Should be finite";
  rect2.x += rect2.width;

  // Test against a rect that overlaps the top edge of rect1
  rect2.y--;
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) ||
     !(dest.IsEqualInterior(RectType(rect1.x, rect1.y, rect1.width, rect1.height - 1)))) <<
    "[5] Test against a rect that overlaps the top edge of rect1";
  EXPECT_TRUE(dest.IsFinite()) << "[5b] Should be finite";
  rect2.y++;

  // Test against a rect that's outside of rect1 on the top
  rect2.y -= rect2.height;
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2)) <<
    "[6] Test against a rect that's outside of rect1 on the top";
  // Make sure an empty rect is returned
  EXPECT_FALSE(!dest.IsEmpty()) <<
    "[6] Make sure an empty rect is returned";
  EXPECT_TRUE(dest.IsFinite()) << "[6b] Should be finite";
  rect2.y += rect2.height;

  // Test against a rect that overlaps the right edge of rect1
  rect2.x++;
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) ||
     !(dest.IsEqualInterior(RectType(rect1.x + 1, rect1.y, rect1.width - 1, rect1.height)))) <<
    "[7] Test against a rect that overlaps the right edge of rect1";
  rect2.x--;

  // Test against a rect that's outside of rect1 on the right
  rect2.x += rect2.width;
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2)) <<
    "[8] Test against a rect that's outside of rect1 on the right";
  // Make sure an empty rect is returned
  EXPECT_FALSE(!dest.IsEmpty()) <<
    "[8] Make sure an empty rect is returned";
  EXPECT_TRUE(dest.IsFinite()) << "[8b] Should be finite";
  rect2.x -= rect2.width;

  // Test against a rect that overlaps the bottom edge of rect1
  rect2.y++;
  EXPECT_FALSE(!dest.IntersectRect(rect1, rect2) ||
     !(dest.IsEqualInterior(RectType(rect1.x, rect1.y + 1, rect1.width, rect1.height - 1)))) <<
    "[9] Test against a rect that overlaps the bottom edge of rect1";
  EXPECT_TRUE(dest.IsFinite()) << "[9b] Should be finite";
  rect2.y--;

  // Test against a rect that's outside of rect1 on the bottom
  rect2.y += rect2.height;
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2)) <<
    "[10] Test against a rect that's outside of rect1 on the bottom";
  // Make sure an empty rect is returned
  EXPECT_FALSE(!dest.IsEmpty()) <<
    "[10] Make sure an empty rect is returned";
  EXPECT_TRUE(dest.IsFinite()) << "[10b] Should be finite";
  rect2.y -= rect2.height;

  // Test against a rect with zero width or height
  rect1.SetRect(100, 100, 100, 100);
  rect2.SetRect(150, 100, 0, 100);
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2) || !dest.IsEmpty()) <<
    "[11] Intersection of rects with zero width or height should be empty";
  EXPECT_TRUE(dest.IsFinite()) << "[11b] Should be finite";

  // Tests against a rect with negative width or height
  //

  // Test against a rect with negative width
  rect1.SetRect(100, 100, 100, 100);
  rect2.SetRect(100, 100, -100, 100);
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2) || !dest.IsEmpty()) <<
    "[12] Intersection of rects with negative width or height should be empty";
  EXPECT_TRUE(dest.IsFinite()) << "[12b] Should be finite";

  // Those two rects exactly overlap in some way...
  // but we still want to return an empty rect
  rect1.SetRect(100, 100, 100, 100);
  rect2.SetRect(200, 200, -100, -100);
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2) || !dest.IsEmpty()) <<
    "[13] Intersection of rects with negative width or height should be empty";
  EXPECT_TRUE(dest.IsFinite()) << "[13b] Should be finite";

  // Test against two identical rects with negative height
  rect1.SetRect(100, 100, 100, -100);
  rect2.SetRect(100, 100, 100, -100);
  EXPECT_FALSE(dest.IntersectRect(rect1, rect2) || !dest.IsEmpty()) <<
    "[14] Intersection of rects with negative width or height should be empty";
  EXPECT_TRUE(dest.IsFinite()) << "[14b] Should be finite";

  return true;
}

template <class RectType>
static bool
TestUnion()
{
  RectType  rect1;
  RectType  rect2(10, 10, 50, 50);
  RectType  dest;

  // Check the case where the receiver is an empty rect
  rect1.SetEmpty();
  dest.UnionRect(rect1, rect2);
  EXPECT_FALSE(dest.IsEmpty() || !dest.IsEqualInterior(rect2)) <<
    "[1] Check the case where the receiver is an empty rect";
  EXPECT_TRUE(dest.IsFinite()) << "[1b] Should be finite";

  // Check the case where the source rect is an empty rect
  rect1 = rect2;
  rect2.SetEmpty();
  dest.UnionRect(rect1, rect2);
  EXPECT_FALSE(dest.IsEmpty() || !dest.IsEqualInterior(rect1)) <<
    "[2] Check the case where the source rect is an empty rect";
  EXPECT_TRUE(dest.IsFinite()) << "[2b] Should be finite";

  // Test the case where both rects are empty
  rect1.SetEmpty();
  rect2.SetEmpty();
  dest.UnionRect(rect1, rect2);
  EXPECT_FALSE(!dest.IsEmpty()) <<
    "[3] Test the case where both rects are empty";
  EXPECT_TRUE(dest.IsFinite()) << "[3b] Should be finite";

  // Test union case where the two rects don't overlap at all
  rect1.SetRect(10, 10, 50, 50);
  rect2.SetRect(100, 100, 50, 50);
  dest.UnionRect(rect1, rect2);
  EXPECT_FALSE(dest.IsEmpty() ||
     !(dest.IsEqualInterior(RectType(rect1.x, rect1.y, rect2.XMost() - rect1.x, rect2.YMost() - rect1.y)))) <<
    "[4] Test union case where the two rects don't overlap at all";
  EXPECT_TRUE(dest.IsFinite()) << "[4b] Should be finite";

  // Test union case where the two rects overlap
  rect1.SetRect(30, 30, 50, 50);
  rect2.SetRect(10, 10, 50, 50);
  dest.UnionRect(rect1, rect2);
  EXPECT_FALSE(dest.IsEmpty() ||
      !(dest.IsEqualInterior(RectType(rect2.x, rect2.y, rect1.XMost() - rect2.x, rect1.YMost() - rect2.y)))) <<
    "[5] Test union case where the two rects overlap";
  EXPECT_TRUE(dest.IsFinite()) << "[5b] Should be finite";

  return true;
}

static bool
TestFiniteGfx()
{
  // Doesn't appear that __builtin_inf() and __builtin_nan() are available on
  // all compilers, so go the old fashioned way for inf and nan.
  float posInf = 1.0/0.0;
  float negInf = -1.0/0.0;
  float justNaN = 0.0/0.0;

  gfxFloat values[4] = {5.0, 10.0, 15.0, 20.0};

  // Try the "non-finite" values for x, y, width, height, one at a time
  for (int i=0; i<4; i+=1) {
    values[i] = posInf;
    gfxRect rectPosInf(values[0], values[1], values[2], values[3]);
    EXPECT_FALSE(rectPosInf.IsFinite()) << "For +inf (" << values[0] << "," << values[1] << "," << values[2] << "," << values[3] << ")";

    values[i] = negInf;
    gfxRect rectNegInf(values[0], values[1], values[2], values[3]);
    EXPECT_FALSE(rectNegInf.IsFinite()) << "For -inf (" << values[0] << "," << values[1] << "," << values[2] << "," << values[3] << ")";

    values[i] = justNaN;
    gfxRect rectNaN(values[0], values[1], values[2], values[3]);
    EXPECT_FALSE(rectNaN.IsFinite()) << "For NaN (" << values[0] << "," << values[1] << "," << values[2] << "," << values[3] << ")";

    // Reset to a finite value...
    values[i] = 5.0*i;
  }

  return true;
}

TEST(Gfx, nsRect) {
  TestConstructors<nsRect>();
  TestEqualityOperator<nsRect>();
  TestContainment<nsRect>();
  TestIntersects<nsRect>();
  TestIntersection<nsRect>();
  TestUnion<nsRect>();
}

TEST(Gfx, nsIntRect) {
  TestConstructors<nsIntRect>();
  TestEqualityOperator<nsIntRect>();
  TestContainment<nsIntRect>();
  TestIntersects<nsIntRect>();
  TestIntersection<nsIntRect>();
  TestUnion<nsIntRect>();
}

TEST(Gfx, gfxRect) {
  TestConstructors<gfxRect>();
  // Skip TestEqualityOperator<gfxRect>(); as gfxRect::operator== is private
  TestContainment<gfxRect>();
  TestIntersects<gfxRect>();
  TestIntersection<gfxRect>();
  TestUnion<gfxRect>();
  TestFiniteGfx();
}
