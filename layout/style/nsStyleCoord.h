/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nscore.h"
#include "nsCoord.h"
#include "nsCRT.h"
class nsString;

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
  nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null);
  nsStyleCoord(nscoord aValue);
  nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue, nsStyleUnit aUnit);
  nsStyleCoord(const nsStyleCoord& aCopy);

  nsStyleCoord&  operator=(const nsStyleCoord& aCopy);
  PRBool         operator==(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit(void) const { return mUnit; }
  nscoord     GetCoordValue(void) const;
  PRInt32     GetIntValue(void) const;
  float       GetPercentValue(void) const;
  float       GetFactorValue(void) const;
  void        GetUnionValue(nsStyleUnion& aValue) const;

  void  Reset(void);  // sets to null
  void  SetCoordValue(nscoord aValue);
  void  SetIntValue(PRInt32 aValue, nsStyleUnit aUnit);
  void  SetPercentValue(float aValue);
  void  SetFactorValue(float aValue);
  void  SetNormalValue(void);
  void  SetAutoValue(void);
  void  SetInheritValue(void);
  void  SetUnionValue(const nsStyleUnion& aValue, nsStyleUnit aUnit);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

protected:
  nsStyleUnit   mUnit;
  nsStyleUnion  mValue;
};


class nsStyleSides {
public:
  nsStyleSides(void);

//  nsStyleSides&  operator=(const nsStyleSides& aCopy);  // use compiler's version
  PRBool         operator==(const nsStyleSides& aOther) const;

  nsStyleUnit GetLeftUnit(void) const;
  nsStyleUnit GetTopUnit(void) const;
  nsStyleUnit GetRightUnit(void) const;
  nsStyleUnit GetBottomUnit(void) const;

  nsStyleCoord& GetLeft(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetTop(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetRight(nsStyleCoord& aCoord) const;
  nsStyleCoord& GetBottom(nsStyleCoord& aCoord) const;

  void  Reset(void);
  void  SetLeft(const nsStyleCoord& aCoord);
  void  SetTop(const nsStyleCoord& aCoord);
  void  SetRight(const nsStyleCoord& aCoord);
  void  SetBottom(const nsStyleCoord& aCoord);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

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
