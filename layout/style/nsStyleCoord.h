/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of length values in computed style data */

#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include <type_traits>

#include "mozilla/EnumTypeTraits.h"
#include "mozilla/gfx/Types.h"
#include "Units.h"
#include "nsCoord.h"
#include "nsISupportsImpl.h"
#include "nsStyleConsts.h"

namespace mozilla {

class WritingMode;

// Logical axis, edge, side and corner constants for use in various places.
enum LogicalAxis { eLogicalAxisBlock = 0x0, eLogicalAxisInline = 0x1 };
enum LogicalEdge { eLogicalEdgeStart = 0x0, eLogicalEdgeEnd = 0x1 };
enum LogicalSide : uint8_t {
  eLogicalSideBStart = (eLogicalAxisBlock << 1) | eLogicalEdgeStart,   // 0x0
  eLogicalSideBEnd = (eLogicalAxisBlock << 1) | eLogicalEdgeEnd,       // 0x1
  eLogicalSideIStart = (eLogicalAxisInline << 1) | eLogicalEdgeStart,  // 0x2
  eLogicalSideIEnd = (eLogicalAxisInline << 1) | eLogicalEdgeEnd       // 0x3
};

enum LogicalCorner {
  eLogicalCornerBStartIStart = 0,
  eLogicalCornerBStartIEnd = 1,
  eLogicalCornerBEndIEnd = 2,
  eLogicalCornerBEndIStart = 3
};

using LengthPercentage = StyleLengthPercentage;
using LengthPercentageOrAuto = StyleLengthPercentageOrAuto;
using NonNegativeLengthPercentage = StyleNonNegativeLengthPercentage;
using NonNegativeLengthPercentageOrAuto =
    StyleNonNegativeLengthPercentageOrAuto;
using NonNegativeLengthPercentageOrNormal =
    StyleNonNegativeLengthPercentageOrNormal;
using Length = StyleLength;
using LengthOrAuto = StyleLengthOrAuto;
using NonNegativeLength = StyleNonNegativeLength;
using NonNegativeLengthOrAuto = StyleNonNegativeLengthOrAuto;
using BorderRadius = StyleBorderRadius;

bool StyleCSSPixelLength::IsZero() const { return _0 == 0.0f; }

nscoord StyleCSSPixelLength::ToAppUnits() const {
  // We want to resolve the length part of the calc() expression rounding 0.5
  // away from zero, instead of the default behavior of
  // NSToCoordRound{,WithClamp} which do floor(x + 0.5).
  //
  // This is what the rust code in the app_units crate does, and not doing this
  // would regress bug 1323735, for example.
  //
  // FIXME(emilio, bug 1528114): Probably we should do something smarter.
  float length = _0 * float(mozilla::AppUnitsPerCSSPixel());
  if (length >= nscoord_MAX) {
    return nscoord_MAX;
  }
  if (length <= nscoord_MIN) {
    return nscoord_MIN;
  }
  return NSToIntRound(length);
}

constexpr LengthPercentage LengthPercentage::Zero() {
  return {{0.}, {0.}, StyleAllowedNumericType::All, false, false};
}

LengthPercentage LengthPercentage::FromPixels(CSSCoord aCoord) {
  return {{aCoord}, {0.}, StyleAllowedNumericType::All, false, false};
}

LengthPercentage LengthPercentage::FromAppUnits(nscoord aCoord) {
  return LengthPercentage::FromPixels(CSSPixel::FromAppUnits(aCoord));
}

LengthPercentage LengthPercentage::FromPercentage(float aPercentage) {
  return {{0.}, {aPercentage}, StyleAllowedNumericType::All, true, false};
}

CSSCoord LengthPercentage::LengthInCSSPixels() const { return length._0; }

float LengthPercentage::Percentage() const { return percentage._0; }

bool LengthPercentage::HasPercent() const { return has_percentage; }

bool LengthPercentage::ConvertsToLength() const { return !HasPercent(); }

nscoord LengthPercentage::ToLength() const {
  MOZ_ASSERT(ConvertsToLength());
  return length.ToAppUnits();
}

bool LengthPercentage::ConvertsToPercentage() const {
  return has_percentage && length.IsZero();
}

float LengthPercentage::ToPercentage() const {
  MOZ_ASSERT(ConvertsToPercentage());
  return Percentage();
}

bool LengthPercentage::HasLengthAndPercentage() const {
  return !ConvertsToLength() && !ConvertsToPercentage();
}

bool LengthPercentage::IsDefinitelyZero() const {
  return length.IsZero() && Percentage() == 0.0f;
}

CSSCoord LengthPercentage::ResolveToCSSPixels(CSSCoord aPercentageBasis) const {
  return LengthInCSSPixels() + Percentage() * aPercentageBasis;
}

template <typename T>
CSSCoord LengthPercentage::ResolveToCSSPixelsWith(T aPercentageGetter) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), CSSCoord>::value,
                "Should return CSS pixels");
  if (ConvertsToLength()) {
    return LengthInCSSPixels();
  }
  return ResolveToCSSPixels(aPercentageGetter());
}

