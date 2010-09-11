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

/* representation of length values in computed style data */

#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nscore.h"
#include "nsCoord.h"
#include "nsCRT.h"
#include "nsStyleConsts.h"
class nsString;
class nsStyleContext;

enum nsStyleUnit {
  eStyleUnit_Null         = 0,      // (no value) value is not specified
  eStyleUnit_Normal       = 1,      // (no value)
  eStyleUnit_Auto         = 2,      // (no value)
  eStyleUnit_None         = 3,      // (no value)
  eStyleUnit_Percent      = 10,     // (float) 1.0 == 100%
  eStyleUnit_Factor       = 11,     // (float) a multiplier
  eStyleUnit_Degree       = 12,     // (float) angle in degrees
  eStyleUnit_Grad         = 13,     // (float) angle in grads
  eStyleUnit_Radian       = 14,     // (float) angle in radians
  eStyleUnit_Coord        = 20,     // (nscoord) value is twips
  eStyleUnit_Integer      = 30,     // (int) value is simple integer
  eStyleUnit_Enumerated   = 32,     // (int) value has enumerated meaning
  // The following are all of the eCSSUnit_Calc_* types.  They are weak
  // pointers to a calc tree allocated by nsStyleContext::Alloc.
  // NOTE:  They are in the same order as the eCSSUnit_Calc_* values so
  // that converting between the two sets is just addition/subtraction.
  eStyleUnit_Calc         = 39,     // (Array*) calc() toplevel, to
                                    // distinguish 50% from calc(50%), etc.
  eStyleUnit_Calc_Plus    = 40,     // (Array*) + node within calc()
  eStyleUnit_Calc_Minus   = 41,     // (Array*) - within calc
  eStyleUnit_Calc_Times_L = 42,     // (Array*) num * val within calc
  eStyleUnit_Calc_Times_R = 43,     // (Array*) val * num within calc
  eStyleUnit_Calc_Divided = 44      // (Array*) / within calc
};

typedef union {
  PRInt32     mInt;   // nscoord is a PRInt32 for now
  float       mFloat;
  // An mPointer is a weak pointer to a value that is guaranteed to
  // outlive the nsStyleCoord.  In the case of nsStyleCoord::Array*, it
  // is a pointer owned by the style context, allocated through
  // nsStyleContext::Alloc (and, therefore, is never stored in the rule
  // tree).
  void*       mPointer;
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
  struct Array;
  friend struct Array;

  nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null);
  enum CoordConstructorType { CoordConstructor };
  inline nsStyleCoord(nscoord aValue, CoordConstructorType);
  nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue, nsStyleUnit aUnit);
  inline nsStyleCoord(const nsStyleCoord& aCopy);
  inline nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit);

  nsStyleCoord&  operator=(const nsStyleCoord& aCopy);
  PRBool         operator==(const nsStyleCoord& aOther) const;
  PRBool         operator!=(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit() const {
    NS_ASSERTION(mUnit != eStyleUnit_Null, "reading uninitialized value");
    return mUnit;
  }

  PRBool IsAngleValue() const {
    return eStyleUnit_Degree <= mUnit && mUnit <= eStyleUnit_Radian;
  }

  PRBool IsCalcUnit() const {
    return eStyleUnit_Calc <= mUnit && mUnit <= eStyleUnit_Calc_Divided;
  }

  PRBool IsCoordPercentCalcUnit() const {
    return mUnit == eStyleUnit_Coord ||
           mUnit == eStyleUnit_Percent ||
           IsCalcUnit();
  }

  // Does this calc() expression have any percentages inside it?  Can be
  // called only when IsCalcUnit() is true.
  PRBool CalcHasPercent() const;

  PRBool IsArrayValue() const {
    return IsCalcUnit();
  }

  PRBool HasPercent() const {
    return mUnit == eStyleUnit_Percent ||
           (IsCalcUnit() && CalcHasPercent());
  }

  PRBool ConvertsToLength() const {
    return mUnit == eStyleUnit_Coord ||
           (IsCalcUnit() && !CalcHasPercent());
  }

  nscoord     GetCoordValue() const;
  PRInt32     GetIntValue() const;
  float       GetPercentValue() const;
  float       GetFactorValue() const;
  float       GetAngleValue() const;
  double      GetAngleValueInRadians() const;
  Array*      GetArrayValue() const;
  void        GetUnionValue(nsStyleUnion& aValue) const;

  void  Reset();  // sets to null
  void  SetCoordValue(nscoord aValue);
  void  SetIntValue(PRInt32 aValue, nsStyleUnit aUnit);
  void  SetPercentValue(float aValue);
  void  SetFactorValue(float aValue);
  void  SetAngleValue(float aValue, nsStyleUnit aUnit);
  void  SetNormalValue();
  void  SetAutoValue();
  void  SetNoneValue();
  void  SetArrayValue(Array* aValue, nsStyleUnit aUnit);

public: // FIXME: private!
  nsStyleUnit   mUnit;
  nsStyleUnion  mValue;
};

