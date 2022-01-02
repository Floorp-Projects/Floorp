/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsCoord.h"

namespace mozilla {

template <typename CoordTyped>
void TestConstructors() {
  CoordTyped coord1;
  EXPECT_EQ(coord1.value, 0);

  CoordTyped coord2 = 6000;
  EXPECT_EQ(coord2.value, 6000);

  EXPECT_EQ(CoordTyped::FromRound(5.3).value, 5);
  EXPECT_EQ(CoordTyped::FromRound(5.5).value, 6);
  EXPECT_EQ(CoordTyped::FromRound(-2.5).value, -2);

  EXPECT_EQ(CoordTyped::FromTruncate(5.9).value, 5);
  EXPECT_EQ(CoordTyped::FromTruncate(-2.7).value, -2);

  EXPECT_EQ(CoordTyped::FromCeil(5.3).value, 6);
  EXPECT_EQ(CoordTyped::FromCeil(-2.7).value, -2);

  EXPECT_EQ(CoordTyped::FromFloor(5.9).value, 5);
  EXPECT_EQ(CoordTyped::FromFloor(-2.7).value, -3);
}

template <typename CoordTyped>
void TestComparisonOperators() {
  CoordTyped coord1 = 6000;
  CoordTyped coord2 = 10000;

  EXPECT_LT(coord1, CoordTyped::kMax);
  EXPECT_GT(coord1, CoordTyped::kMin);
  EXPECT_NE(coord1, coord2);
}

template <typename CoordTyped>
void TestArithmeticOperators() {
  CoordTyped coord1 = 6000;
  CoordTyped coord2 = 10000;

  EXPECT_EQ(coord1 + coord2, CoordTyped(16000));
  EXPECT_EQ(coord2 - coord1, CoordTyped(4000));

  decltype(CoordTyped::value) scaleInt = 2;
  EXPECT_EQ(coord1 * scaleInt, CoordTyped(12000));
  EXPECT_EQ(coord1 / scaleInt, CoordTyped(3000));

  EXPECT_EQ(coord1 * 2.0, 12000.0);
  EXPECT_EQ(coord1 / 2.0, 3000.0);
}

template <typename CoordTyped>
void TestClamp() {
  CoordTyped coord1 = CoordTyped::kMax + 1000;
  EXPECT_EQ(coord1.ToMinMaxClamped(), CoordTyped::kMax);

  CoordTyped coord2 = CoordTyped::kMin - 1000;
  EXPECT_EQ(coord2.ToMinMaxClamped(), CoordTyped::kMin);

  CoordTyped coord3 = 12000;
  EXPECT_EQ(coord3.ToMinMaxClamped(), CoordTyped(12000));
}

TEST(Gfx, TestAuCoord)
{
  TestConstructors<AuCoord>();
  TestComparisonOperators<AuCoord>();
  TestArithmeticOperators<AuCoord>();
  TestClamp<AuCoord>();
}

TEST(Gfx, TestAuCoord64)
{
  TestConstructors<AuCoord64>();
  TestComparisonOperators<AuCoord64>();
  TestArithmeticOperators<AuCoord64>();
  TestClamp<AuCoord64>();
}

}  // namespace mozilla
