/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "PolygonTestUtils.h"

#include "nsTArray.h"
#include "Point.h"
#include "Polygon.h"
#include "Triangle.h"

using namespace mozilla::gfx;

TEST(Polygon, TriangulateRectangle)
{
  const Polygon p {
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(0.0f, 1.0f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f)
  };

  const nsTArray<Triangle> triangles = p.ToTriangles();
  const nsTArray<Triangle> expected = {
    Triangle(Point(0.0f, 0.0f), Point(0.0f, 1.0f), Point(1.0f, 1.0f)),
    Triangle(Point(0.0f, 0.0f), Point(1.0f, 1.0f), Point(1.0f, 0.0f))
  };

  AssertArrayEQ(triangles, expected);
}

TEST(Polygon, TriangulatePentagon)
{
  const Polygon p {
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(0.0f, 1.0f, 1.0f),
    Point3D(0.5f, 1.5f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f)
  };

  const nsTArray<Triangle> triangles = p.ToTriangles();
  const nsTArray<Triangle> expected = {
    Triangle(Point(0.0f, 0.0f), Point(0.0f, 1.0f), Point(0.5f, 1.5f)),
    Triangle(Point(0.0f, 0.0f), Point(0.5f, 1.5f), Point(1.0f, 1.0f)),
    Triangle(Point(0.0f, 0.0f), Point(1.0f, 1.0f), Point(1.0f, 0.0f))
  };

  AssertArrayEQ(triangles, expected);
}

void
TestClipRect(const Polygon& aPolygon,
             const Polygon& aExpected,
             const Rect& aRect)
{
  const Polygon res = aPolygon.ClipPolygon(Polygon::FromRect(aRect));
  EXPECT_TRUE(res == aExpected);
}

TEST(Polygon, ClipRectangle)
{
  Polygon polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(1.0f, 0.0f, 0.0f)
  };
  TestClipRect(polygon, polygon, Rect(0.0f, 0.0f, 1.0f, 1.0f));

  Polygon expected = Polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f),
    Point3D(0.8f, 0.0f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 0.8f, 0.8f));

  expected = Polygon {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(1.0f, 0.2f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.8f, 0.8f));

  expected = Polygon {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f),
    Point3D(0.8f, 0.2f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.6f, 0.6f));
}

TEST(Polygon, ClipTriangle)
{
  Polygon clipped, expected;
  const Polygon polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };

  expected = Polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 1.0f, 1.0f));

  expected = Polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 0.8f, 0.8f));

  expected = Polygon {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.8f, 0.8f));

  expected = Polygon {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f)
  };
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.6f, 0.6f));
}