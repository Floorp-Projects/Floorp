/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of length values in computed style data */

#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nsCoord.h"
#include "nsStyleConsts.h"

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
  eStyleUnit_Turn         = 15,     // (float) angle in turns
  eStyleUnit_FlexFraction = 16,     // (float) <flex> in fr units
  eStyleUnit_Coord        = 20,     // (nscoord) value is twips
  eStyleUnit_Integer      = 30,     // (int) value is simple integer
  eStyleUnit_Enumerated   = 32,     // (int) value has enumerated meaning

  // The following are allocated types.  They are weak pointers to
  // values allocated by nsStyleContext::Alloc.
  eStyleUnit_Calc         = 40      // (Calc*) calc() toplevel; always present
                                    // to distinguish 50% from calc(50%), etc.
};

typedef union {
  int32_t     mInt;   // nscoord is a int32_t for now
  float       mFloat;
  // An mPointer is a weak pointer to a value that is guaranteed to
  // outlive the nsStyleCoord.  In the case of nsStyleCoord::Calc*, it
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
  struct Calc {
    // Every calc() expression evaluates to a length plus a percentage.
    nscoord mLength;
    float mPercent;
    bool mHasPercent; // whether there was any % syntax, even if 0

    bool operator==(const Calc& aOther) const {
      return mLength == aOther.mLength &&
             mPercent == aOther.mPercent &&
             mHasPercent == aOther.mHasPercent;
    }
    bool operator!=(const Calc& aOther) const { return !(*this == aOther); }
  };

  nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null);
  enum CoordConstructorType { CoordConstructor };
  inline nsStyleCoord(nscoord aValue, CoordConstructorType);
  nsStyleCoord(int32_t aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue, nsStyleUnit aUnit);
  inline nsStyleCoord(const nsStyleCoord& aCopy);
  inline nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit);

  nsStyleCoord&  operator=(const nsStyleCoord& aOther)
  {
    mUnit = aOther.mUnit;
    mValue = aOther.mValue;
    return *this;
  }
  bool           operator==(const nsStyleCoord& aOther) const;
  bool           operator!=(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit() const {
    NS_ASSERTION(mUnit != eStyleUnit_Null, "reading uninitialized value");
    return mUnit;
  }

  bool IsAngleValue() const {
    return eStyleUnit_Degree <= mUnit && mUnit <= eStyleUnit_Turn;
  }

  bool IsCalcUnit() const {
    return eStyleUnit_Calc == mUnit;
  }

  bool IsPointerValue() const {
    return IsCalcUnit();
  }

  bool IsCoordPercentCalcUnit() const {
    return mUnit == eStyleUnit_Coord ||
           mUnit == eStyleUnit_Percent ||
           IsCalcUnit();
  }

  // Does this calc() expression have any percentages inside it?  Can be
  // called only when IsCalcUnit() is true.
  bool CalcHasPercent() const {
    return GetCalcValue()->mHasPercent;
  }

  bool HasPercent() const {
    return mUnit == eStyleUnit_Percent ||
           (IsCalcUnit() && CalcHasPercent());
  }

  bool ConvertsToLength() const {
    return mUnit == eStyleUnit_Coord ||
           (IsCalcUnit() && !CalcHasPercent());
  }

  nscoord     GetCoordValue() const;
  int32_t     GetIntValue() const;
  float       GetPercentValue() const;
  float       GetFactorValue() const;
  float       GetAngleValue() const;
  double      GetAngleValueInRadians() const;
  float       GetFlexFractionValue() const;
  Calc*       GetCalcValue() const;
  void        GetUnionValue(nsStyleUnion& aValue) const;
  uint32_t    HashValue(uint32_t aHash) const;

  void  Reset();  // sets to null
  void  SetCoordValue(nscoord aValue);
  void  SetIntValue(int32_t aValue, nsStyleUnit aUnit);
  void  SetPercentValue(float aValue);
  void  SetFactorValue(float aValue);
  void  SetAngleValue(float aValue, nsStyleUnit aUnit);
  void  SetFlexFractionValue(float aValue);
  void  SetNormalValue();
  void  SetAutoValue();
  void  SetNoneValue();
  void  SetCalcValue(Calc* aValue);

private:
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
  nsStyleSides();

//  nsStyleSides&  operator=(const nsStyleSides& aCopy);  // use compiler's version
  bool           operator==(const nsStyleSides& aOther) const;
  bool           operator!=(const nsStyleSides& aOther) const;

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
  uint8_t       mUnits[4];
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
  bool           operator==(const nsStyleCorners& aOther) const;
  bool           operator!=(const nsStyleCorners& aOther) const;

  // aCorner is always one of NS_CORNER_* defined in nsStyleConsts.h
  inline nsStyleUnit GetUnit(uint8_t aHalfCorner) const;

  inline nsStyleCoord Get(uint8_t aHalfCorner) const;

  void  Reset();

  inline void Set(uint8_t aHalfCorner, const nsStyleCoord& aCoord);

protected:
  uint8_t       mUnits[8];
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
  else if (IsPointerValue()) {
    mValue.mPointer = aCopy.mValue.mPointer;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
}

inline nsStyleCoord::nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit)
  : mUnit(aUnit), mValue(aValue)
{
}

inline bool nsStyleCoord::operator!=(const nsStyleCoord& aOther) const
{
  return !((*this) == aOther);
}

inline nscoord nsStyleCoord::GetCoordValue() const
{
  NS_ASSERTION((mUnit == eStyleUnit_Coord), "not a coord value");
  if (mUnit == eStyleUnit_Coord) {
    return mValue.mInt;
  }
  return 0;
}

inline int32_t nsStyleCoord::GetIntValue() const
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
               mUnit <= eStyleUnit_Turn, "not an angle value");
  if (mUnit >= eStyleUnit_Degree && mUnit <= eStyleUnit_Turn) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetFlexFractionValue() const
{
  NS_ASSERTION(mUnit == eStyleUnit_FlexFraction, "not a fr value");
  if (mUnit == eStyleUnit_FlexFraction) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsStyleCoord::Calc* nsStyleCoord::GetCalcValue() const
{
  NS_ASSERTION(IsCalcUnit(), "not a pointer value");
  if (IsCalcUnit()) {
    return static_cast<Calc*>(mValue.mPointer);
  }
  return nullptr;
}


inline void nsStyleCoord::GetUnionValue(nsStyleUnion& aValue) const
{
  aValue = mValue;
}

// -------------------------
// nsStyleSides inlines
//
inline bool nsStyleSides::operator!=(const nsStyleSides& aOther) const
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
inline bool nsStyleCorners::operator!=(const nsStyleCorners& aOther) const
{
  return !((*this) == aOther);
}

inline nsStyleUnit nsStyleCorners::GetUnit(uint8_t aCorner) const
{
  return (nsStyleUnit)mUnits[aCorner];
}

inline nsStyleCoord nsStyleCorners::Get(uint8_t aCorner) const
{
  return nsStyleCoord(mValues[aCorner], nsStyleUnit(mUnits[aCorner]));
}

inline void nsStyleCorners::Set(uint8_t aCorner, const nsStyleCoord& aCoord)
{
  mUnits[aCorner] = aCoord.GetUnit();
  aCoord.GetUnionValue(mValues[aCorner]);
}

#endif /* nsStyleCoord_h___ */
