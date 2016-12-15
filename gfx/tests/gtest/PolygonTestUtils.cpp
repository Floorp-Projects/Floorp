/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PolygonTestUtils.h"

#include <cmath>

namespace mozilla {
namespace gfx {

const float kEpsilon = 0.001f;

// Compares two points while allowing some numerical inaccuracy.
bool FuzzyEquals(const Point4D& lhs, const Point4D& rhs)
{
  const auto d = lhs - rhs;

  return std::abs(d.x) < kEpsilon &&
         std::abs(d.y) < kEpsilon &&
         std::abs(d.z) < kEpsilon &&
         std::abs(d.w) < kEpsilon;
}

bool FuzzyEquals(const Point3D& lhs, const Point3D& rhs)
{
  const auto d = lhs - rhs;

  return std::abs(d.x) < kEpsilon &&
         std::abs(d.y) < kEpsilon &&
         std::abs(d.z) < kEpsilon;
}

bool FuzzyEquals(const Point& lhs, const Point& rhs)
{
  const auto d = lhs - rhs;

  return std::abs(d.x) < kEpsilon &&
         std::abs(d.y) < kEpsilon;
}

bool operator==(const Triangle& lhs, const Triangle& rhs)
{
  return FuzzyEquals(lhs.p1, rhs.p1) &&
         FuzzyEquals(lhs.p2, rhs.p2) &&
         FuzzyEquals(lhs.p3, rhs.p3);
}

// Compares the points of two polygons and ensures
// that the points are in the same winding order.
bool operator==(const Polygon& lhs, const Polygon& rhs)
{
  const auto& left = lhs.GetPoints();
  const auto& right = rhs.GetPoints();

  // Polygons do not have the same amount of points.
  if (left.Length() != right.Length()) {
    return false;
  }

  const size_t pointCount = left.Length();

  // Find the first vertex of the first polygon from the second polygon.
  // This assumes that the polygons do not contain duplicate vertices.
  int start = -1;
  for (size_t i = 0; i < pointCount; ++i) {
    if (FuzzyEquals(left[0], right[i])) {
      start = i;
      break;
    }
  }

  // Found at least one different vertex.
  if (start == -1) {
    return false;
  }

  // Verify that the polygons have the same points.
  for (size_t i = 0; i < pointCount; ++i) {
    size_t j = (start + i) % pointCount;

    if (!FuzzyEquals(left[i], right[j])) {
      return false;
    }
  }

  return true;
}

} // namespace gfx
} // namespace mozilla


TEST(PolygonTestUtils, TestSanity)
{
  EXPECT_TRUE(FuzzyEquals(Point3D(0.0f, 0.0f, 0.0f),
                          Point3D(0.0f, 0.0f, 0.0f)));

  EXPECT_TRUE(FuzzyEquals(Point3D(0.0f, 0.0f, 0.0f),
                          Point3D(0.00001f, 0.00001f, 0.00001f)));

  EXPECT_TRUE(FuzzyEquals(Point3D(0.00001f, 0.00001f, 0.00001f),
                          Point3D(0.0f, 0.0f, 0.0f)));

  EXPECT_FALSE(FuzzyEquals(Point3D(0.0f, 0.0f, 0.0f),
                           Point3D(0.01f, 0.01f, 0.01f)));

  EXPECT_FALSE(FuzzyEquals(Point3D(0.01f, 0.01f, 0.01f),
                           Point3D(0.0f, 0.0f, 0.0f)));

  Polygon p1 {
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f),
    Point3D(0.0f, 1.0f, 1.0f)
  };

  // Same points as above shifted forward by one position.
  Polygon shifted {
    Point3D(0.0f, 1.0f, 1.0f),
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f)
  };

  Polygon p2 {
    Point3D(0.00001f, 0.00001f, 1.00001f),
    Point3D(1.00001f, 0.00001f, 1.00001f),
    Point3D(1.00001f, 1.00001f, 1.00001f),
    Point3D(0.00001f, 1.00001f, 1.00001f)
  };

  Polygon p3 {
    Point3D(0.01f, 0.01f, 1.01f),
    Point3D(1.01f, 0.01f, 1.01f),
    Point3D(1.01f, 1.01f, 1.01f),
    Point3D(0.01f, 1.01f, 1.01f)
  };

  // Trivial equals
  EXPECT_TRUE(p1 == p1);
  EXPECT_TRUE(p2 == p2);
  EXPECT_TRUE(p3 == p3);
  EXPECT_TRUE(shifted == shifted);

  // Polygons with the same point order
  EXPECT_TRUE(p1 == p2);
  EXPECT_TRUE(p1 == shifted);

  // Polygons containing different points
  EXPECT_FALSE(p1 == p3);
  EXPECT_FALSE(p2 == p3);
  EXPECT_FALSE(shifted == p3);

  const nsTArray<Triangle> t1 {
    Triangle(Point(0.0f, 0.0f), Point(0.0f, 1.0f), Point(1.0f, 1.0f))
  };

  const nsTArray<Triangle> t2 {
    Triangle(Point(0.0f, 0.0f), Point(1.0f, 1.0f), Point(1.0f, 0.0f))
  };

  const nsTArray<Triangle> t3 {
    Triangle(
      Point(0.00001f, 0.00001f),
      Point(0.00001f, 1.00001f),
      Point(1.00001f, 1.00001f)
    )
  };

  EXPECT_TRUE(t1[0] == t1[0]);
  EXPECT_TRUE(t2[0] == t2[0]);
  EXPECT_TRUE(t3[0] == t1[0]);

  EXPECT_FALSE(t1[0] == t2[0]);
  EXPECT_FALSE(t2[0] == t3[0]);

  AssertArrayEQ(t1, t1);
  AssertArrayEQ(t2, t2);
}