template <typename T, typename U>
nscoord LengthPercentage::Resolve(T aPercentageGetter,
                                  U aPercentageRounder) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), nscoord>::value,
                "Should return app units");
  static_assert(
      std::is_same<decltype(aPercentageRounder(1.0f)), nscoord>::value,
      "Should return app units");
  if (ConvertsToLength()) {
    return ToLength();
  }
  nscoord basis = aPercentageGetter();
  return length.ToAppUnits() + aPercentageRounder(basis * Percentage());
}

nscoord LengthPercentage::Resolve(nscoord aPercentageBasis) const {
  return Resolve([=] { return aPercentageBasis; }, NSToCoordFloorClamped);
}

template <typename T>
nscoord LengthPercentage::Resolve(T aPercentageGetter) const {
  static_assert(std::is_same<decltype(aPercentageGetter()), nscoord>::value,
                "Should return app units");
  return Resolve(aPercentageGetter, NSToCoordFloorClamped);
}

template <typename T>
nscoord LengthPercentage::Resolve(nscoord aPercentageBasis,
                                  T aPercentageRounder) const {
  return Resolve([=] { return aPercentageBasis; }, aPercentageRounder);
}

#define IMPL_LENGTHPERCENTAGE_FORWARDS(ty_)                                 \
  template <>                                                               \
  inline bool ty_::HasPercent() const {                                     \
    return IsLengthPercentage() && AsLengthPercentage().HasPercent();       \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::ConvertsToLength() const {                               \
    return IsLengthPercentage() && AsLengthPercentage().ConvertsToLength(); \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::HasLengthAndPercentage() const {                         \
    return IsLengthPercentage() &&                                          \
           AsLengthPercentage().HasLengthAndPercentage();                   \
  }                                                                         \
  template <>                                                               \
  inline nscoord ty_::ToLength() const {                                    \
    MOZ_ASSERT(ConvertsToLength());                                         \
    return AsLengthPercentage().ToLength();                                 \
  }                                                                         \
  template <>                                                               \
  inline bool ty_::ConvertsToPercentage() const {                           \
    return IsLengthPercentage() &&                                          \
           AsLengthPercentage().ConvertsToPercentage();                     \
  }                                                                         \
  template <>                                                               \
  inline float ty_::ToPercentage() const {                                  \
    MOZ_ASSERT(ConvertsToPercentage());                                     \
    return AsLengthPercentage().ToPercentage();                             \
  }

IMPL_LENGTHPERCENTAGE_FORWARDS(LengthPercentageOrAuto)
IMPL_LENGTHPERCENTAGE_FORWARDS(StyleSize)
IMPL_LENGTHPERCENTAGE_FORWARDS(StyleMaxSize)

template <>
inline bool LengthOrAuto::IsLength() const {
  return IsLengthPercentage();
}

template <>
inline const Length& LengthOrAuto::AsLength() const {
  return AsLengthPercentage();
}

template <>
inline bool StyleFlexBasis::IsAuto() const {
  return IsSize() && AsSize().IsAuto();
}

template <>
inline bool StyleSize::BehavesLikeInitialValueOnBlockAxis() const {
  return IsAuto() || IsExtremumLength();
}

template <>
inline bool StyleMaxSize::BehavesLikeInitialValueOnBlockAxis() const {
  return IsNone() || IsExtremumLength();
}

template <>
inline bool StyleBackgroundSize::IsInitialValue() const {
  return IsExplicitSize() && explicit_size.width.IsAuto() &&
         explicit_size.height.IsAuto();
}

template <typename T>
const T& StyleRect<T>::Get(mozilla::Side aSide) const {
  static_assert(sizeof(StyleRect<T>) == sizeof(T) * 4, "");
  static_assert(alignof(StyleRect<T>) == alignof(T), "");
  return reinterpret_cast<const T*>(this)[aSide];
}

template <typename T>
template <typename Predicate>
bool StyleRect<T>::All(Predicate aPredicate) const {
  return aPredicate(_0) && aPredicate(_1) && aPredicate(_2) && aPredicate(_3);
}

template <typename T>
template <typename Predicate>
bool StyleRect<T>::Any(Predicate aPredicate) const {
  return aPredicate(_0) || aPredicate(_1) || aPredicate(_2) || aPredicate(_3);
}

template <>
inline const LengthPercentage& BorderRadius::Get(HalfCorner aCorner) const {
  static_assert(sizeof(BorderRadius) == sizeof(LengthPercentage) * 8, "");
  static_assert(alignof(BorderRadius) == alignof(LengthPercentage), "");
  auto* self = reinterpret_cast<const LengthPercentage*>(this);
  return self[aCorner];
}

}  // namespace mozilla

enum nsStyleUnit : uint8_t {
  eStyleUnit_Null = 0,           // (no value) value is not specified
  eStyleUnit_Normal = 1,         // (no value)
  eStyleUnit_Auto = 2,           // (no value)
  eStyleUnit_None = 3,           // (no value)
  eStyleUnit_Percent = 10,       // (float) 1.0 == 100%
  eStyleUnit_Factor = 11,        // (float) a multiplier
  eStyleUnit_Degree = 12,        // (float) angle in degrees
  eStyleUnit_FlexFraction = 16,  // (float) <flex> in fr units
  eStyleUnit_Coord = 20,         // (nscoord) value is twips
  eStyleUnit_Integer = 30,       // (int) value is simple integer
  eStyleUnit_Enumerated = 32,    // (int) value has enumerated meaning

  // The following are reference counted allocated types.
  eStyleUnit_Calc = 40,  // (Calc*) calc() toplevel; always present
                         // to distinguish 50% from calc(50%), etc.

  eStyleUnit_MAX = 40  // highest valid nsStyleUnit value
};

typedef union {
  int32_t mInt;  // nscoord is a int32_t for now
  float mFloat;
  // An mPointer is a reference counted pointer.  Currently this can only
  // ever be an nsStyleCoord::Calc*.
  void* mPointer;
} nsStyleUnion;

/**
 * Class that hold a single size specification used by the style
 * system.  The size specification consists of two parts -- a number
 * and a unit.  The number is an integer, a floating point value, an
 * nscoord, or undefined, and the unit is an nsStyleUnit.  Checking
 * the unit is a must before asking for the value in any particular
 * form.
 */
/** <div rustbindgen private accessor="unsafe"></div> */
class nsStyleCoord {
 public:
  // Non-reference counted calc() value.  See nsStyleStruct.h for some uses
  // of this.
  struct CalcValue {
    // Every calc() expression evaluates to a length plus a percentage.
    nscoord mLength;
    float mPercent;
    bool mHasPercent;  // whether there was any % syntax, even if 0

    bool operator==(const CalcValue& aOther) const {
      return mLength == aOther.mLength && mPercent == aOther.mPercent &&
             mHasPercent == aOther.mHasPercent;
    }
    bool operator!=(const CalcValue& aOther) const {
      return !(*this == aOther);
    }

    nscoord ToLength() const {
      MOZ_ASSERT(!mHasPercent);
      return mLength;
    }

    // If this returns true the value is definitely zero. It it returns false
    // it might be zero. So it's best used for conservative optimization.
    bool IsDefinitelyZero() const { return mLength == 0 && mPercent == 0; }
  };

  // Reference counted calc() value.  This is the type that is used to store
  // the calc() value in nsStyleCoord.
  struct Calc final : public CalcValue {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Calc)
    Calc() {}

   private:
    Calc(const Calc&) = delete;
    ~Calc() {}
    Calc& operator=(const Calc&) = delete;
  };

  explicit nsStyleCoord(nsStyleUnit aUnit = eStyleUnit_Null);
  enum CoordConstructorType { CoordConstructor };
  inline nsStyleCoord(nscoord aValue, CoordConstructorType);
  nsStyleCoord(int32_t aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue, nsStyleUnit aUnit);
  inline nsStyleCoord(const nsStyleCoord& aCopy);
  inline nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit);
  ~nsStyleCoord() { Reset(); }

  nsStyleCoord& operator=(const nsStyleCoord& aOther) {
    if (this != &aOther) {
      SetValue(mUnit, mValue, aOther);
    }
    return *this;
  }
  bool operator==(const nsStyleCoord& aOther) const;
  bool operator!=(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit() const {
    NS_ASSERTION(mUnit != eStyleUnit_Null, "reading uninitialized value");
    return mUnit;
  }

  // This is especially useful to check if it is the property's initial value
  // or keyword for sizing properties.
  bool IsAutoOrEnum() const {
    // The initial value of width/height and min-width/min-height is `auto`.
    // The initial value of max-width/max-height is `none`.
    return mUnit == eStyleUnit_Auto || mUnit == eStyleUnit_None ||
           mUnit == eStyleUnit_Enumerated;
  }

  bool IsAngleValue() const { return eStyleUnit_Degree == mUnit; }

  static bool IsCalcUnit(nsStyleUnit aUnit) { return aUnit == eStyleUnit_Calc; }

  static bool IsPointerUnit(nsStyleUnit aUnit) { return IsCalcUnit(aUnit); }

  bool IsCalcUnit() const { return IsCalcUnit(mUnit); }

  bool IsPointerValue() const { return IsPointerUnit(mUnit); }

  bool IsCoordPercentCalcUnit() const {
    return mUnit == eStyleUnit_Coord || mUnit == eStyleUnit_Percent ||
           IsCalcUnit();
  }

  // Does this calc() expression have any percentages inside it?  Can be
  // called only when IsCalcUnit() is true.
  bool CalcHasPercent() const { return GetCalcValue()->mHasPercent; }

  bool HasPercent() const {
    return mUnit == eStyleUnit_Percent || (IsCalcUnit() && CalcHasPercent());
  }

  static bool ConvertsToPercent(const nsStyleUnit aUnit,
                                const nsStyleUnion aValue) {
    if (aUnit == eStyleUnit_Percent) {
      return true;
    }
    if (!IsCalcUnit(aUnit)) {
      return false;
    }
    auto* calc = AsCalcValue(aValue);
    return calc->mLength == 0 && calc->mHasPercent;
  }

  bool ConvertsToPercent() const { return ConvertsToPercent(mUnit, mValue); }

  float ToPercent() const {
    MOZ_ASSERT(ConvertsToPercent());
    if (IsCalcUnit()) {
      MOZ_ASSERT(CalcHasPercent() && GetCalcValue()->mLength == 0);
      return GetCalcValue()->mPercent;
    }
    return mValue.mFloat;
  }

  static bool ConvertsToLength(const nsStyleUnit aUnit,
                               const nsStyleUnion aValue) {
    return aUnit == eStyleUnit_Coord ||
           (IsCalcUnit(aUnit) && !AsCalcValue(aValue)->mHasPercent);
  }

  bool ConvertsToLength() const { return ConvertsToLength(mUnit, mValue); }

  static nscoord ToLength(nsStyleUnit aUnit, nsStyleUnion aValue) {
    MOZ_ASSERT(ConvertsToLength(aUnit, aValue));
    if (IsCalcUnit(aUnit)) {
      // Note: ToLength asserts !mHasPercent
      return AsCalcValue(aValue)->ToLength();
    }
    MOZ_ASSERT(aUnit == eStyleUnit_Coord);
    return aValue.mInt;
  }

  nscoord ToLength() const { return ToLength(GetUnit(), mValue); }

  // Callers must verify IsCalcUnit before calling this function.
  static Calc* AsCalcValue(nsStyleUnion aValue) {
    return static_cast<Calc*>(aValue.mPointer);
  }

  // Compute the value that IsCalcUnit().
  // @note the caller is expected to handle percentage of an indefinite size
  // and NOT call this method with aPercentageBasis == NS_UNCONSTRAINEDSIZE.
  // @note the return value may be negative, e.g. for "calc(a - b%)"
  nscoord ComputeComputedCalc(nscoord aPercentageBasis) const;

  // Compute the value that is either a coord, a percent, or a calc expression.
  // @note the caller is expected to handle percentage of an indefinite size
  // and NOT call this method with aPercentageBasis == NS_UNCONSTRAINEDSIZE.
  // @note the return value may be negative, e.g. for "calc(a - b%)"
  nscoord ComputeCoordPercentCalc(nscoord aPercentageBasis) const;

  nscoord GetCoordValue() const;
  int32_t GetIntValue() const;
  float GetPercentValue() const;
  float GetFactorValue() const;
  float GetFactorOrPercentValue() const;
  float GetAngleValue() const;
  double GetAngleValueInDegrees() const;
  double GetAngleValueInRadians() const;
  float GetFlexFractionValue() const;
  Calc* GetCalcValue() const;
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  T GetEnumValue() const {
    MOZ_ASSERT(GetUnit() == eStyleUnit_Enumerated,
               "The unit must be eStyleUnit_Enumerated!");
    return static_cast<T>(GetIntValue());
  }

  // Sets to null and releases any refcounted objects.  Only use this if the
  // object is initialized (i.e. don't use it in nsStyleCoord constructors).
  void Reset();

  void SetCoordValue(nscoord aValue);
  void SetIntValue(int32_t aValue, nsStyleUnit aUnit);
  void SetPercentValue(float aValue);
  void SetFactorValue(float aValue);
  void SetFlexFractionValue(float aValue);
  void SetNormalValue();
  void SetAutoValue();
  void SetNoneValue();
  void SetCalcValue(Calc* aValue);
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetEnumValue(T aValue) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt!");
    SetIntValue(static_cast<int32_t>(aValue), eStyleUnit_Enumerated);
  }

  // Resets a coord represented by a unit/value pair.
  static inline void Reset(nsStyleUnit& aUnit, nsStyleUnion& aValue);

  // Sets a coord represented by a unit/value pair from a second
  // unit/value pair.
  static inline void SetValue(nsStyleUnit& aUnit, nsStyleUnion& aValue,
                              nsStyleUnit aOtherUnit,
                              const nsStyleUnion& aOtherValue);

  // Sets a coord represented by a unit/value pair from an nsStyleCoord.
  static inline void SetValue(nsStyleUnit& aUnit, nsStyleUnion& aValue,
                              const nsStyleCoord& aOther);

  // Like the above, but do not reset before setting.
  static inline void InitWithValue(nsStyleUnit& aUnit, nsStyleUnion& aValue,
                                   nsStyleUnit aOtherUnit,
                                   const nsStyleUnion& aOtherValue);

  static inline void InitWithValue(nsStyleUnit& aUnit, nsStyleUnion& aValue,
                                   const nsStyleCoord& aOther);

 private:
  nsStyleUnit mUnit;
  nsStyleUnion mValue;
};

