/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include <cmath>

#include "BasePoint.h"
#include "Units.h"
#include "Point.h"

using mozilla::CSSCoord;
using mozilla::CSSIntCoord;
using mozilla::CSSIntPoint;
using mozilla::CSSPixel;
using mozilla::gfx::CoordTyped;
using mozilla::gfx::FloatType_t;
using mozilla::gfx::IntCoordTyped;
using mozilla::gfx::IntPointTyped;
using mozilla::gfx::PointTyped;
using mozilla::gfx::UnknownUnits;

TEST(Gfx, TestCSSIntPointLength)
{
  CSSIntPoint testPnt(1, 1);
  float res = testPnt.Length();

  float epsilon = 0.001f;
  constexpr float sqrtOfTwo = 1.414;
  float diff = std::abs(res - sqrtOfTwo);
  EXPECT_LT(diff, epsilon);
}

TEST(Gfx, TestPointAddition)
{
  PointTyped<CSSPixel> a, b;
  a.x = 2;
  a.y = 2;
  b.x = 5;
  b.y = -5;

  a += b;

  EXPECT_EQ(a.x, 7.f);
  EXPECT_EQ(a.y, -3.f);
}

TEST(Gfx, TestPointSubtraction)
{
  PointTyped<CSSPixel> a, b;
  a.x = 2;
  a.y = 2;
  b.x = 5;
  b.y = -5;

  a -= b;

  EXPECT_EQ(a.x, -3.f);
  EXPECT_EQ(a.y, 7.f);
}

TEST(Gfx, TestPointRoundToMultiple)
{
  const int32_t roundTo = 2;

  IntPointTyped<CSSPixel> p(478, -394);
  EXPECT_EQ(p.RoundedToMultiple(roundTo), p);

  IntPointTyped<CSSPixel> p2(478, 393);
  EXPECT_NE(p2.RoundedToMultiple(roundTo), p2);
}

TEST(Gfx, TestFloatTypeMeta)
{
  // TODO: Should int64_t's FloatType be double instead?
  static_assert(std::is_same_v<FloatType_t<int64_t>, float>,
                "The FloatType of an integer type should be float");
  static_assert(std::is_same_v<FloatType_t<int32_t>, float>,
                "The FloatType of an integer type should be float");
  static_assert(
      std::is_same_v<FloatType_t<float>, float>,
      "The FloatType of a floating-point type should be the given type");
  static_assert(
      std::is_same_v<FloatType_t<double>, double>,
      "The FloatType of a floating-point type should be the given type");
  static_assert(
      std::is_same_v<FloatType_t<IntCoordTyped<UnknownUnits, int32_t>>,
                     CoordTyped<UnknownUnits, float>>,
      "The FloatType of an IntCoordTyped<Units, Rep> should be "
      "CoordTyped<Units, float>");
  static_assert(
      std::is_same_v<FloatType_t<CoordTyped<UnknownUnits, long double>>,
                     CoordTyped<UnknownUnits, long double>>,
      "The FloatType of a CoordTyped<Units, Rep> should be "
      "CoordTyped<Units, Rep>");
}
