/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsStyleCoord.h"
#include "nsString.h"
#include "nsCRT.h"

nsStyleCoord::nsStyleCoord(nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit < eStyleUnit_Percent, "not a valueless unit");
  if (aUnit >= eStyleUnit_Percent) {
    mUnit = eStyleUnit_Null;
  }
  mValue.mInt = 0;
}

nsStyleCoord::nsStyleCoord(nscoord aValue)
  : mUnit(eStyleUnit_Coord)
{
  mValue.mInt = aValue;
}

nsStyleCoord::nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  //if you want to pass in eStyleUnit_Coord, don't. instead, use the
  //constructor just above this one... MMP
  NS_ASSERTION((aUnit == eStyleUnit_Proportional) ||
               (aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  if ((aUnit == eStyleUnit_Proportional) ||
      (aUnit == eStyleUnit_Enumerated) ||
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
  NS_ASSERTION((aUnit == eStyleUnit_Percent) ||
               (aUnit == eStyleUnit_Factor), "not a float value");
  if ((aUnit == eStyleUnit_Percent) ||
      (aUnit == eStyleUnit_Factor)) {
    mValue.mFloat = aValue;
  }
  else {
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
  }
}

nsStyleCoord::nsStyleCoord(const nsStyleCoord& aCopy)
  : mUnit(aCopy.mUnit)
{
  if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
}

nsStyleCoord& nsStyleCoord::operator=(const nsStyleCoord& aCopy)
{
  mUnit = aCopy.mUnit;
  if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
  return *this;
}

PRBool nsStyleCoord::operator==(const nsStyleCoord& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
    else {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
  }
  return PR_FALSE;
}

void nsStyleCoord::Reset(void)
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
  NS_ASSERTION((aUnit == eStyleUnit_Proportional) ||
               (aUnit == eStyleUnit_Enumerated) ||
               (aUnit == eStyleUnit_Chars) ||
               (aUnit == eStyleUnit_Integer), "not an int value");
  if ((aUnit == eStyleUnit_Proportional) ||
      (aUnit == eStyleUnit_Enumerated) ||
      (aUnit == eStyleUnit_Chars) ||
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

void nsStyleCoord::SetNormalValue(void)
{
  mUnit = eStyleUnit_Normal;
  mValue.mInt = 0;
}

void nsStyleCoord::SetAutoValue(void)
{
  mUnit = eStyleUnit_Auto;
  mValue.mInt = 0;
}

void nsStyleCoord::SetInheritValue(void)
{
  mUnit = eStyleUnit_Inherit;
  mValue.mInt = 0;
}

void nsStyleCoord::SetUnionValue(const nsStyleUnion& aValue, nsStyleUnit aUnit)
{
  mUnit = aUnit;
#if PR_BYTES_PER_INT == PR_BYTES_PER_FLOAT
  mValue.mInt = aValue.mInt;
#else
  nsCRT::memcpy(&mValue, &aValue, sizeof(nsStyleUnion));
#endif
}

void nsStyleCoord::AppendToString(nsString& aBuffer) const
{
  if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
    aBuffer.Append(mValue.mFloat);
  }
  else if ((eStyleUnit_Coord == mUnit) || 
           (eStyleUnit_Proportional == mUnit) ||
           (eStyleUnit_Enumerated == mUnit) ||
           (eStyleUnit_Integer == mUnit)) {
    aBuffer.AppendWithConversion(mValue.mInt, 10);
    aBuffer.AppendWithConversion("[0x");
    aBuffer.AppendWithConversion(mValue.mInt, 16);
    aBuffer.AppendWithConversion(']');
  }

  switch (mUnit) {
    case eStyleUnit_Null:         aBuffer.AppendWithConversion("Null");     break;
    case eStyleUnit_Coord:        aBuffer.AppendWithConversion("tw");       break;
    case eStyleUnit_Percent:      aBuffer.AppendWithConversion("%");        break;
    case eStyleUnit_Factor:       aBuffer.AppendWithConversion("f");        break;
    case eStyleUnit_Normal:       aBuffer.AppendWithConversion("Normal");   break;
    case eStyleUnit_Auto:         aBuffer.AppendWithConversion("Auto");     break;
    case eStyleUnit_Inherit:      aBuffer.AppendWithConversion("Inherit");  break;
    case eStyleUnit_Proportional: aBuffer.AppendWithConversion("*");        break;
    case eStyleUnit_Enumerated:   aBuffer.AppendWithConversion("enum");     break;
    case eStyleUnit_Integer:      aBuffer.AppendWithConversion("int");      break;
    case eStyleUnit_Chars:        aBuffer.AppendWithConversion("chars");    break;
  }
  aBuffer.Append(' ');
}

void nsStyleCoord::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}




nsStyleSides::nsStyleSides(void)
{
  nsCRT::memset(this, 0x00, sizeof(nsStyleSides));
}

#define COMPARE_SIDE(side)                                                            \
  if ((eStyleUnit_Percent <= m##side##Unit) && (m##side##Unit < eStyleUnit_Coord)) {  \
    if (m##side##Value.mFloat != aOther.m##side##Value.mFloat)                        \
      return PR_FALSE;                                                                \
  }                                                                                   \
  else {                                                                              \
    if (m##side##Value.mInt != aOther.m##side##Value.mInt)                            \
      return PR_FALSE;                                                                \
  }

PRBool nsStyleSides::operator==(const nsStyleSides& aOther) const
{
  if ((mLeftUnit == aOther.mLeftUnit) && 
      (mTopUnit == aOther.mTopUnit) &&
      (mRightUnit == aOther.mRightUnit) &&
      (mBottomUnit == aOther.mBottomUnit)) {
    COMPARE_SIDE(Left);
    COMPARE_SIDE(Top);
    COMPARE_SIDE(Right);
    COMPARE_SIDE(Bottom);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsStyleSides::Reset(void)
{
  nsCRT::memset(this, 0x00, sizeof(nsStyleSides));
}

void nsStyleSides::AppendToString(nsString& aBuffer) const
{
  nsStyleCoord  temp;

  GetLeft(temp);
  aBuffer.AppendWithConversion("left: ");
  temp.AppendToString(aBuffer);

  GetTop(temp);
  aBuffer.AppendWithConversion("top: ");
  temp.AppendToString(aBuffer);

  GetRight(temp);
  aBuffer.AppendWithConversion("right: ");
  temp.AppendToString(aBuffer);

  GetBottom(temp);
  aBuffer.AppendWithConversion("bottom: ");
  temp.AppendToString(aBuffer);
}

void nsStyleSides::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}