/**
 * Class that represents a set of top/right/bottom/left nsStyleCoords.
 * This is commonly used to hold the widths of the borders, margins,
 * or paddings of a box.
 */
/** <div rustbindgen private accessor="unsafe"></div> */
class nsStyleSides {
 public:
  nsStyleSides();
  nsStyleSides(const nsStyleSides&);
  ~nsStyleSides();

  nsStyleSides& operator=(const nsStyleSides& aCopy);
  bool operator==(const nsStyleSides& aOther) const;
  bool operator!=(const nsStyleSides& aOther) const;

  inline nsStyleUnit GetUnit(mozilla::Side aSide) const;
  inline nsStyleUnit GetLeftUnit() const;
  inline nsStyleUnit GetTopUnit() const;
  inline nsStyleUnit GetRightUnit() const;
  inline nsStyleUnit GetBottomUnit() const;

  inline nsStyleCoord Get(mozilla::Side aSide) const;
  inline nsStyleCoord GetLeft() const;
  inline nsStyleCoord GetTop() const;
  inline nsStyleCoord GetRight() const;
  inline nsStyleCoord GetBottom() const;

  // Methods to access the units and values in terms of logical sides
  // for a given writing mode.
  // NOTE: The definitions are in WritingModes.h (after we have the full
  // declaration of WritingMode available).
  inline nsStyleUnit GetUnit(mozilla::WritingMode aWritingMode,
                             mozilla::LogicalSide aSide) const;
  inline nsStyleUnit GetIStartUnit(mozilla::WritingMode aWritingMode) const;
  inline nsStyleUnit GetBStartUnit(mozilla::WritingMode aWritingMode) const;
  inline nsStyleUnit GetIEndUnit(mozilla::WritingMode aWritingMode) const;
  inline nsStyleUnit GetBEndUnit(mozilla::WritingMode aWritingMode) const;

