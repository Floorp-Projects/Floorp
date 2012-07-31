/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of length values in computed style data */

#include "nsStyleCoord.h"
#include "nsString.h"
#include "nsCRT.h"
#include "prlog.h"
#include "nsMathUtils.h"
#include "nsStyleContext.h"

nsStyleCoord::nsStyleCoord(nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit < eStyleUnit_Percent, "not a valueless unit");
  if (aUnit >= eStyleUnit_Percent) {
    mUnit = eStyleUnit_Null;
  }
  mValue.mInt = 0;
}

nsStyleCoord::nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  //if you want to pass in eStyleUnit_Coord, don't. instead, use the
  //constructor just above this one... MMP
  NS_ASSERTION((aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  if ((aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Integer)) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
  }
}

nsStyleCoord::nsStyleCoord(float aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  if (aUnit < eStyleUnit_Percent || aUnit >= eStyleUnit_Coord) {
    NS_NOTREACHED("not a float value");
    Reset();
  } else {
    mValue.mFloat = aValue;
  }
}

bool nsStyleCoord::operator==(const nsStyleCoord& aOther) const
{
  if (mUnit != aOther.mUnit) {
    return false;
  }
  switch (mUnit) {
    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Auto:
    case eStyleUnit_None:
      return true;
    case eStyleUnit_Percent:
    case eStyleUnit_Factor:
    case eStyleUnit_Degree:
    case eStyleUnit_Grad:
    case eStyleUnit_Radian:
    case eStyleUnit_Turn:
      return mValue.mFloat == aOther.mValue.mFloat;
    case eStyleUnit_Coord:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
      return mValue.mInt == aOther.mValue.mInt;
    case eStyleUnit_Calc:
      return *this->GetCalcValue() == *aOther.GetCalcValue();
  }
  NS_ABORT_IF_FALSE(false, "unexpected unit");
  return false;
}

void nsStyleCoord::Reset()
{
  mUnit = eStyleUnit_Null;
  mValue.mInt = 0;
}

void nsStyleCoord::SetCoordValue(nscoord aValue)
{
  mUnit = eStyleUnit_Coord;
  mValue.mInt = aValue;
}