// A fixed-size array, that, like everything else in nsStyleCoord,
// doesn't require that its destructors be called.
struct nsStyleCoord::Array {
  static Array* Create(nsStyleContext *aAllocationContext,
                       PRBool& aCanStoreInRuleTree,
                       size_t aCount);

  size_t Count() const { return mCount; }

  nsStyleCoord& operator[](size_t aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  const nsStyleCoord& operator[](size_t aIndex) const {
    NS_ABORT_IF_FALSE(aIndex < mCount, "out of range");
    return mArray[aIndex];
  }

  // Easier to use with an Array*:
  nsStyleCoord& Item(size_t aIndex) { return (*this)[aIndex]; }
  const nsStyleCoord& Item(size_t aIndex) const { return (*this)[aIndex]; }

  bool operator==(const Array& aOther) const;

  bool operator!=(const Array& aOther) const {
    return !(*this == aOther);
  }

private:
  inline void* operator new(size_t aSelfSize,
                            nsStyleContext *aAllocationContext,
                            size_t aItemCount) CPP_THROW_NEW;

  Array(size_t aCount)
    : mCount(aCount)
  {
    // Initialize all entries not in the class.
    for (size_t i = 1; i < aCount; ++i) {
      new (mArray + i) nsStyleCoord();
    }
  }

  size_t mCount;
  nsStyleCoord mArray[1]; // for alignment, have the first element in the class

  // not to be implemented
  Array(const Array& aOther);
  Array& operator=(const Array& aOther);
  ~Array();
};

/**
 * Class that represents a set of top/right/bottom/left nsStyleCoords.
 * This is commonly used to hold the widths of the borders, margins,
 * or paddings of a box.
 */
class nsStyleSides {
public:
  nsStyleSides();

//  nsStyleSides&  operator=(const nsStyleSides& aCopy);  // use compiler's version
  PRBool         operator==(const nsStyleSides& aOther) const;
  PRBool         operator!=(const nsStyleSides& aOther) const;

  inline nsStyleUnit GetUnit(mozilla::css::Side aSide) const;
  inline nsStyleUnit GetLeftUnit() const;
  inline nsStyleUnit GetTopUnit() const;
  inline nsStyleUnit GetRightUnit() const;
  inline nsStyleUnit GetBottomUnit() const;

  inline nsStyleCoord Get(mozilla::css::Side aSide) const;
  inline nsStyleCoord GetLeft() const;
  inline nsStyleCoord GetTop() const;
  inline nsStyleCoord GetRight() const;
  inline nsStyleCoord GetBottom() const;

  void  Reset();

  inline void Set(mozilla::css::Side aSide, const nsStyleCoord& aCoord);
  inline void SetLeft(const nsStyleCoord& aCoord);
  inline void SetTop(const nsStyleCoord& aCoord);
  inline void SetRight(const nsStyleCoord& aCoord);
  inline void SetBottom(const nsStyleCoord& aCoord);

protected:
  PRUint8       mUnits[4];
  nsStyleUnion  mValues[4];
};

/**
 * Class that represents a set of top-left/top-right/bottom-left/bottom-right
 * nsStyleCoord pairs.  This is used to hold the dimensions of the
 * corners of a box (for, e.g., border-radius and outline-radius).
 */
class nsStyleCorners {
public:
  nsStyleCorners();

  // use compiler's version
  //nsStyleCorners&  operator=(const nsStyleCorners& aCopy);
  PRBool         operator==(const nsStyleCorners& aOther) const;
  PRBool         operator!=(const nsStyleCorners& aOther) const;

  // aCorner is always one of NS_CORNER_* defined in nsStyleConsts.h
  inline nsStyleUnit GetUnit(PRUint8 aHalfCorner) const;

  inline nsStyleCoord Get(PRUint8 aHalfCorner) const;

  void  Reset();