  // Return true if either the start or end side in the axis is 'auto'.
  inline bool HasBlockAxisAuto(mozilla::WritingMode aWritingMode) const;
  inline bool HasInlineAxisAuto(mozilla::WritingMode aWritingMode) const;

  inline nsStyleCoord Get(mozilla::WritingMode aWritingMode,
                          mozilla::LogicalSide aSide) const;
  inline nsStyleCoord GetIStart(mozilla::WritingMode aWritingMode) const;
  inline nsStyleCoord GetBStart(mozilla::WritingMode aWritingMode) const;
  inline nsStyleCoord GetIEnd(mozilla::WritingMode aWritingMode) const;
  inline nsStyleCoord GetBEnd(mozilla::WritingMode aWritingMode) const;

  // Sets each side to null and releases any refcounted objects.  Only use this
  // if the object is initialized (i.e. don't use it in nsStyleSides
  // constructors).
  void Reset();

  inline void Set(mozilla::Side aSide, const nsStyleCoord& aCoord);
  inline void SetLeft(const nsStyleCoord& aCoord);
  inline void SetTop(const nsStyleCoord& aCoord);
  inline void SetRight(const nsStyleCoord& aCoord);
  inline void SetBottom(const nsStyleCoord& aCoord);

  nscoord ToLength(mozilla::Side aSide) const {
    return nsStyleCoord::ToLength(mUnits[aSide], mValues[aSide]);
  }

