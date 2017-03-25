/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for animation of computed style values */

#ifndef mozilla_StyleAnimationValue_h_
#define mozilla_StyleAnimationValue_h_

#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"
#include "nsStringBuffer.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsStyleCoord.h"
#include "nsStyleTransformMatrix.h"

class nsIFrame;
class nsStyleContext;
class gfx3DMatrix;

namespace mozilla {

namespace css {
class StyleRule;
} // namespace css

namespace dom {
class Element;
} // namespace dom

enum class CSSPseudoElementType : uint8_t;
struct PropertyStyleAnimationValuePair;

/**
 * Utility class to handle animated style values
 */
class StyleAnimationValue {
public:
  // Mathematical methods
  // --------------------
  /**
   * Adds |aCount| copies of |aValueToAdd| to |aDest|.  The result of this
   * addition is stored in aDest.
   *
   * Note that if |aCount| is 0, then |aDest| will be unchanged.  Also, if
   * this method fails, then |aDest| will be unchanged.
   *
   * @param aDest       The value to add to.
   * @param aValueToAdd The value to add.
   * @param aCount      The number of times to add aValueToAdd.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool
  Add(nsCSSPropertyID aProperty, StyleAnimationValue& aDest,
      const StyleAnimationValue& aValueToAdd, uint32_t aCount) {
    return AddWeighted(aProperty, 1.0, aDest, aCount, aValueToAdd, aDest);
  }

  /**
   * Alternative version of Add that reflects the naming used in Web Animations
   * and which returns the result using the return value.
   */
  static StyleAnimationValue
  Add(nsCSSPropertyID aProperty,
      const StyleAnimationValue& aA,
      StyleAnimationValue&& aB);

  /**
   * Calculates a measure of 'distance' between two colors.
   *
   * @param aStartColor The start of the interval for which the distance
   *                    should be calculated.
   * @param aEndColor   The end of the interval for which the distance
   *                    should be calculated.
   * @return the result of the calculation.
   */
  static double ComputeColorDistance(const css::RGBAColorData& aStartColor,
                                     const css::RGBAColorData& aEndColor);

  /**
   * Calculates a measure of 'distance' between two values.
   *
   * This measure of Distance is guaranteed to be proportional to
   * portions passed to Interpolate, Add, or AddWeighted.  However, for
   * some types of StyleAnimationValue it may not produce sensible results
   * for paced animation.
   *
   * If this method succeeds, the returned distance value is guaranteed to be
   * non-negative.
   *
   * @param aStartValue The start of the interval for which the distance
   *                    should be calculated.
   * @param aEndValue   The end of the interval for which the distance
   *                    should be calculated.
   * @param aStyleContext The style context to use for processing the
   *                      translate part of transforms.
   * @param aDistance   The result of the calculation.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool
  ComputeDistance(nsCSSPropertyID aProperty,
                  const StyleAnimationValue& aStartValue,
                  const StyleAnimationValue& aEndValue,
                  nsStyleContext* aStyleContext,
                  double& aDistance);

  /**
   * Calculates an interpolated value that is the specified |aPortion| between
   * the two given values.
   *
   * This really just does the following calculation:
   *   aResultValue = (1.0 - aPortion) * aStartValue + aPortion * aEndValue
   *
   * @param aStartValue The value defining the start of the interval of
   *                    interpolation.
   * @param aEndValue   The value defining the end of the interval of
   *                    interpolation.
   * @param aPortion    A number in the range [0.0, 1.0] defining the
   *                    distance of the interpolated value in the interval.
   * @param [out] aResultValue The resulting interpolated value.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool
  Interpolate(nsCSSPropertyID aProperty,
              const StyleAnimationValue& aStartValue,
              const StyleAnimationValue& aEndValue,
              double aPortion,
              StyleAnimationValue& aResultValue) {
    return AddWeighted(aProperty, 1.0 - aPortion, aStartValue,
                       aPortion, aEndValue, aResultValue);
  }

  /**
   * Does the calculation:
   *   aResultValue = aCoeff1 * aValue1 + aCoeff2 * aValue2
   *
   * @param [out] aResultValue The resulting interpolated value.  May be
   *                           the same as aValue1 or aValue2.
   * @return true on success, false on failure.
   *
   * NOTE: Current callers always pass aCoeff1 and aCoeff2 >= 0.  They
   * are currently permitted to be negative; however, if, as we add
   * support more value types types, we find that this causes
   * difficulty, we might change this to restrict them to being
   * positive.
   */
  static MOZ_MUST_USE bool
  AddWeighted(nsCSSPropertyID aProperty,
              double aCoeff1, const StyleAnimationValue& aValue1,
              double aCoeff2, const StyleAnimationValue& aValue2,
              StyleAnimationValue& aResultValue);

