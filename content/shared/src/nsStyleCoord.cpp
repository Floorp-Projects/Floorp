/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

void nsStyleCoord::SetUnionValue(const nsStyleUnion& aValue, nsStyleUnit aUnit)
{
  mUnit = aUnit;
#if PR_BYTES_PER_INT == PR_BYTES_PER_FLOAT
  mValue.mInt = aValue.mInt;
#else
  memcpy(&mValue, &aValue, sizeof(nsStyleUnion));
#endif
}

void nsStyleCoord::AppendToString(nsString& aBuffer) const
{
  if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
    aBuffer.AppendFloat(mValue.mFloat);
  }
  else if ((eStyleUnit_Coord == mUnit) || 
           (eStyleUnit_Proportional == mUnit) ||
           (eStyleUnit_Enumerated == mUnit) ||
           (eStyleUnit_Integer == mUnit)) {
    aBuffer.AppendInt(mValue.mInt, 10);
    aBuffer.AppendLiteral("[0x");
    aBuffer.AppendInt(mValue.mInt, 16);
    aBuffer.Append(PRUnichar(']'));
  }

  switch (mUnit) {
    case eStyleUnit_Null:         aBuffer.AppendLiteral("Null");     break;
    case eStyleUnit_Coord:        aBuffer.AppendLiteral("tw");       break;
    case eStyleUnit_Percent:      aBuffer.AppendLiteral("%");        break;
    case eStyleUnit_Factor:       aBuffer.AppendLiteral("f");        break;
    case eStyleUnit_Normal:       aBuffer.AppendLiteral("Normal");   break;
    case eStyleUnit_Auto:         aBuffer.AppendLiteral("Auto");     break;
    case eStyleUnit_Proportional: aBuffer.AppendLiteral("*");        break;
    case eStyleUnit_Enumerated:   aBuffer.AppendLiteral("enum");     break;
    case eStyleUnit_Integer:      aBuffer.AppendLiteral("int");      break;
    case eStyleUnit_Chars:        aBuffer.AppendLiteral("chars");    break;
  }
  aBuffer.Append(PRUnichar(' '));
}

void nsStyleCoord::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}




nsStyleSides::nsStyleSides(void)
{
  memset(this, 0x00, sizeof(nsStyleSides));
}

#define COMPARE_SIDE(side)                                                    \
  if ((eStyleUnit_Percent <= mUnits[side]) &&                                 \
      (mUnits[side] < eStyleUnit_Coord)) {                                    \
    if (mValues[side].mFloat != aOther.mValues[side].mFloat)                  \
      return PR_FALSE;                                                        \
  }                                                                           \
  else {                                                                      \
    if (mValues[side].mInt != aOther.mValues[side].mInt)                      \
      return PR_FALSE;                                                        \
  }

PRBool nsStyleSides::operator==(const nsStyleSides& aOther) const
{
  if ((mUnits[NS_SIDE_LEFT] == aOther.mUnits[NS_SIDE_LEFT]) && 
      (mUnits[NS_SIDE_TOP] == aOther.mUnits[NS_SIDE_TOP]) &&
      (mUnits[NS_SIDE_RIGHT] == aOther.mUnits[NS_SIDE_RIGHT]) &&
      (mUnits[NS_SIDE_BOTTOM] == aOther.mUnits[NS_SIDE_BOTTOM])) {
    COMPARE_SIDE(NS_SIDE_LEFT);
    COMPARE_SIDE(NS_SIDE_TOP);
    COMPARE_SIDE(NS_SIDE_RIGHT);
    COMPARE_SIDE(NS_SIDE_BOTTOM);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsStyleSides::Reset(void)
{
  memset(this, 0x00, sizeof(nsStyleSides));
}

void nsStyleSides::AppendToString(nsString& aBuffer) const
{
  nsStyleCoord  temp;

  GetLeft(temp);
  aBuffer.AppendLiteral("left: ");
  temp.AppendToString(aBuffer);

  GetTop(temp);
  aBuffer.AppendLiteral("top: ");
  temp.AppendToString(aBuffer);

  GetRight(temp);
  aBuffer.AppendLiteral("right: ");
  temp.AppendToString(aBuffer);

  GetBottom(temp);
  aBuffer.AppendLiteral("bottom: ");
  temp.AppendToString(aBuffer);
}

void nsStyleSides::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}