  bool ConvertsToLength() const {
    NS_FOR_CSS_SIDES(side) {
      if (!nsStyleCoord::ConvertsToLength(mUnits[side], mValues[side])) {
        return false;
      }
    }
    return true;
  }

 protected:
  nsStyleUnit mUnits[4];
  nsStyleUnion mValues[4];
};

// -------------------------
// nsStyleCoord inlines
//
inline nsStyleCoord::nsStyleCoord(nscoord aValue, CoordConstructorType)
    : mUnit(eStyleUnit_Coord) {
  mValue.mInt = aValue;
}

inline nsStyleCoord::nsStyleCoord(const nsStyleCoord& aCopy)
    : mUnit(eStyleUnit_Null) {
  InitWithValue(mUnit, mValue, aCopy);
}

inline nsStyleCoord::nsStyleCoord(const nsStyleUnion& aValue, nsStyleUnit aUnit)
    : mUnit(eStyleUnit_Null) {
  InitWithValue(mUnit, mValue, aUnit, aValue);
}

inline bool nsStyleCoord::operator!=(const nsStyleCoord& aOther) const {
  return !((*this) == aOther);
}

inline nscoord nsStyleCoord::GetCoordValue() const {
  NS_ASSERTION((mUnit == eStyleUnit_Coord), "not a coord value");
  if (mUnit == eStyleUnit_Coord) {
    return mValue.mInt;
  }
  return 0;
}