  /**
   * Accumulates |aA| onto |aA| (|aCount| - 1) times then accumulates |aB| onto
   * the result.
   * If |aCount| is zero or no accumulation or addition procedure is defined
   * for |aProperty|, the result will be |aB|.
   */
  static StyleAnimationValue
  Accumulate(nsCSSPropertyID aProperty,
             const StyleAnimationValue& aA,
             StyleAnimationValue&& aB,
             uint64_t aCount = 1);

  // Type-conversion methods
  // -----------------------
  /**
   * Creates a computed value for the given specified value
   * (property ID + string).  A style context is needed in case the
   * specified value depends on inherited style or on the values of other
   * properties.
   *
   * @param aProperty       The property whose value we're computing.
   * @param aTargetElement  The content node to which our computed value is
   *                        applicable. For pseudo-elements, this is the parent
   *                        element to which the pseudo is attached, not the
   *                        generated content node.
   * @param aStyleContext   The style context used to compute values from the
   *                        specified value. For pseudo-elements, this should
   *                        be the style context corresponding to the pseudo
   *                        element.
   * @param aSpecifiedValue The specified value, from which we'll build our
   *                        computed value.
   * @param aUseSVGMode     A flag to indicate whether we should parse
   *                        |aSpecifiedValue| in SVG mode.
   * @param [out] aComputedValue The resulting computed value.
   * @param [out] aIsContextSensitive
   *                        Set to true if |aSpecifiedValue| may produce
   *                        a different |aComputedValue| depending on other CSS
   *                        properties on |aTargetElement| or its ancestors.
   *                        false otherwise.
   *                        Note that the operation of this method is
   *                        significantly faster when |aIsContextSensitive| is
   *                        nullptr.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool
  ComputeValue(nsCSSPropertyID aProperty,
               mozilla::dom::Element* aTargetElement,
               nsStyleContext* aStyleContext,
               const nsAString& aSpecifiedValue,
               bool aUseSVGMode,
               StyleAnimationValue& aComputedValue,
               bool* aIsContextSensitive = nullptr);

  /**
   * Like ComputeValue, but returns an array of StyleAnimationValues.
   *
   * On success, when aProperty is a longhand, aResult will have a single
   * value in it.  When aProperty is a shorthand, aResult will be filled with
   * values for all of aProperty's longhand components.  aEnabledState
   * is used to filter the longhand components that will be appended
   * to aResult.  On failure, aResult might still have partial results
   * in it.
   */
  static MOZ_MUST_USE bool
  ComputeValues(nsCSSPropertyID aProperty,
                mozilla::CSSEnabledState aEnabledState,
                mozilla::dom::Element* aTargetElement,
                nsStyleContext* aStyleContext,
                const nsAString& aSpecifiedValue,
                bool aUseSVGMode,
                nsTArray<PropertyStyleAnimationValuePair>& aResult);

