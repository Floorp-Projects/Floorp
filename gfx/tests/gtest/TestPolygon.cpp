/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "PolygonTestUtils.h"

#include "nsTArray.h"
#include "Point.h"
#include "Polygon.h"
#include "Triangle.h"

using namespace mozilla::gfx;
typedef mozilla::gfx::Polygon MozPolygon;

TEST(MozPolygon, TriangulateRectangle)
{
  const MozPolygon p{
      Point4D(0.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 1.0f, 1.0f, 1.0f),
      Point4D(1.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 0.0f, 1.0f, 1.0f)};

  const nsTArray<Triangle> triangles = p.ToTriangles();
  const nsTArray<Triangle> expected = {
      Triangle(Point(0.0f, 0.0f), Point(0.0f, 1.0f), Point(1.0f, 1.0f)),
      Triangle(Point(0.0f, 0.0f), Point(1.0f, 1.0f), Point(1.0f, 0.0f))};

  AssertArrayEQ(triangles, expected);
}

TEST(MozPolygon, TriangulatePentagon)
{
  const MozPolygon p{
      Point4D(0.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 1.0f, 1.0f, 1.0f),
      Point4D(0.5f, 1.5f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f),
      Point4D(1.0f, 0.0f, 1.0f, 1.0f)};

  const nsTArray<Triangle> triangles = p.ToTriangles();
  const nsTArray<Triangle> expected = {
      Triangle(Point(0.0f, 0.0f), Point(0.0f, 1.0f), Point(0.5f, 1.5f)),
      Triangle(Point(0.0f, 0.0f), Point(0.5f, 1.5f), Point(1.0f, 1.0f)),
      Triangle(Point(0.0f, 0.0f), Point(1.0f, 1.0f), Point(1.0f, 0.0f))};

  AssertArrayEQ(triangles, expected);
}

static void TestClipRect(const MozPolygon& aPolygon,
                         const MozPolygon& aExpected, const Rect& aRect) {
  const MozPolygon res = aPolygon.ClipPolygon(MozPolygon::FromRect(aRect));
  EXPECT_TRUE(res == aExpected);
}

TEST(MozPolygon, ClipRectangle)
{
  MozPolygon polygon{
      Point4D(0.0f, 0.0f, 0.0f, 1.0f), Point4D(0.0f, 1.0f, 0.0f, 1.0f),
      Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 0.0f, 1.0f)};
  TestClipRect(polygon, polygon, Rect(0.0f, 0.0f, 1.0f, 1.0f));

  MozPolygon expected = MozPolygon{
      Point4D(0.0f, 0.0f, 0.0f, 1.0f), Point4D(0.0f, 0.8f, 0.0f, 1.0f),
      Point4D(0.8f, 0.8f, 0.0f, 1.0f), Point4D(0.8f, 0.0f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 0.8f, 0.8f));

  expected = MozPolygon{
      Point4D(0.2f, 0.2f, 0.0f, 1.0f), Point4D(0.2f, 1.0f, 0.0f, 1.0f),
      Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(1.0f, 0.2f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.8f, 0.8f));

  expected = MozPolygon{
      Point4D(0.2f, 0.2f, 0.0f, 1.0f), Point4D(0.2f, 0.8f, 0.0f, 1.0f),
      Point4D(0.8f, 0.8f, 0.0f, 1.0f), Point4D(0.8f, 0.2f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.6f, 0.6f));
}

TEST(MozPolygon, ClipTriangle)
{
  MozPolygon clipped, expected;
  const MozPolygon polygon{Point4D(0.0f, 0.0f, 0.0f, 1.0f),
                           Point4D(0.0f, 1.0f, 0.0f, 1.0f),
                           Point4D(1.0f, 1.0f, 0.0f, 1.0f)};

  expected = MozPolygon{Point4D(0.0f, 0.0f, 0.0f, 1.0f),
                        Point4D(0.0f, 1.0f, 0.0f, 1.0f),
                        Point4D(1.0f, 1.0f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 1.0f, 1.0f));

  expected = MozPolygon{Point4D(0.0f, 0.0f, 0.0f, 1.0f),
                        Point4D(0.0f, 0.8f, 0.0f, 1.0f),
                        Point4D(0.8f, 0.8f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.0f, 0.0f, 0.8f, 0.8f));

  expected = MozPolygon{Point4D(0.2f, 0.2f, 0.0f, 1.0f),
                        Point4D(0.2f, 1.0f, 0.0f, 1.0f),
                        Point4D(1.0f, 1.0f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.8f, 0.8f));

  expected = MozPolygon{Point4D(0.2f, 0.2f, 0.0f, 1.0f),
                        Point4D(0.2f, 0.8f, 0.0f, 1.0f),
                        Point4D(0.8f, 0.8f, 0.0f, 1.0f)};
  TestClipRect(polygon, expected, Rect(0.2f, 0.2f, 0.6f, 0.6f));
}