inline int32_t nsStyleCoord::GetIntValue() const {
  NS_ASSERTION(
      (mUnit == eStyleUnit_Enumerated) || (mUnit == eStyleUnit_Integer),
      "not an int value");
  if ((mUnit == eStyleUnit_Enumerated) || (mUnit == eStyleUnit_Integer)) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsStyleCoord::GetPercentValue() const {
  NS_ASSERTION(mUnit == eStyleUnit_Percent, "not a percent value");
  if (mUnit == eStyleUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetFactorValue() const {
  NS_ASSERTION(mUnit == eStyleUnit_Factor, "not a factor value");
  if (mUnit == eStyleUnit_Factor) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetFactorOrPercentValue() const {
  NS_ASSERTION(mUnit == eStyleUnit_Factor || mUnit == eStyleUnit_Percent,
               "not a percent or factor value");
  if (mUnit == eStyleUnit_Factor || mUnit == eStyleUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsStyleCoord::GetAngleValue() const {
  MOZ_ASSERT(mUnit == eStyleUnit_Degree);
  return mValue.mFloat;
}

inline float nsStyleCoord::GetFlexFractionValue() const {
  NS_ASSERTION(mUnit == eStyleUnit_FlexFraction, "not a fr value");
  if (mUnit == eStyleUnit_FlexFraction) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsStyleCoord::Calc* nsStyleCoord::GetCalcValue() const {
  NS_ASSERTION(IsCalcUnit(), "not a pointer value");
  if (IsCalcUnit()) {
    return AsCalcValue(mValue);
  }
  return nullptr;
}

/* static */ inline void nsStyleCoord::Reset(nsStyleUnit& aUnit,
                                             nsStyleUnion& aValue) {
  MOZ_ASSERT(aUnit <= eStyleUnit_MAX,
             "calling Reset on uninitialized nsStyleCoord?");

  switch (aUnit) {
    case eStyleUnit_Calc:
      static_cast<Calc*>(aValue.mPointer)->Release();
      break;
    default:
      MOZ_ASSERT(!IsPointerUnit(aUnit), "check pointer refcounting logic");
  }

  aUnit = eStyleUnit_Null;
  aValue.mInt = 0;
}

/* static */ inline void nsStyleCoord::SetValue(
    nsStyleUnit& aUnit, nsStyleUnion& aValue, nsStyleUnit aOtherUnit,
    const nsStyleUnion& aOtherValue) {
  Reset(aUnit, aValue);
  InitWithValue(aUnit, aValue, aOtherUnit, aOtherValue);
}

/* static */ inline void nsStyleCoord::InitWithValue(
    nsStyleUnit& aUnit, nsStyleUnion& aValue, nsStyleUnit aOtherUnit,
    const nsStyleUnion& aOtherValue) {
  aUnit = aOtherUnit;
  aValue = aOtherValue;

  switch (aUnit) {
    case eStyleUnit_Calc:
      static_cast<Calc*>(aValue.mPointer)->AddRef();
      break;
    default:
      MOZ_ASSERT(!IsPointerUnit(aUnit), "check pointer refcounting logic");
  }
}

/* static */ inline void nsStyleCoord::SetValue(nsStyleUnit& aUnit,
                                                nsStyleUnion& aValue,
                                                const nsStyleCoord& aOther) {
  SetValue(aUnit, aValue, aOther.mUnit, aOther.mValue);
}

/* static */ inline void nsStyleCoord::InitWithValue(
    nsStyleUnit& aUnit, nsStyleUnion& aValue, const nsStyleCoord& aOther) {
  InitWithValue(aUnit, aValue, aOther.mUnit, aOther.mValue);
}

// -------------------------
// nsStyleSides inlines
//
inline bool nsStyleSides::operator!=(const nsStyleSides& aOther) const {
  return !((*this) == aOther);
}

inline nsStyleUnit nsStyleSides::GetUnit(mozilla::Side aSide) const {
  return (nsStyleUnit)mUnits[aSide];
}

inline nsStyleUnit nsStyleSides::GetLeftUnit() const {
  return GetUnit(mozilla::eSideLeft);
}

inline nsStyleUnit nsStyleSides::GetTopUnit() const {
  return GetUnit(mozilla::eSideTop);
}

inline nsStyleUnit nsStyleSides::GetRightUnit() const {
  return GetUnit(mozilla::eSideRight);
}

inline nsStyleUnit nsStyleSides::GetBottomUnit() const {
  return GetUnit(mozilla::eSideBottom);
}

inline nsStyleCoord nsStyleSides::Get(mozilla::Side aSide) const {
  return nsStyleCoord(mValues[aSide], nsStyleUnit(mUnits[aSide]));
}

inline nsStyleCoord nsStyleSides::GetLeft() const {
  return Get(mozilla::eSideLeft);
}

inline nsStyleCoord nsStyleSides::GetTop() const {
  return Get(mozilla::eSideTop);
}

inline nsStyleCoord nsStyleSides::GetRight() const {
  return Get(mozilla::eSideRight);
}

inline nsStyleCoord nsStyleSides::GetBottom() const {
  return Get(mozilla::eSideBottom);
}

inline void nsStyleSides::Set(mozilla::Side aSide, const nsStyleCoord& aCoord) {
  nsStyleCoord::SetValue(mUnits[aSide], mValues[aSide], aCoord);
}

inline void nsStyleSides::SetLeft(const nsStyleCoord& aCoord) {
  Set(mozilla::eSideLeft, aCoord);
}

inline void nsStyleSides::SetTop(const nsStyleCoord& aCoord) {
  Set(mozilla::eSideTop, aCoord);
}

inline void nsStyleSides::SetRight(const nsStyleCoord& aCoord) {
  Set(mozilla::eSideRight, aCoord);
}

inline void nsStyleSides::SetBottom(const nsStyleCoord& aCoord) {
  Set(mozilla::eSideBottom, aCoord);
}
#endif /* nsStyleCoord_h___ */