  /**
   * A variant on ComputeValues that takes an nsCSSValue as the specified
   * value. Only longhand properties are supported.
   */
  static MOZ_MUST_USE bool
  ComputeValues(nsCSSPropertyID aProperty,
                mozilla::CSSEnabledState aEnabledState,
                mozilla::dom::Element* aTargetElement,
                nsStyleContext* aStyleContext,
                const nsCSSValue& aSpecifiedValue,
                bool aUseSVGMode,
                nsTArray<PropertyStyleAnimationValuePair>& aResult);

  /**
   * Creates a specified value for the given computed value.
   *
   * The first two overloads fill in an nsCSSValue object; the third
   * produces a string.  For the overload that takes a const
   * StyleAnimationValue& reference, the nsCSSValue result may depend on
   * objects owned by the |aComputedValue| object, so users of that variant
   * must keep |aComputedValue| alive longer than |aSpecifiedValue|.
   * The overload that takes an rvalue StyleAnimationValue reference
   * transfers ownership for some resources such that the |aComputedValue|
   * does not depend on the lifetime of |aSpecifiedValue|.
   *
   * @param aProperty      The property whose value we're uncomputing.
   * @param aComputedValue The computed value to be converted.
   * @param [out] aSpecifiedValue The resulting specified value.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool
  UncomputeValue(nsCSSPropertyID aProperty,
                 const StyleAnimationValue& aComputedValue,
                 nsCSSValue& aSpecifiedValue);
  static MOZ_MUST_USE bool
  UncomputeValue(nsCSSPropertyID aProperty,
                 StyleAnimationValue&& aComputedValue,
                 nsCSSValue& aSpecifiedValue);
  static MOZ_MUST_USE bool
  UncomputeValue(nsCSSPropertyID aProperty,
                 const StyleAnimationValue& aComputedValue,
                 nsAString& aSpecifiedValue);

  /**
   * Gets the computed value for the given property from the given style
   * context.
   *
   * Obtaining the computed value allows us to animate properties when the
   * content author has specified a value like "inherit" or "initial" or some
   * other keyword that isn't directly interpolatable, but which *computes* to
   * something interpolatable.
   *
   * @param aProperty     The property whose value we're looking up.
   * @param aStyleContext The style context to check for the computed value.
   * @param [out] aComputedValue The resulting computed value.
   * @return true on success, false on failure.
   */
  static MOZ_MUST_USE bool ExtractComputedValue(
    nsCSSPropertyID aProperty,
    nsStyleContext* aStyleContext,
    StyleAnimationValue& aComputedValue);

  static already_AddRefed<nsCSSValue::Array>
    AppendTransformFunction(nsCSSKeyword aTransformFunction,
                            nsCSSValueList**& aListTail);

