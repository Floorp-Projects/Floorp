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
#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nscore.h"
#include "nsCoord.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"
class nsString;

enum nsStyleUnit {
  eStyleUnit_Null         = 0,      // (no value) value is not specified
  eStyleUnit_Normal       = 1,      // (no value)
  eStyleUnit_Auto         = 2,      // (no value)
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

/**
 * Class that hold a single size specification used by the style
 * system.  The size specification consists of two parts -- a number
 * and a unit.  The number is an integer, a floating point value, an
 * nscoord, or undefined, and the unit is an nsStyleUnit.  Checking
 * the unit is a must before asking for the value in any particular
 * form.
 */
class nsStyleCoord {
public:
  nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null);
  nsStyleCoord(nscoord aValue);
  nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue, nsStyleUnit aUnit);
  nsStyleCoord(const nsStyleCoord& aCopy);

  nsStyleCoord&  operator=(const nsStyleCoord& aCopy);
  PRBool         operator==(const nsStyleCoord& aOther) const;
  PRBool         operator!=(const nsStyleCoord& aOther) const;

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
  void  SetUnionValue(const nsStyleUnion& aValue, nsStyleUnit aUnit);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

public:
  nsStyleUnit   mUnit;
  nsStyleUnion  mValue;
};


/**
 * Class that represents a set of top/right/bottom/left nsStyleCoords.
 * This is commonly used to hold the widths of the borders, margins,
 * or paddings of a box.
 */
class nsStyleSides {
public:
  nsStyleSides(void);

//  nsStyleSides&  operator=(const nsStyleSides& aCopy);  // use compiler's version
  PRBool         operator==(const nsStyleSides& aOther) const;
  PRBool         operator!=(const nsStyleSides& aOther) const;

  // aSide is always one of NS_SIDE_* defined in nsStyleConsts.h

  inline nsStyleUnit GetUnit(PRUint8 aSide) const;
  inline nsStyleUnit GetLeftUnit(void) const;
  inline nsStyleUnit GetTopUnit(void) const;
  inline nsStyleUnit GetRightUnit(void) const;
  inline nsStyleUnit GetBottomUnit(void) const;

  inline nsStyleCoord& Get(PRUint8 aSide, nsStyleCoord& aCoord) const;
  inline nsStyleCoord& GetLeft(nsStyleCoord& aCoord) const;
  inline nsStyleCoord& GetTop(nsStyleCoord& aCoord) const;
  inline nsStyleCoord& GetRight(nsStyleCoord& aCoord) const;
  inline nsStyleCoord& GetBottom(nsStyleCoord& aCoord) const;

  void  Reset(void);

  inline void Set(PRUint8 aSide, const nsStyleCoord& aCoord);
  inline void SetLeft(const nsStyleCoord& aCoord);
  inline void SetTop(const nsStyleCoord& aCoord);
  inline void SetRight(const nsStyleCoord& aCoord);
  inline void SetBottom(const nsStyleCoord& aCoord);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

protected:
  PRUint8       mUnits[4];
  nsStyleUnion  mValues[4];
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
  memcpy(&aValue, &mValue, sizeof(nsStyleUnion));
}

// -------------------------
// nsStyleSides inlines
//
inline PRBool nsStyleSides::operator!=(const nsStyleSides& aOther) const
{
  return PRBool(! ((*this) == aOther));
}

inline nsStyleUnit nsStyleSides::GetUnit(PRUint8 aSide) const
{
  return (nsStyleUnit)mUnits[aSide];
}

inline nsStyleUnit nsStyleSides::GetLeftUnit(void) const
{
  return GetUnit(NS_SIDE_LEFT);
}

inline nsStyleUnit nsStyleSides::GetTopUnit(void) const
{
  return GetUnit(NS_SIDE_TOP);
}

inline nsStyleUnit nsStyleSides::GetRightUnit(void) const
{
  return GetUnit(NS_SIDE_RIGHT);
}

inline nsStyleUnit nsStyleSides::GetBottomUnit(void) const
{
  return GetUnit(NS_SIDE_BOTTOM);
}

inline nsStyleCoord& nsStyleSides::Get(PRUint8 aSide, nsStyleCoord& aCoord) const
{
  aCoord.SetUnionValue(mValues[aSide], (nsStyleUnit)mUnits[aSide]);
  return aCoord;
}

inline nsStyleCoord& nsStyleSides::GetLeft(nsStyleCoord& aCoord) const
{
  return Get(NS_SIDE_LEFT, aCoord);
}

inline nsStyleCoord& nsStyleSides::GetTop(nsStyleCoord& aCoord) const
{
  return Get(NS_SIDE_TOP, aCoord);
}

inline nsStyleCoord& nsStyleSides::GetRight(nsStyleCoord& aCoord) const
{
  return Get(NS_SIDE_RIGHT, aCoord);
}

inline nsStyleCoord& nsStyleSides::GetBottom(nsStyleCoord& aCoord) const
{
  return Get(NS_SIDE_BOTTOM, aCoord);
}

inline void nsStyleSides::Set(PRUint8 aSide, const nsStyleCoord& aCoord)
{
  mUnits[aSide] = aCoord.GetUnit();
  aCoord.GetUnionValue(mValues[aSide]);
}

inline void nsStyleSides::SetLeft(const nsStyleCoord& aCoord)
{
  Set(NS_SIDE_LEFT, aCoord);
}

inline void nsStyleSides::SetTop(const nsStyleCoord& aCoord)
{
  Set(NS_SIDE_TOP, aCoord);
}

inline void nsStyleSides::SetRight(const nsStyleCoord& aCoord)
{
  Set(NS_SIDE_RIGHT, aCoord);
}

inline void nsStyleSides::SetBottom(const nsStyleCoord& aCoord)
{
  Set(NS_SIDE_BOTTOM, aCoord);
}

#endif /* nsStyleCoord_h___ */
