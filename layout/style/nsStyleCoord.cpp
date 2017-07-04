/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of length values in computed style data */

#include "mozilla/HashFunctions.h"
#include "mozilla/PodOperations.h"

// nsStyleCoord.h must not be the first header in a unified source file,
// otherwise it may not build with MSVC due to a bug in our STL wrapper.
// See bug 1331102.
#include "nsStyleCoord.h"

using namespace mozilla;

nsStyleCoord::nsStyleCoord(nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit < eStyleUnit_Percent, "not a valueless unit");
  if (aUnit >= eStyleUnit_Percent) {
    mUnit = eStyleUnit_Null;
  }
  mValue.mInt = 0;
}

nsStyleCoord::nsStyleCoord(int32_t aValue, nsStyleUnit aUnit)
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
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
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
    case eStyleUnit_FlexFraction:
      return mValue.mFloat == aOther.mValue.mFloat;
    case eStyleUnit_Coord:
    case eStyleUnit_Integer:
    case eStyleUnit_Enumerated:
      return mValue.mInt == aOther.mValue.mInt;
    case eStyleUnit_Calc:
      return *this->GetCalcValue() == *aOther.GetCalcValue();
  }
  MOZ_ASSERT(false, "unexpected unit");
  return false;
}

void nsStyleCoord::Reset()
{
  Reset(mUnit, mValue);
}

void nsStyleCoord::SetCoordValue(nscoord aValue)
{
  Reset();
  mUnit = eStyleUnit_Coord;
  mValue.mInt = aValue;
}