  /**
   * The types and values for the values that we extract and animate.
   */
  enum Unit {
    eUnit_Null, // not initialized
    eUnit_Normal,
    eUnit_Auto,
    eUnit_None,
    eUnit_Enumerated,
    eUnit_Visibility, // special case for transitions (which converts
                      // Enumerated to Visibility as needed)
    eUnit_Integer,
    eUnit_Coord,
    eUnit_Percent,
    eUnit_Float,
    eUnit_Color, // nsCSSValue* (never null), always with an nscolor or
                 // an nsCSSValueFloatColor
    eUnit_CurrentColor,
    eUnit_ComplexColor, // ComplexColorValue* (never null)
    eUnit_Calc, // nsCSSValue* (never null), always with a single
                // calc() expression that's either length or length+percent
    eUnit_ObjectPosition, // nsCSSValue* (never null), always with a
                          // 4-entry nsCSSValue::Array
    eUnit_URL, // nsCSSValue* (never null), always with a css::URLValue
    eUnit_DiscreteCSSValue, // nsCSSValue* (never null)
    eUnit_CSSValuePair, // nsCSSValuePair* (never null)
    eUnit_CSSValueTriplet, // nsCSSValueTriplet* (never null)
    eUnit_CSSRect, // nsCSSRect* (never null)
    eUnit_Dasharray, // nsCSSValueList* (never null)
    eUnit_Shadow, // nsCSSValueList* (may be null)
    eUnit_Shape,  // nsCSSValue::Array* (never null)
    eUnit_Filter, // nsCSSValueList* (may be null)
    eUnit_Transform, // nsCSSValueList* (never null)
    eUnit_BackgroundPositionCoord, // nsCSSValueList* (never null)
    eUnit_CSSValuePairList, // nsCSSValuePairList* (never null)
    eUnit_UnparsedString // nsStringBuffer* (never null)
  };

private:
  Unit mUnit;
  union {
    int32_t mInt;
    nscoord mCoord;
    float mFloat;
    nsCSSValue* mCSSValue;
    nsCSSValuePair* mCSSValuePair;
    nsCSSValueTriplet* mCSSValueTriplet;
    nsCSSRect* mCSSRect;
    nsCSSValue::Array* mCSSValueArray;
    nsCSSValueList* mCSSValueList;
    nsCSSValueSharedList* mCSSValueSharedList;
    nsCSSValuePairList* mCSSValuePairList;
    nsStringBuffer* mString;
    css::ComplexColorValue* mComplexColor;
  } mValue;

public:
  Unit GetUnit() const {
    NS_ASSERTION(mUnit != eUnit_Null, "uninitialized");
    return mUnit;
  }

  // Accessor to let us verify assumptions about presence of null unit,
  // without tripping the assertion in GetUnit().
  bool IsNull() const {
    return mUnit == eUnit_Null;
  }

  int32_t GetIntValue() const {
    NS_ASSERTION(IsIntUnit(mUnit), "unit mismatch");
    return mValue.mInt;
  }
  nscoord GetCoordValue() const {
    NS_ASSERTION(mUnit == eUnit_Coord, "unit mismatch");
    return mValue.mCoord;
  }
  float GetPercentValue() const {
    NS_ASSERTION(mUnit == eUnit_Percent, "unit mismatch");
    return mValue.mFloat;
  }
  float GetFloatValue() const {
    NS_ASSERTION(mUnit == eUnit_Float, "unit mismatch");
    return mValue.mFloat;
  }
  nsCSSValue* GetCSSValueValue() const {
    NS_ASSERTION(IsCSSValueUnit(mUnit), "unit mismatch");
    return mValue.mCSSValue;
  }
  nsCSSValuePair* GetCSSValuePairValue() const {
    NS_ASSERTION(IsCSSValuePairUnit(mUnit), "unit mismatch");
    return mValue.mCSSValuePair;
  }
  nsCSSValueTriplet* GetCSSValueTripletValue() const {
    NS_ASSERTION(IsCSSValueTripletUnit(mUnit), "unit mismatch");
    return mValue.mCSSValueTriplet;
  }
  nsCSSRect* GetCSSRectValue() const {
    NS_ASSERTION(IsCSSRectUnit(mUnit), "unit mismatch");
    return mValue.mCSSRect;
  }
  nsCSSValue::Array* GetCSSValueArrayValue() const {
    NS_ASSERTION(IsCSSValueArrayUnit(mUnit), "unit mismatch");
    return mValue.mCSSValueArray;
  }
  nsCSSValueList* GetCSSValueListValue() const {
    NS_ASSERTION(IsCSSValueListUnit(mUnit), "unit mismatch");
    return mValue.mCSSValueList;
  }
  nsCSSValueSharedList* GetCSSValueSharedListValue() const {
    NS_ASSERTION(IsCSSValueSharedListValue(mUnit), "unit mismatch");
    return mValue.mCSSValueSharedList;
  }
  nsCSSValuePairList* GetCSSValuePairListValue() const {
    NS_ASSERTION(IsCSSValuePairListUnit(mUnit), "unit mismatch");
    return mValue.mCSSValuePairList;
  }
  const char16_t* GetStringBufferValue() const {
    NS_ASSERTION(IsStringUnit(mUnit), "unit mismatch");
    return GetBufferValue(mValue.mString);
  }

