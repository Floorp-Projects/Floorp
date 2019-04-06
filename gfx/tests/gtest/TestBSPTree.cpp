/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "BSPTree.h"
#include "Polygon.h"
#include "PolygonTestUtils.h"

#include <deque>
#include <list>

using namespace mozilla::gfx;
using namespace mozilla::layers;
typedef mozilla::gfx::Polygon MozPolygon;

namespace {

static void RunTest(std::deque<MozPolygon> aPolygons,
                    std::deque<MozPolygon> aExpected) {
  std::list<LayerPolygon> layers;
  for (MozPolygon& polygon : aPolygons) {
    layers.push_back(LayerPolygon(nullptr, std::move(polygon)));
  }

  const BSPTree tree(layers);
  const nsTArray<LayerPolygon> order = tree.GetDrawOrder();

  EXPECT_EQ(aExpected.size(), order.Length());

  for (size_t i = 0; i < order.Length(); ++i) {
    EXPECT_TRUE(aExpected[i] == *order[i].geometry);
  }
}

}  // namespace

TEST(BSPTree, SameNode)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{
          Point4D(0.0f, 0.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 0.0f, 1.0f),
          Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(0.0f, 1.0f, 0.0f, 1.0f)},
      MozPolygon{
          Point4D(0.0f, 0.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 0.0f, 1.0f),
          Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(0.0f, 1.0f, 0.0f, 1.0f)}};

  ::RunTest(polygons, polygons);
}

TEST(BSPTree, OneChild)
{
  const MozPolygon p1{
      Point4D(0.0f, 0.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 0.0f, 1.0f),
      Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(0.0f, 1.0f, 0.0f, 1.0f)};

  const MozPolygon p2{
      Point4D(0.0f, 0.0f, 1.0f, 1.0f), Point4D(1.0f, 0.0f, 1.0f, 1.0f),
      Point4D(1.0f, 1.0f, 1.0f, 1.0f), Point4D(0.0f, 1.0f, 1.0f, 1.0f)};

  ::RunTest({p1, p2}, {p1, p2});
  ::RunTest({p2, p1}, {p1, p2});
}

TEST(BSPTree, SharedEdge1)
{
  MozPolygon p1{
      Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 0.0f, 1.0f, 1.0f),
      Point4D(0.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f)};

  MozPolygon p2{
      Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f),
      Point4D(2.0f, 2.0f, 1.0f, 1.0f), Point4D(2.0f, 0.0f, 1.0f, 1.0f)};

  ::RunTest({p1, p2}, {p1, p2});
}

TEST(BSPTree, SharedEdge2)
{
  MozPolygon p1{
      Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 0.0f, 1.0f, 1.0f),
      Point4D(0.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f)};

  MozPolygon p2{
      Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f),
      Point4D(2.0f, 2.0f, 0.0f, 1.0f), Point4D(2.0f, 0.0f, 0.0f, 1.0f)};

  ::RunTest({p1, p2}, {p2, p1});
}

TEST(BSPTree, SplitSharedEdge)
{
  MozPolygon p1{
      Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 0.0f, 1.0f, 1.0f),
      Point4D(0.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f)};

  MozPolygon p2{
      Point4D(1.0f, 0.0f, 2.0f, 1.0f), Point4D(1.0f, 1.0f, 2.0f, 1.0f),
      Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 0.0f, 1.0f)};

  const std::deque<MozPolygon> expected{
      MozPolygon{
          Point4D(1.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 0.0f, 1.0f),
          Point4D(1.0f, 0.0f, 0.0f, 1.0f), Point4D(1.0f, 0.0f, 1.0f, 1.0f)},
      MozPolygon{
          Point4D(1.0f, 0.0f, 1.0f, 1.0f), Point4D(0.0f, 0.0f, 1.0f, 1.0f),
          Point4D(0.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 1.0f, 1.0f)},
      MozPolygon{
          Point4D(1.0f, 0.0f, 2.0f, 1.0f), Point4D(1.0f, 1.0f, 2.0f, 1.0f),
          Point4D(1.0f, 1.0f, 1.0f, 1.0f), Point4D(1.0f, 0.0f, 1.0f, 1.0f)}};

  ::RunTest({p1, p2}, expected);
}

