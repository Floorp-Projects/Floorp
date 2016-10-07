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

TEST(Polygon3D, TriangulateRectangle)
{
  const Polygon3D p {
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

TEST(Polygon3D, TriangulatePentagon)
{
  const Polygon3D p {
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

TEST(Polygon3D, ClipRectangle)
{
  Polygon3D clipped, expected;

  Polygon3D polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(1.0f, 0.0f, 0.0f)
  };

  clipped = polygon.ClipPolygon(Rect(0.0f, 0.0f, 1.0f, 1.0f));
  EXPECT_TRUE(clipped == polygon);


  clipped = polygon.ClipPolygon(Rect(0.0f, 0.0f, 0.8f, 0.8f));
  expected = Polygon3D {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f),
    Point3D(0.8f, 0.0f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);


  clipped = polygon.ClipPolygon(Rect(0.2f, 0.2f, 0.8f, 0.8f));
  expected = Polygon3D {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(1.0f, 0.2f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);


  clipped = polygon.ClipPolygon(Rect(0.2f, 0.2f, 0.6f, 0.6f));
  expected = Polygon3D {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f),
    Point3D(0.8f, 0.2f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);
}

TEST(Polygon3D, ClipTriangle)
{
  Polygon3D clipped, expected;
  const Polygon3D polygon {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };

  clipped = polygon.ClipPolygon(Rect(0.0f, 0.0f, 1.0f, 1.0f));
  expected = Polygon3D {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);


  clipped = polygon.ClipPolygon(Rect(0.0f, 0.0f, 0.8f, 0.8f));
  expected = Polygon3D {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(0.0f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);


  clipped = polygon.ClipPolygon(Rect(0.2f, 0.2f, 0.8f, 0.8f));
  expected = Polygon3D {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 1.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);


  clipped = polygon.ClipPolygon(Rect(0.2f, 0.2f, 0.6f, 0.6f));
  expected = Polygon3D {
    Point3D(0.2f, 0.2f, 0.0f),
    Point3D(0.2f, 0.8f, 0.0f),
    Point3D(0.8f, 0.8f, 0.0f)
  };
  EXPECT_TRUE(clipped == expected);
}
