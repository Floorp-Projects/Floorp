/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for animation of computed style values */

#ifndef nsStyleAnimation_h_
#define nsStyleAnimation_h_

#include "nsAString.h"
#include "nsCRTGlue.h"
#include "nsStringBuffer.h"
#include "nsCSSProperty.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsCSSValue.h"

class nsStyleContext;
class gfx3DMatrix;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/**
 * Utility class to handle animated style values
 */
class nsStyleAnimation {
public:
  class Value;

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
  static bool Add(nsCSSProperty aProperty, Value& aDest,
                    const Value& aValueToAdd, uint32_t aCount) {
    return AddWeighted(aProperty, 1.0, aDest, aCount, aValueToAdd, aDest);
  }

  /**
   * Calculates a measure of 'distance' between two values.
   *
   * This measure of Distance is guaranteed to be proportional to
   * portions passed to Interpolate, Add, or AddWeighted.  However, for
   * some types of Value it may not produce sensible results for paced
   * animation.
   *
   * If this method succeeds, the returned distance value is guaranteed to be
   * non-negative.
   *
   * @param aStartValue The start of the interval for which the distance
   *                    should be calculated.
   * @param aEndValue   The end of the interval for which the distance
   *                    should be calculated.
   * @param aDistance   The result of the calculation.
   * @return true on success, false on failure.
   */
  static bool ComputeDistance(nsCSSProperty aProperty,
                                const Value& aStartValue,
                                const Value& aEndValue,
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
  static bool Interpolate(nsCSSProperty aProperty,
                            const Value& aStartValue,
                            const Value& aEndValue,
                            double aPortion,
                            Value& aResultValue) {
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
  static bool AddWeighted(nsCSSProperty aProperty,
                            double aCoeff1, const Value& aValue1,
                            double aCoeff2, const Value& aValue2,
                            Value& aResultValue);

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
   *                        applicable.
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
  static bool ComputeValue(nsCSSProperty aProperty,
                             mozilla::dom::Element* aTargetElement,
                             const nsAString& aSpecifiedValue,
                             bool aUseSVGMode,
                             Value& aComputedValue,
                             bool* aIsContextSensitive = nullptr);

  /**
   * Creates a specified value for the given computed value.
   *
   * The first overload fills in an nsCSSValue object; the second
   * produces a string.  The nsCSSValue result may depend on objects
   * owned by the |aComputedValue| object, so users of that variant
   * must keep |aComputedValue| alive longer than |aSpecifiedValue|.
   *
   * @param aProperty      The property whose value we're uncomputing.
   * @param aComputedValue The computed value to be converted.
   * @param [out] aSpecifiedValue The resulting specified value.
   * @return true on success, false on failure.
   */
  static bool UncomputeValue(nsCSSProperty aProperty,
                             const Value& aComputedValue,
                             nsCSSValue& aSpecifiedValue);
  static bool UncomputeValue(nsCSSProperty aProperty,
                             const Value& aComputedValue,
                             nsAString& aSpecifiedValue);

  /**
   * Gets the computed value for the given property from the given style
   * context.
   *
   * @param aProperty     The property whose value we're looking up.
   * @param aStyleContext The style context to check for the computed value.
   * @param [out] aComputedValue The resulting computed value.
   * @return true on success, false on failure.
   */
  static bool ExtractComputedValue(nsCSSProperty aProperty,
                                     nsStyleContext* aStyleContext,
                                     Value& aComputedValue);

   /**
    * Interpolates between 2 matrices by decomposing them.
    *
    * @param aMatrix1   First matrix, using CSS pixel units.
    * @param aMatrix2   Second matrix, using CSS pixel units.
    * @param aProgress  Interpolation value in the range [0.0, 1.0]
    */
   static gfx3DMatrix InterpolateTransformMatrix(const gfx3DMatrix &aMatrix1,
                                                 const gfx3DMatrix &aMatrix2,
                                                 double aProgress);

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
    eUnit_Color,
    eUnit_Calc, // nsCSSValue* (never null), always with a single
                // calc() expression that's either length or length+percent
    eUnit_CSSValuePair, // nsCSSValuePair* (never null)
    eUnit_CSSValueTriplet, // nsCSSValueTriplet* (never null)
    eUnit_CSSRect, // nsCSSRect* (never null)
    eUnit_Dasharray, // nsCSSValueList* (never null)
    eUnit_Shadow, // nsCSSValueList* (may be null)
    eUnit_Transform, // nsCSSValueList* (never null)
    eUnit_BackgroundPosition, // nsCSSValueList* (never null)
    eUnit_CSSValuePairList, // nsCSSValuePairList* (never null)
    eUnit_UnparsedString // nsStringBuffer* (never null)
  };

  class Value {
  private:
    Unit mUnit;
    union {
      int32_t mInt;
      nscoord mCoord;
      float mFloat;
      nscolor mColor;
      nsCSSValue* mCSSValue;
      nsCSSValuePair* mCSSValuePair;
      nsCSSValueTriplet* mCSSValueTriplet;
      nsCSSRect* mCSSRect;
      nsCSSValueList* mCSSValueList;
      nsCSSValuePairList* mCSSValuePairList;
      nsStringBuffer* mString;
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
    nscolor GetColorValue() const {
      NS_ASSERTION(mUnit == eUnit_Color, "unit mismatch");
      return mValue.mColor;
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
    nsCSSValueList* GetCSSValueListValue() const {
      NS_ASSERTION(IsCSSValueListUnit(mUnit), "unit mismatch");
      return mValue.mCSSValueList;
    }
    nsCSSValuePairList* GetCSSValuePairListValue() const {
      NS_ASSERTION(IsCSSValuePairListUnit(mUnit), "unit mismatch");
      return mValue.mCSSValuePairList;
    }
    const PRUnichar* GetStringBufferValue() const {
      NS_ASSERTION(IsStringUnit(mUnit), "unit mismatch");
      return GetBufferValue(mValue.mString);
    }

    void GetStringValue(nsAString& aBuffer) const {
      NS_ASSERTION(IsStringUnit(mUnit), "unit mismatch");
      aBuffer.Truncate();
      uint32_t len = NS_strlen(GetBufferValue(mValue.mString));
      mValue.mString->ToString(len, aBuffer);
    }

    explicit Value(Unit aUnit = eUnit_Null) : mUnit(aUnit) {
      NS_ASSERTION(aUnit == eUnit_Null || aUnit == eUnit_Normal ||
                   aUnit == eUnit_Auto || aUnit == eUnit_None,
                   "must be valueless unit");
    }
    Value(const Value& aOther) : mUnit(eUnit_Null) { *this = aOther; }
    enum IntegerConstructorType { IntegerConstructor };
    Value(int32_t aInt, Unit aUnit, IntegerConstructorType);
    enum CoordConstructorType { CoordConstructor };
    Value(nscoord aLength, CoordConstructorType);
    enum PercentConstructorType { PercentConstructor };
    Value(float aPercent, PercentConstructorType);
    enum FloatConstructorType { FloatConstructor };
    Value(float aFloat, FloatConstructorType);
    enum ColorConstructorType { ColorConstructor };
    Value(nscolor aColor, ColorConstructorType);

    ~Value() { FreeValue(); }

    void SetNormalValue();
    void SetAutoValue();
    void SetNoneValue();
    void SetIntValue(int32_t aInt, Unit aUnit);
    void SetCoordValue(nscoord aCoord);
    void SetPercentValue(float aPercent);
    void SetFloatValue(float aFloat);
    void SetColorValue(nscolor aColor);
    void SetUnparsedStringValue(const nsString& aString);

    // These setters take ownership of |aValue|, and are therefore named
    // "SetAndAdopt*".
    void SetAndAdoptCSSValueValue(nsCSSValue *aValue, Unit aUnit);
    void SetAndAdoptCSSValuePairValue(nsCSSValuePair *aValue, Unit aUnit);
    void SetAndAdoptCSSValueTripletValue(nsCSSValueTriplet *aValue, Unit aUnit);
    void SetAndAdoptCSSRectValue(nsCSSRect *aValue, Unit aUnit);
    void SetAndAdoptCSSValueListValue(nsCSSValueList *aValue, Unit aUnit);
    void SetAndAdoptCSSValuePairListValue(nsCSSValuePairList *aValue);

    Value& operator=(const Value& aOther);

    bool operator==(const Value& aOther) const;
    bool operator!=(const Value& aOther) const
      { return !(*this == aOther); }

  private:
    void FreeValue();

    static const PRUnichar* GetBufferValue(nsStringBuffer* aBuffer) {
      return static_cast<PRUnichar*>(aBuffer->Data());
    }

    static bool IsIntUnit(Unit aUnit) {
      return aUnit == eUnit_Enumerated || aUnit == eUnit_Visibility ||
             aUnit == eUnit_Integer;
    }
    static bool IsCSSValueUnit(Unit aUnit) {
      return aUnit == eUnit_Calc;
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
    static bool IsCSSValueListUnit(Unit aUnit) {
      return aUnit == eUnit_Dasharray || aUnit == eUnit_Shadow ||
             aUnit == eUnit_Transform || aUnit == eUnit_BackgroundPosition;
    }
    static bool IsCSSValuePairListUnit(Unit aUnit) {
      return aUnit == eUnit_CSSValuePairList;
    }
    static bool IsStringUnit(Unit aUnit) {
      return aUnit == eUnit_UnparsedString;
    }
  };
};

#endif