TEST(BSPTree, SplitSimple1)
{
  MozPolygon p1{
      Point4D(0.0f, 0.0f, 1.0f, 1.0f), Point4D(1.0f, 0.0f, 1.0f, 1.0f),
      Point4D(1.0f, 1.0f, 1.0f, 1.0f), Point4D(0.0f, 1.0f, 1.0f, 1.0f)};

  MozPolygon p2{
      Point4D(0.0f, 0.0f, 2.0f, 1.0f), Point4D(1.0f, 0.0f, 2.0f, 1.0f),
      Point4D(1.0f, 1.0f, 0.0f, 1.0f), Point4D(0.0f, 1.0f, 0.0f, 1.0f)};

  const std::deque<MozPolygon> expected{
      MozPolygon{
          Point4D(0.0f, 1.0f, 0.0f, 1.0f), Point4D(0.0f, 0.5f, 1.0f, 1.0f),
          Point4D(1.0f, 0.5f, 1.0f, 1.0f), Point4D(1.0f, 1.0f, 0.0f, 1.0f)},
      p1,
      MozPolygon{
          Point4D(0.0f, 0.0f, 2.0f, 1.0f), Point4D(1.0f, 0.0f, 2.0f, 1.0f),
          Point4D(1.0f, 0.5f, 1.0f, 1.0f), Point4D(0.0f, 0.5f, 1.0f, 1.0f)}};

  ::RunTest({p1, p2}, expected);
}