  void GetStringValue(nsAString& aBuffer) const {
    NS_ASSERTION(IsStringUnit(mUnit), "unit mismatch");
    aBuffer.Truncate();
    uint32_t len = NS_strlen(GetBufferValue(mValue.mString));
    mValue.mString->ToString(len, aBuffer);
  }

  /// @return the scale for this value, calculated with reference to @aForFrame.
  gfxSize GetScaleValue(const nsIFrame* aForFrame) const;

  const css::ComplexColorData& GetComplexColorData() const {
    MOZ_ASSERT(mUnit == eUnit_ComplexColor, "unit mismatch");
    return *mValue.mComplexColor;
  }
  StyleComplexColor GetStyleComplexColorValue() const {
    return GetComplexColorData().ToComplexColor();
  }

  UniquePtr<nsCSSValueList> TakeCSSValueListValue() {
    nsCSSValueList* list = GetCSSValueListValue();
    mValue.mCSSValueList = nullptr;
    mUnit = eUnit_Null;
    return UniquePtr<nsCSSValueList>(list);
  }
  UniquePtr<nsCSSValuePairList> TakeCSSValuePairListValue() {
    nsCSSValuePairList* list = GetCSSValuePairListValue();
    mValue.mCSSValuePairList = nullptr;
    mUnit = eUnit_Null;
    return UniquePtr<nsCSSValuePairList>(list);
  }

  explicit StyleAnimationValue(Unit aUnit = eUnit_Null) : mUnit(aUnit) {
    NS_ASSERTION(aUnit == eUnit_Null || aUnit == eUnit_Normal ||
                 aUnit == eUnit_Auto || aUnit == eUnit_None,
                 "must be valueless unit");
  }
  StyleAnimationValue(const StyleAnimationValue& aOther)
    : mUnit(eUnit_Null) { *this = aOther; }
  StyleAnimationValue(StyleAnimationValue&& aOther)
    : mUnit(aOther.mUnit)
    , mValue(aOther.mValue)
  {
    aOther.mUnit = eUnit_Null;
  }
  enum IntegerConstructorType { IntegerConstructor };
  StyleAnimationValue(int32_t aInt, Unit aUnit, IntegerConstructorType);
  enum CoordConstructorType { CoordConstructor };
  StyleAnimationValue(nscoord aLength, CoordConstructorType);
  enum PercentConstructorType { PercentConstructor };
  StyleAnimationValue(float aPercent, PercentConstructorType);
  enum FloatConstructorType { FloatConstructor };
  StyleAnimationValue(float aFloat, FloatConstructorType);
  enum ColorConstructorType { ColorConstructor };
  StyleAnimationValue(nscolor aColor, ColorConstructorType);

  ~StyleAnimationValue() { FreeValue(); }

  void SetNormalValue();
  void SetAutoValue();
  void SetNoneValue();
  void SetIntValue(int32_t aInt, Unit aUnit);
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetEnumValue(T aInt)
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    SetIntValue(static_cast<int32_t>(aInt), eUnit_Enumerated);
  }
  void SetCoordValue(nscoord aCoord);
  void SetPercentValue(float aPercent);
  void SetFloatValue(float aFloat);
  void SetColorValue(nscolor aColor);
  void SetCurrentColorValue();
  void SetComplexColorValue(const StyleComplexColor& aColor);
  void SetComplexColorValue(already_AddRefed<css::ComplexColorValue> aValue);
  void SetUnparsedStringValue(const nsString& aString);
  void SetCSSValueArrayValue(nsCSSValue::Array* aValue, Unit aUnit);