void nsStyleCoord::SetIntValue(int32_t aValue, nsStyleUnit aUnit)
{
  NS_ASSERTION((aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  Reset();
  if ((aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Integer)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsStyleCoord::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_Percent;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetFactorValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_Factor;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetAngleValue(float aValue, nsStyleUnit aUnit)
{
  Reset();
  if (aUnit == eStyleUnit_Degree ||
      aUnit == eStyleUnit_Grad ||
      aUnit == eStyleUnit_Radian ||
      aUnit == eStyleUnit_Turn) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  } else {
    NS_NOTREACHED("not an angle value");
  }
}

void nsStyleCoord::SetFlexFractionValue(float aValue)
{
  Reset();
  mUnit = eStyleUnit_FlexFraction;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetCalcValue(Calc* aValue)
{
  Reset();
  mUnit = eStyleUnit_Calc;
  mValue.mPointer = aValue;
  aValue->AddRef();
}

void nsStyleCoord::SetNormalValue()
{
  Reset();
  mUnit = eStyleUnit_Normal;
  mValue.mInt = 0;
}

void nsStyleCoord::SetAutoValue()
{
  Reset();
  mUnit = eStyleUnit_Auto;
  mValue.mInt = 0;
}

void nsStyleCoord::SetNoneValue()
{
  Reset();
  mUnit = eStyleUnit_None;
  mValue.mInt = 0;
}

// accessors that are not inlined

double
nsStyleCoord::GetAngleValueInDegrees() const
{
  return GetAngleValueInRadians() * (180.0 / M_PI);
}

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
  NS_FOR_CSS_SIDES(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  mozilla::PodArrayZero(mValues);
}

nsStyleSides::nsStyleSides(const nsStyleSides& aOther)
{
  NS_FOR_CSS_SIDES(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  *this = aOther;
}

nsStyleSides::~nsStyleSides()
{
  Reset();
}

nsStyleSides&
nsStyleSides::operator=(const nsStyleSides& aCopy)
{
  if (this != &aCopy) {
    NS_FOR_CSS_SIDES(i) {
      nsStyleCoord::SetValue(mUnits[i], mValues[i],
                             aCopy.mUnits[i], aCopy.mValues[i]);
    }
  }
  return *this;
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
  NS_FOR_CSS_SIDES(i) {
    nsStyleCoord::Reset(mUnits[i], mValues[i]);
  }
}

nsStyleCorners::nsStyleCorners()
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  mozilla::PodArrayZero(mValues);
}

nsStyleCorners::nsStyleCorners(const nsStyleCorners& aOther)
{
  NS_FOR_CSS_HALF_CORNERS(i) {
    mUnits[i] = eStyleUnit_Null;
  }
  *this = aOther;
}

nsStyleCorners::~nsStyleCorners()
{
  Reset();
}

nsStyleCorners&
nsStyleCorners::operator=(const nsStyleCorners& aCopy)
{
  if (this != &aCopy) {
    NS_FOR_CSS_HALF_CORNERS(i) {
      nsStyleCoord::SetValue(mUnits[i], mValues[i],
                             aCopy.mUnits[i], aCopy.mValues[i]);
    }
  }
  return *this;
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
  NS_FOR_CSS_HALF_CORNERS(i) {
    nsStyleCoord::Reset(mUnits[i], mValues[i]);
  }
}

// Validation of SideIsVertical.
#define CASE(side, result)                                                    \
  static_assert(SideIsVertical(side) == result,                               \
                "SideIsVertical is wrong")
CASE(eSideTop,    false);
CASE(eSideRight,  true);
CASE(eSideBottom, false);
CASE(eSideLeft,   true);
#undef CASE

// Validation of HalfCornerIsX.
#define CASE(corner, result)                                                  \
  static_assert(HalfCornerIsX(corner) == result,                              \
                "HalfCornerIsX is wrong")
CASE(eCornerTopLeftX,     true);
CASE(eCornerTopLeftY,     false);
CASE(eCornerTopRightX,    true);
CASE(eCornerTopRightY,    false);
CASE(eCornerBottomRightX, true);
CASE(eCornerBottomRightY, false);
CASE(eCornerBottomLeftX,  true);
CASE(eCornerBottomLeftY,  false);
#undef CASE

// Validation of HalfToFullCorner.
#define CASE(corner, result)                                                  \
  static_assert(HalfToFullCorner(corner) == result,                           \
                "HalfToFullCorner is wrong")
CASE(eCornerTopLeftX,     eCornerTopLeft);
CASE(eCornerTopLeftY,     eCornerTopLeft);
CASE(eCornerTopRightX,    eCornerTopRight);
CASE(eCornerTopRightY,    eCornerTopRight);
CASE(eCornerBottomRightX, eCornerBottomRight);
CASE(eCornerBottomRightY, eCornerBottomRight);
CASE(eCornerBottomLeftX,  eCornerBottomLeft);
CASE(eCornerBottomLeftY,  eCornerBottomLeft);
#undef CASE

// Validation of FullToHalfCorner.
#define CASE(corner, vert, result)                                            \
  static_assert(FullToHalfCorner(corner, vert) == result,                     \
                "FullToHalfCorner is wrong")
CASE(eCornerTopLeft,     false, eCornerTopLeftX);
CASE(eCornerTopLeft,     true,  eCornerTopLeftY);
CASE(eCornerTopRight,    false, eCornerTopRightX);
CASE(eCornerTopRight,    true,  eCornerTopRightY);
CASE(eCornerBottomRight, false, eCornerBottomRightX);
CASE(eCornerBottomRight, true,  eCornerBottomRightY);
CASE(eCornerBottomLeft,  false, eCornerBottomLeftX);
CASE(eCornerBottomLeft,  true,  eCornerBottomLeftY);
#undef CASE

// Validation of SideToFullCorner.
#define CASE(side, second, result)                                            \
  static_assert(SideToFullCorner(side, second) == result,                     \
                "SideToFullCorner is wrong")
CASE(eSideTop,    false, eCornerTopLeft);
CASE(eSideTop,    true,  eCornerTopRight);

CASE(eSideRight,  false, eCornerTopRight);
CASE(eSideRight,  true,  eCornerBottomRight);

CASE(eSideBottom, false, eCornerBottomRight);
CASE(eSideBottom, true,  eCornerBottomLeft);

CASE(eSideLeft,   false, eCornerBottomLeft);
CASE(eSideLeft,   true,  eCornerTopLeft);
#undef CASE

// Validation of SideToHalfCorner.
#define CASE(side, second, parallel, result)                                  \
  static_assert(SideToHalfCorner(side, second, parallel) == result,           \
                "SideToHalfCorner is wrong")
CASE(eSideTop,    false, true,  eCornerTopLeftX);
CASE(eSideTop,    false, false, eCornerTopLeftY);
CASE(eSideTop,    true,  true,  eCornerTopRightX);
CASE(eSideTop,    true,  false, eCornerTopRightY);

CASE(eSideRight,  false, false, eCornerTopRightX);
CASE(eSideRight,  false, true,  eCornerTopRightY);
CASE(eSideRight,  true,  false, eCornerBottomRightX);
CASE(eSideRight,  true,  true,  eCornerBottomRightY);

CASE(eSideBottom, false, true,  eCornerBottomRightX);
CASE(eSideBottom, false, false, eCornerBottomRightY);
CASE(eSideBottom, true,  true,  eCornerBottomLeftX);
CASE(eSideBottom, true,  false, eCornerBottomLeftY);

CASE(eSideLeft,   false, false, eCornerBottomLeftX);
CASE(eSideLeft,   false, true,  eCornerBottomLeftY);
CASE(eSideLeft,   true,  false, eCornerTopLeftX);
CASE(eSideLeft,   true,  true,  eCornerTopLeftY);
#undef CASE