TEST(BSPTree, SplitSimple2)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-5.00000f, -5.00000f, 0.00000f, 1.0f),
                 Point4D(-5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, -5.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, -5.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, 5.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, 5.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(0.00000f, -5.00000f, 0.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(-5.00000f, -5.00000f, 0.00000f, 1.0f),
                 Point4D(-5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, -5.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, 5.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, 5.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, 0.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, NoSplit1)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(0.00000f, 10.00000f, 0.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, 0.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, 0.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 10.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, -5.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, -5.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, -5.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 10.00000f, 5.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, 5.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, 5.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, 5.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(0.00000f, 10.00000f, -5.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, -5.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, -5.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, -5.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 10.00000f, 0.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, 0.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, 0.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 10.00000f, 5.00000f, 1.0f),
                 Point4D(0.00000f, 0.00000f, 5.00000f, 1.0f),
                 Point4D(10.00000f, 0.00000f, 5.00000f, 1.0f),
                 Point4D(10.00000f, 10.00000f, 5.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, NoSplit2)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-5.00000f, -5.00000f, 0.00000f, 1.0f),
                 Point4D(-5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, -5.00000f, 0.00000f, 1.0f)},
      MozPolygon{Point4D(0.00000f, 5.00000f, -15.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, -15.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, -10.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, -10.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(0.00000f, 5.00000f, -15.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, -15.00000f, 1.0f),
                 Point4D(0.00000f, -5.00000f, -10.00000f, 1.0f),
                 Point4D(0.00000f, 5.00000f, -10.00000f, 1.0f)},
      MozPolygon{Point4D(-5.00000f, -5.00000f, 0.00000f, 1.0f),
                 Point4D(-5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, 5.00000f, 0.00000f, 1.0f),
                 Point4D(5.00000f, -5.00000f, 0.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate0degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(-0.00000f, -2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, -2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00000f, 2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00000f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, 2.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, 0.00000f, 2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00000f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(-0.00000f, -2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, -2.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate20degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate40degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(-0.00000f, -2.73200f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, 0.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(-0.00000f, -2.73200f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, 0.73210f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate60degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(-0.00000f, -0.73200f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, 2.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-2.00000f, 1.26793f, 0.73210f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, 1.26793f, 0.73210f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(-0.00000f, -0.73200f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, 2.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.26793f, 0.73210f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.26793f, 0.73210f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate80degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate100degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(-0.00000f, 0.73210f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, -2.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -1.26783f, -0.73200f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.26783f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(-0.00000f, 0.73210f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, -2.73200f, 1.0f)},
      MozPolygon{Point4D(-2.00000f, -1.26783f, -0.73200f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, -1.26783f, -0.73200f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate120degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(-0.00000f, -2.73200f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, 0.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(-0.00000f, -2.73200f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, 0.73210f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate140degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate160degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(-0.00000f, -2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, -2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(-0.00000f, -2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, -2.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate180degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate200degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate220degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(-0.00000f, 2.73210f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(-0.00000f, 2.73210f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate240degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(-0.00000f, -0.73200f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, 2.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-2.00000f, 1.26793f, 0.73210f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(2.00000f, 1.26793f, 0.73210f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(-0.00000f, -0.73200f, -2.73200f, 1.0f),
                 Point4D(0.00010f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(0.00010f, 0.73210f, 2.73210f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.26793f, 0.73210f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(-2.00000f, 1.26793f, 0.73210f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate260degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)},
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(-0.00000f, -2.56340f, 1.19540f, 1.0f),
                 Point4D(0.00010f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(0.00010f, 2.56350f, -1.19530f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate280degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(-0.00000f, 0.73210f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, -2.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(2.00000f, -1.26783f, -0.73200f, 1.0f),
                 Point4D(2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, -1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.26783f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(-0.00000f, 2.73210f, -0.73200f, 1.0f),
                 Point4D(-0.00000f, 0.73210f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, -2.73200f, 1.0f)},
      MozPolygon{Point4D(-2.00000f, -1.26783f, -0.73200f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, 1.73210f, 1.00010f, 1.0f),
                 Point4D(2.00000f, -1.26783f, -0.73200f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate300degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(-0.00000f, 2.73210f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, 0.73210f, -2.73200f, 1.0f),
                 Point4D(-0.00000f, 2.73210f, 0.73210f, 1.0f),
                 Point4D(0.00010f, -0.73200f, 2.73210f, 1.0f),
                 Point4D(0.00010f, -2.73200f, -0.73200f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 1.73210f, -0.99990f, 1.0f),
                 Point4D(2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, -1.73200f, 1.00000f, 1.0f),
                 Point4D(-2.00000f, 1.73210f, -0.99990f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate320degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -1.19530f, -2.56340f, 1.0f),
                 Point4D(-0.00000f, 2.56350f, -1.19530f, 1.0f),
                 Point4D(0.00010f, 1.19540f, 2.56350f, 1.0f),
                 Point4D(0.00010f, -2.56340f, 1.19540f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.68410f, -1.87930f, 1.0f),
                 Point4D(2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, -0.68400f, 1.87940f, 1.0f),
                 Point4D(-2.00000f, 0.68410f, -1.87930f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate340degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate360degrees)
{
  const std::deque<MozPolygon> polygons{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};

  const std::deque<MozPolygon> expected{
      MozPolygon{Point4D(-0.00000f, -2.00000f, -2.00000f, 1.0f),
                 Point4D(-0.00000f, 2.00000f, -2.00000f, 1.0f),
                 Point4D(0.00010f, 2.00000f, 2.00000f, 1.0f),
                 Point4D(0.00010f, -2.00000f, 2.00000f, 1.0f)},
      MozPolygon{Point4D(2.00000f, 0.00010f, -2.00000f, 1.0f),
                 Point4D(2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, -0.00000f, 2.00000f, 1.0f),
                 Point4D(-2.00000f, 0.00010f, -2.00000f, 1.0f)}};
  ::RunTest(polygons, expected);
}