  inline void Set(PRUint8 aHalfCorner, const nsStyleCoord& aCoord);

protected:
  PRUint8       mUnits[8];
  nsStyleUnion  mValues[8];
};


// -------------------------
// nsStyleCoord inlines
//
inline nsStyleCoord::nsStyleCoord(nscoord aValue, CoordConstructorType)
  : mUnit(eStyleUnit_Coord)
{
  mValue.mInt = aValue;
}

// FIXME: In C++0x we can rely on the default copy constructor since
// default copy construction is defined properly for unions.  But when
// can we actually use that?  (It seems to work in gcc 4.4.)
inline nsStyleCoord::nsStyleCoord(const nsStyleCoord& aCopy)
  : mUnit(aCopy.mUnit)
{
  if ((eStyleUnit_Percent <= mUnit) && (mUnit < eStyleUnit_Coord)) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else if (IsArrayValue()) {
    mValue.mPointer = aCopy.mValue.mPointer;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
}

inline nsStyleCoord::nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  memcpy(&mValue, &aValue, sizeof(nsStyleUnion));
}

inline PRBool nsStyleCoord::operator!=(const nsStyleCoord& aOther) const
{
  return !((*this) == aOther);
}

inline PRInt32 nsStyleCoord::GetCoordValue() const
{
  NS_ASSERTION((mUnit == eStyleUnit_Coord), "not a coord value");
  if (mUnit == eStyleUnit_Coord) {
    return mValue.mInt;
  }
  return 0;
}

inline PRInt32 nsStyleCoord::GetIntValue() const
{
  NS_ASSERTION((mUnit == eStyleUnit_Enumerated) ||
               (mUnit == eStyleUnit_Integer), "not an int value");
  if ((mUnit == eStyleUnit_Enumerated) ||
      (mUnit == eStyleUnit_Integer)) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsStyleCoord::GetPercentValue() const
{
  NS_ASSERTION(mUnit == eStyleUnit_Percent, "not a percent value");
  if (mUnit == eStyleUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetFactorValue() const
{
  NS_ASSERTION(mUnit == eStyleUnit_Factor, "not a factor value");
  if (mUnit == eStyleUnit_Factor) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetAngleValue() const
{
  NS_ASSERTION(mUnit >= eStyleUnit_Degree &&
               mUnit <= eStyleUnit_Radian, "not an angle value");
  if (mUnit >= eStyleUnit_Degree && mUnit <= eStyleUnit_Radian) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsStyleCoord::Array* nsStyleCoord::GetArrayValue() const
{
  NS_ASSERTION(IsArrayValue(), "not a pointer value");
  if (IsArrayValue()) {
    return static_cast<Array*>(mValue.mPointer);
  }
  return nsnull;
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
  return !((*this) == aOther);
}

inline nsStyleUnit nsStyleSides::GetUnit(mozilla::css::Side aSide) const
{
  return (nsStyleUnit)mUnits[aSide];
}

inline nsStyleUnit nsStyleSides::GetLeftUnit() const
{
  return GetUnit(NS_SIDE_LEFT);
}

inline nsStyleUnit nsStyleSides::GetTopUnit() const
{
  return GetUnit(NS_SIDE_TOP);
}

inline nsStyleUnit nsStyleSides::GetRightUnit() const
{
  return GetUnit(NS_SIDE_RIGHT);
}

inline nsStyleUnit nsStyleSides::GetBottomUnit() const
{
  return GetUnit(NS_SIDE_BOTTOM);
}

inline nsStyleCoord nsStyleSides::Get(mozilla::css::Side aSide) const
{
  return nsStyleCoord(mValues[aSide], nsStyleUnit(mUnits[aSide]));
}

inline nsStyleCoord nsStyleSides::GetLeft() const
{
  return Get(NS_SIDE_LEFT);
}

inline nsStyleCoord nsStyleSides::GetTop() const
{
  return Get(NS_SIDE_TOP);
}

inline nsStyleCoord nsStyleSides::GetRight() const
{
  return Get(NS_SIDE_RIGHT);
}

inline nsStyleCoord nsStyleSides::GetBottom() const
{
  return Get(NS_SIDE_BOTTOM);
}

inline void nsStyleSides::Set(mozilla::css::Side aSide, const nsStyleCoord& aCoord)
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

// -------------------------
// nsStyleCorners inlines
//
inline PRBool nsStyleCorners::operator!=(const nsStyleCorners& aOther) const
{
  return !((*this) == aOther);
}

inline nsStyleUnit nsStyleCorners::GetUnit(PRUint8 aCorner) const
{
  return (nsStyleUnit)mUnits[aCorner];
}

inline nsStyleCoord nsStyleCorners::Get(PRUint8 aCorner) const
{
  return nsStyleCoord(mValues[aCorner], nsStyleUnit(mUnits[aCorner]));
}

inline void nsStyleCorners::Set(PRUint8 aCorner, const nsStyleCoord& aCoord)
{
  mUnits[aCorner] = aCoord.GetUnit();
  aCoord.GetUnionValue(mValues[aCorner]);
}

#endif /* nsStyleCoord_h___ */