  // These setters take ownership of |aValue|, and are therefore named
  // "SetAndAdopt*".
  void SetAndAdoptCSSValueValue(nsCSSValue *aValue, Unit aUnit);
  void SetAndAdoptCSSValuePairValue(nsCSSValuePair *aValue, Unit aUnit);
  void SetAndAdoptCSSValueTripletValue(nsCSSValueTriplet *aValue, Unit aUnit);
  void SetAndAdoptCSSRectValue(nsCSSRect *aValue, Unit aUnit);
  void SetAndAdoptCSSValueListValue(nsCSSValueList *aValue, Unit aUnit);
  void SetAndAdoptCSSValuePairListValue(nsCSSValuePairList *aValue);

  void SetTransformValue(nsCSSValueSharedList* aList);

  StyleAnimationValue& operator=(const StyleAnimationValue& aOther);
  StyleAnimationValue& operator=(StyleAnimationValue&& aOther)
  {
    MOZ_ASSERT(this != &aOther, "Do not move itself");
    if (this != &aOther) {
      FreeValue();
      mUnit = aOther.mUnit;
      mValue = aOther.mValue;
      aOther.mUnit = eUnit_Null;
    }
    return *this;
  }

  bool operator==(const StyleAnimationValue& aOther) const;
  bool operator!=(const StyleAnimationValue& aOther) const
    { return !(*this == aOther); }

private:
  void FreeValue();

  static const char16_t* GetBufferValue(nsStringBuffer* aBuffer) {
    return static_cast<char16_t*>(aBuffer->Data());
  }

  static bool IsIntUnit(Unit aUnit) {
    return aUnit == eUnit_Enumerated || aUnit == eUnit_Visibility ||
           aUnit == eUnit_Integer;
  }
  static bool IsCSSValueUnit(Unit aUnit) {
    return aUnit == eUnit_Color ||
           aUnit == eUnit_Calc ||
           aUnit == eUnit_ObjectPosition ||
           aUnit == eUnit_URL ||
           aUnit == eUnit_DiscreteCSSValue;
  }
  static bool IsCSSValuePairUnit(Unit aUnit) {
    return aUnit == eUnit_CSSValuePair;
  }
  static bool IsCSSValueTripletUnit(Unit aUnit) {
    return aUnit == eUnit_CSSValueTriplet;
  }
  static bool IsCSSRectUnit(Unit aUnit) {
    return aUnit == eUnit_CSSRect;
  }
  static bool IsCSSValueArrayUnit(Unit aUnit) {
    return aUnit == eUnit_Shape;
  }
  static bool IsCSSValueListUnit(Unit aUnit) {
    return aUnit == eUnit_Dasharray || aUnit == eUnit_Filter ||
           aUnit == eUnit_Shadow ||
           aUnit == eUnit_BackgroundPositionCoord;
  }
  static bool IsCSSValueSharedListValue(Unit aUnit) {
    return aUnit == eUnit_Transform;
  }
  static bool IsCSSValuePairListUnit(Unit aUnit) {
    return aUnit == eUnit_CSSValuePairList;
  }
  static bool IsStringUnit(Unit aUnit) {
    return aUnit == eUnit_UnparsedString;
  }
};

struct AnimationValue
{
  StyleAnimationValue mGecko;
  RefPtr<RawServoAnimationValue> mServo;

  inline bool operator==(const AnimationValue& aOther) const;

  bool IsNull() const { return mGecko.IsNull() && !mServo; }

  inline float GetOpacity() const;

  // Returns the scale for mGecko or mServo, which are calculated with
  // reference to aFrame.
  inline gfxSize GetScaleValue(const nsIFrame* aFrame) const;

  // Uncompute this AnimationValue and then serialize it.
  inline void SerializeSpecifiedValue(nsCSSPropertyID aProperty,
                                      nsAString& aString) const;
};

struct PropertyStyleAnimationValuePair
{
  nsCSSPropertyID mProperty;
  AnimationValue mValue;
};
} // namespace mozilla

#endif