void nsStyleCoord::SetIntValue(PRInt32 aValue, nsStyleUnit aUnit)
{
  NS_ASSERTION((aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  if ((aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Integer)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
  else {
    Reset();
  }
}

void nsStyleCoord::SetPercentValue(float aValue)
{
  mUnit = eStyleUnit_Percent;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetFactorValue(float aValue)
{
  mUnit = eStyleUnit_Factor;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetAngleValue(float aValue, nsStyleUnit aUnit)
{
  if (aUnit == eStyleUnit_Degree ||
      aUnit == eStyleUnit_Grad ||
      aUnit == eStyleUnit_Radian ||
      aUnit == eStyleUnit_Turn) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  } else {
    NS_NOTREACHED("not an angle value");
    Reset();
  }
}

void nsStyleCoord::SetCalcValue(Calc* aValue)
{
  mUnit = eStyleUnit_Calc;
  mValue.mPointer = aValue;
}

void nsStyleCoord::SetNormalValue()
{
  mUnit = eStyleUnit_Normal;
  mValue.mInt = 0;
}

void nsStyleCoord::SetAutoValue()
{
  mUnit = eStyleUnit_Auto;
  mValue.mInt = 0;
}

void nsStyleCoord::SetNoneValue()
{
  mUnit = eStyleUnit_None;
  mValue.mInt = 0;
}

// accessors that are not inlined

double
nsStyleCoord::GetAngleValueInRadians() const
{
  double angle = mValue.mFloat;

  switch (GetUnit()) {
  case eStyleUnit_Radian: return angle;
  case eStyleUnit_Turn:   return angle * 2 * M_PI;
  case eStyleUnit_Degree: return angle * M_PI / 180.0;
  case eStyleUnit_Grad:   return angle * M_PI / 200.0;

  default:
    NS_NOTREACHED("unrecognized angular unit");
    return 0.0;
  }
}

nsStyleSides::nsStyleSides()
{
  memset(this, 0x00, sizeof(nsStyleSides));
}

bool nsStyleSides::operator==(const nsStyleSides& aOther) const
{
  NS_FOR_CSS_SIDES(i) {
    if (nsStyleCoord(mValues[i], (nsStyleUnit)mUnits[i]) !=
        nsStyleCoord(aOther.mValues[i], (nsStyleUnit)aOther.mUnits[i])) {
      return false;
    }
  }
  return true;
}

void nsStyleSides::Reset()
{
  memset(this, 0x00, sizeof(nsStyleSides));
}

nsStyleCorners::nsStyleCorners()
{
  memset(this, 0x00, sizeof(nsStyleCorners));
}

bool
nsStyleCorners::operator==(const nsStyleCorners& aOther) const
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    if (nsStyleCoord(mValues[i], (nsStyleUnit)mUnits[i]) !=
        nsStyleCoord(aOther.mValues[i], (nsStyleUnit)aOther.mUnits[i])) {
      return false;
    }
  }
  return true;
}

void nsStyleCorners::Reset()
{
  memset(this, 0x00, sizeof(nsStyleCorners));
}

// Validation of NS_SIDE_IS_VERTICAL and NS_HALF_CORNER_IS_X.
#define CASE(side, result)                                                    \
  MOZ_STATIC_ASSERT(NS_SIDE_IS_VERTICAL(side) == result,                      \
                    "NS_SIDE_IS_VERTICAL is wrong")
CASE(NS_SIDE_TOP,    false);
CASE(NS_SIDE_RIGHT,  true);
CASE(NS_SIDE_BOTTOM, false);
CASE(NS_SIDE_LEFT,   true);
#undef CASE

#define CASE(corner, result)                                                  \
  MOZ_STATIC_ASSERT(NS_HALF_CORNER_IS_X(corner) == result,                    \
                    "NS_HALF_CORNER_IS_X is wrong")
CASE(NS_CORNER_TOP_LEFT_X,     true);
CASE(NS_CORNER_TOP_LEFT_Y,     false);
CASE(NS_CORNER_TOP_RIGHT_X,    true);
CASE(NS_CORNER_TOP_RIGHT_Y,    false);
CASE(NS_CORNER_BOTTOM_RIGHT_X, true);
CASE(NS_CORNER_BOTTOM_RIGHT_Y, false);
CASE(NS_CORNER_BOTTOM_LEFT_X,  true);
CASE(NS_CORNER_BOTTOM_LEFT_Y,  false);
#undef CASE

// Validation of NS_HALF_TO_FULL_CORNER.
#define CASE(corner, result)                                                  \
  MOZ_STATIC_ASSERT(NS_HALF_TO_FULL_CORNER(corner) == result,                 \
                    "NS_HALF_TO_FULL_CORNER is wrong")
CASE(NS_CORNER_TOP_LEFT_X,     NS_CORNER_TOP_LEFT);
CASE(NS_CORNER_TOP_LEFT_Y,     NS_CORNER_TOP_LEFT);
CASE(NS_CORNER_TOP_RIGHT_X,    NS_CORNER_TOP_RIGHT);
CASE(NS_CORNER_TOP_RIGHT_Y,    NS_CORNER_TOP_RIGHT);
CASE(NS_CORNER_BOTTOM_RIGHT_X, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_CORNER_BOTTOM_RIGHT_Y, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_CORNER_BOTTOM_LEFT_X,  NS_CORNER_BOTTOM_LEFT);
CASE(NS_CORNER_BOTTOM_LEFT_Y,  NS_CORNER_BOTTOM_LEFT);
#undef CASE

// Validation of NS_FULL_TO_HALF_CORNER.
#define CASE(corner, vert, result)                                            \
  MOZ_STATIC_ASSERT(NS_FULL_TO_HALF_CORNER(corner, vert) == result,           \
                    "NS_FULL_TO_HALF_CORNER is wrong")
CASE(NS_CORNER_TOP_LEFT,     false, NS_CORNER_TOP_LEFT_X);
CASE(NS_CORNER_TOP_LEFT,     true,  NS_CORNER_TOP_LEFT_Y);
CASE(NS_CORNER_TOP_RIGHT,    false, NS_CORNER_TOP_RIGHT_X);
CASE(NS_CORNER_TOP_RIGHT,    true,  NS_CORNER_TOP_RIGHT_Y);
CASE(NS_CORNER_BOTTOM_RIGHT, false, NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_CORNER_BOTTOM_RIGHT, true,  NS_CORNER_BOTTOM_RIGHT_Y);
CASE(NS_CORNER_BOTTOM_LEFT,  false, NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_CORNER_BOTTOM_LEFT,  true,  NS_CORNER_BOTTOM_LEFT_Y);
#undef CASE

// Validation of NS_SIDE_TO_{FULL,HALF}_CORNER.
#define CASE(side, second, result)                                            \
  MOZ_STATIC_ASSERT(NS_SIDE_TO_FULL_CORNER(side, second) == result,           \
                    "NS_SIDE_TO_FULL_CORNER is wrong")
CASE(NS_SIDE_TOP,    false, NS_CORNER_TOP_LEFT);
CASE(NS_SIDE_TOP,    true,  NS_CORNER_TOP_RIGHT);

CASE(NS_SIDE_RIGHT,  false, NS_CORNER_TOP_RIGHT);
CASE(NS_SIDE_RIGHT,  true,  NS_CORNER_BOTTOM_RIGHT);

CASE(NS_SIDE_BOTTOM, false, NS_CORNER_BOTTOM_RIGHT);
CASE(NS_SIDE_BOTTOM, true,  NS_CORNER_BOTTOM_LEFT);

CASE(NS_SIDE_LEFT,   false, NS_CORNER_BOTTOM_LEFT);
CASE(NS_SIDE_LEFT,   true,  NS_CORNER_TOP_LEFT);
#undef CASE

#define CASE(side, second, parallel, result)                                  \
  MOZ_STATIC_ASSERT(NS_SIDE_TO_HALF_CORNER(side, second, parallel) == result, \
                    "NS_SIDE_TO_HALF_CORNER is wrong")
CASE(NS_SIDE_TOP,    false, true,  NS_CORNER_TOP_LEFT_X);
CASE(NS_SIDE_TOP,    false, false, NS_CORNER_TOP_LEFT_Y);
CASE(NS_SIDE_TOP,    true,  true,  NS_CORNER_TOP_RIGHT_X);
CASE(NS_SIDE_TOP,    true,  false, NS_CORNER_TOP_RIGHT_Y);

CASE(NS_SIDE_RIGHT,  false, false, NS_CORNER_TOP_RIGHT_X);
CASE(NS_SIDE_RIGHT,  false, true,  NS_CORNER_TOP_RIGHT_Y);
CASE(NS_SIDE_RIGHT,  true,  false, NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_SIDE_RIGHT,  true,  true,  NS_CORNER_BOTTOM_RIGHT_Y);

CASE(NS_SIDE_BOTTOM, false, true,  NS_CORNER_BOTTOM_RIGHT_X);
CASE(NS_SIDE_BOTTOM, false, false, NS_CORNER_BOTTOM_RIGHT_Y);
CASE(NS_SIDE_BOTTOM, true,  true,  NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_SIDE_BOTTOM, true,  false, NS_CORNER_BOTTOM_LEFT_Y);

CASE(NS_SIDE_LEFT,   false, false, NS_CORNER_BOTTOM_LEFT_X);
CASE(NS_SIDE_LEFT,   false, true,  NS_CORNER_BOTTOM_LEFT_Y);
CASE(NS_SIDE_LEFT,   true,  false, NS_CORNER_TOP_LEFT_X);
CASE(NS_SIDE_LEFT,   true,  true,  NS_CORNER_TOP_LEFT_Y);
#undef CASE
