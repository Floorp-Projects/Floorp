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
#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nscore.h"
#include "nsCoord.h"
#include "nsCRT.h"
#include "nsString.h"

enum nsStyleUnit {
  eStyleUnit_Null         = 0,      // (no value) value is not specified
  eStyleUnit_Normal       = 1,      // (no value)
  eStyleUnit_Auto         = 2,      // (no value)
  eStyleUnit_Inherit      = 3,      // (no value) value should be inherited
  eStyleUnit_Percent      = 10,     // (float) 1.0 == 100%
  eStyleUnit_Factor       = 11,     // (float) a multiplier
  eStyleUnit_Coord        = 20,     // (nscoord) value is twips
  eStyleUnit_Integer      = 30,     // (int) value is simple integer
  eStyleUnit_Proportional = 31,     // (int) value has proportional meaning
  eStyleUnit_Enumerated   = 32,     // (int) value has enumerated meaning
  eStyleUnit_Chars        = 33      // (int) value is number of characters
};

typedef union {
  PRInt32     mInt;   // nscoord is a PRInt32 for now
  float       mFloat;
} nsStyleUnion;

class nsStyleCoord {
public:
  nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null)
    : mUnit(aUnit) {
    NS_ASSERTION(aUnit < eStyleUnit_Percent, "not a valueless unit");
    if (aUnit >= eStyleUnit_Percent) {
      mUnit = eStyleUnit_Null;
    }
    mValue.mInt = 0;
  }

  nsStyleCoord(nscoord aValue)
    : mUnit(eStyleUnit_Coord)  {
    mValue.mInt = aValue;
  }

  nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit)
    : mUnit(aUnit)  {
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

  nsStyleCoord(float aValue, nsStyleUnit aUnit)
    : mUnit(aUnit)  {
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

  nsStyleCoord(const nsStyleCoord& aCopy)
    : mUnit(aCopy.mUnit)  {
    if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
      mValue.mFloat = aCopy.mValue.mFloat;
    }
    else {
      mValue.mInt = aCopy.mValue.mInt;
    }
  }

  nsStyleCoord& operator=(const nsStyleCoord& aCopy) {
    mUnit = aCopy.mUnit;
    if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
      mValue.mFloat = aCopy.mValue.mFloat;
    }
    else {
      mValue.mInt = aCopy.mValue.mInt;
    }
    return *this;
  }

  PRBool operator==(const nsStyleCoord& aOther) const {
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

  PRBool operator!=(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit(void) const { return mUnit; }
  nscoord     GetCoordValue(void) const;
  PRInt32     GetIntValue(void) const;
  float       GetPercentValue(void) const;
  float       GetFactorValue(void) const;
  void        GetUnionValue(nsStyleUnion& aValue) const;

  void  Reset(void) {
    mUnit = eStyleUnit_Null;
    mValue.mInt = 0;
  }

  void  SetCoordValue(nscoord aValue) {
    mUnit = eStyleUnit_Coord;
    mValue.mInt = aValue;
  }

  void  SetIntValue(PRInt32 aValue, nsStyleUnit aUnit) {
    if ((aUnit == eStyleUnit_Proportional) ||
        (aUnit == eStyleUnit_Enumerated) ||
        (aUnit == eStyleUnit_Chars) ||
        (aUnit == eStyleUnit_Integer)) {
      mUnit = aUnit;
      mValue.mInt = aValue;
    }
    else {
      NS_WARNING("not an int value");
      Reset();
    }
  }

  void  SetPercentValue(float aValue) {
    mUnit = eStyleUnit_Percent;
    mValue.mFloat = aValue;
  }

  void  SetFactorValue(float aValue) {
    mUnit = eStyleUnit_Factor;
    mValue.mFloat = aValue;
  }

  void  SetNormalValue(void) {
    mUnit = eStyleUnit_Normal;
    mValue.mInt = 0;
  }

  void  SetAutoValue(void) {
    mUnit = eStyleUnit_Auto;
    mValue.mInt = 0;
  }

  void  SetInheritValue(void) {
    mUnit = eStyleUnit_Inherit;
    mValue.mInt = 0;
  }

  void  SetUnionValue(const nsStyleUnion& aValue, nsStyleUnit aUnit) {
    mUnit = aUnit;
#if PR_BYTES_PER_INT == PR_BYTES_PER_FLOAT
    mValue.mInt = aValue.mInt;
#else
    nsCRT::memcpy(&mValue, &aValue, sizeof(nsStyleUnion));
#endif
  }

  void  AppendToString(nsString& aBuffer) const {
    if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
      aBuffer.AppendFloat(mValue.mFloat);
    }
    else if ((eStyleUnit_Coord == mUnit) || 
             (eStyleUnit_Proportional == mUnit) ||
             (eStyleUnit_Enumerated == mUnit) ||
             (eStyleUnit_Integer == mUnit)) {
      aBuffer.AppendInt(mValue.mInt, 10);
      aBuffer.AppendWithConversion("[0x");
      aBuffer.AppendInt(mValue.mInt, 16);
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
    aBuffer.AppendWithConversion(' ');
  }

  void  ToString(nsString& aBuffer) const {
    aBuffer.Truncate();
    AppendToString(aBuffer);
  }

public:
  nsStyleUnit   mUnit;
  nsStyleUnion  mValue;
};


#define COMPARE_SIDE(side)                                                    \
  if ((eStyleUnit_Percent <= m##side##Unit) &&                                \
      (m##side##Unit < eStyleUnit_Coord)) {                                   \
    if (m##side##Value.mFloat != aOther.m##side##Value.mFloat)                \
      return PR_FALSE;                                                        \
  }                                                                           \
  else {                                                                      \
    if (m##side##Value.mInt != aOther.m##side##Value.mInt)                    \
      return PR_FALSE;                                                        \
  }

class nsStyleSides {
public:
  nsStyleSides(void) {
    nsCRT::memset(this, 0x00, sizeof(nsStyleSides));
  }

//  nsStyleSides&  operator=(const nsStyleSides& aCopy);  // use compiler's version
  PRBool         operator==(const nsStyleSides& aOther) const {
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

  PRBool         operator!=(const nsStyleSides& aOther) const;

  nsStyleUnit GetLeftUnit(void) const;
  nsStyleUnit GetTopUnit(void) const;
  nsStyleUnit GetRightUnit(void) const;
  nsStyleUnit GetBottomUnit(void) const;

  nsStyleCoord& GetLeft(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetTop(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetRight(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetBottom(nsStyleCoord& aCoord) const;

  void  Reset(void) {
    nsCRT::memset(this, 0x00, sizeof(nsStyleSides));
  }

  void  SetLeft(const nsStyleCoord& aCoord);
  void  SetTop(const nsStyleCoord& aCoord);
  void  SetRight(const nsStyleCoord& aCoord);
  void  SetBottom(const nsStyleCoord& aCoord);

  void  AppendToString(nsString& aBuffer) const {
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

  void  ToString(nsString& aBuffer) const {
    aBuffer.Truncate();
    AppendToString(aBuffer);
  }

protected:
  PRUint8       mLeftUnit;
  PRUint8       mTopUnit;
  PRUint8       mRightUnit;
  PRUint8       mBottomUnit;
  nsStyleUnion  mLeftValue;
  nsStyleUnion  mTopValue;
  nsStyleUnion  mRightValue;
  nsStyleUnion  mBottomValue;
};

// -------------------------
// nsStyleCoord inlines
//
inline PRBool nsStyleCoord::operator!=(const nsStyleCoord& aOther) const
{
  return PRBool(! ((*this) == aOther));
}

inline PRInt32 nsStyleCoord::GetCoordValue(void) const
{
  NS_ASSERTION((mUnit == eStyleUnit_Coord), "not a coord value");
  if (mUnit == eStyleUnit_Coord) {
    return mValue.mInt;
  }
  return 0;
}

inline PRInt32 nsStyleCoord::GetIntValue(void) const
{
  NS_ASSERTION((mUnit == eStyleUnit_Proportional) ||
               (mUnit == eStyleUnit_Enumerated) ||
               (mUnit == eStyleUnit_Chars) ||
               (mUnit == eStyleUnit_Integer), "not an int value");
  if ((mUnit == eStyleUnit_Proportional) ||
      (mUnit == eStyleUnit_Enumerated) ||
      (mUnit == eStyleUnit_Chars) ||
      (mUnit == eStyleUnit_Integer)) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsStyleCoord::GetPercentValue(void) const
{
  NS_ASSERTION(mUnit == eStyleUnit_Percent, "not a percent value");
  if (mUnit == eStyleUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetFactorValue(void) const
{
  NS_ASSERTION(mUnit == eStyleUnit_Factor, "not a factor value");
  if (mUnit == eStyleUnit_Factor) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline void nsStyleCoord::GetUnionValue(nsStyleUnion& aValue) const
{
  nsCRT::memcpy(&aValue, &mValue, sizeof(nsStyleUnion));
}

// -------------------------
// nsStyleSides inlines
//
inline PRBool nsStyleSides::operator!=(const nsStyleSides& aOther) const
{
  return PRBool(! ((*this) == aOther));
}

inline nsStyleUnit nsStyleSides::GetLeftUnit(void) const
{
  return (nsStyleUnit)mLeftUnit;
}

inline nsStyleUnit nsStyleSides::GetTopUnit(void) const
{
  return (nsStyleUnit)mTopUnit;
}

inline nsStyleUnit nsStyleSides::GetRightUnit(void) const
{
  return (nsStyleUnit)mRightUnit;
}

inline nsStyleUnit nsStyleSides::GetBottomUnit(void) const
{
  return (nsStyleUnit)mBottomUnit;
}

inline nsStyleCoord& nsStyleSides::GetLeft(nsStyleCoord& aCoord) const
{
  aCoord.SetUnionValue(mLeftValue, (nsStyleUnit)mLeftUnit);
  return aCoord;
}

inline nsStyleCoord& nsStyleSides::GetTop(nsStyleCoord& aCoord) const
{
  aCoord.SetUnionValue(mTopValue, (nsStyleUnit)mTopUnit);
  return aCoord;
}

inline nsStyleCoord& nsStyleSides::GetRight(nsStyleCoord& aCoord) const
{
  aCoord.SetUnionValue(mRightValue, (nsStyleUnit)mRightUnit);
  return aCoord;
}

inline nsStyleCoord& nsStyleSides::GetBottom(nsStyleCoord& aCoord) const
{
  aCoord.SetUnionValue(mBottomValue, (nsStyleUnit)mBottomUnit);
  return aCoord;
}

inline void nsStyleSides::SetLeft(const nsStyleCoord& aCoord)
{
  mLeftUnit = aCoord.GetUnit();
  aCoord.GetUnionValue(mLeftValue);
}

inline void nsStyleSides::SetTop(const nsStyleCoord& aCoord)
{
  mTopUnit = aCoord.GetUnit();
  aCoord.GetUnionValue(mTopValue);
}

inline void nsStyleSides::SetRight(const nsStyleCoord& aCoord)
{
  mRightUnit = aCoord.GetUnit();
  aCoord.GetUnionValue(mRightValue);
}

inline void nsStyleSides::SetBottom(const nsStyleCoord& aCoord)
{
  mBottomUnit = aCoord.GetUnit();
  aCoord.GetUnionValue(mBottomValue);
}

#endif /* nsStyleCoord_h___ */
