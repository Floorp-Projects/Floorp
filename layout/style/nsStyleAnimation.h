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
 * The Original Code is nsStyleAnimation.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>, Mozilla Corporation
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Utilities for animation of computed style values */

#ifndef nsStyleAnimation_h_
#define nsStyleAnimation_h_

#include "prtypes.h"
#include "nsAString.h"
#include "nsCRTGlue.h"
#include "nsStringBuffer.h"
#include "nsCSSProperty.h"
#include "nsCoord.h"
#include "nsColor.h"

class nsPresContext;
class nsStyleContext;
class nsCSSValue;
struct nsCSSValueList;
struct nsCSSValuePair;
struct nsCSSValueTriplet;
struct nsCSSValuePairList;
struct nsCSSRect;
struct gfxMatrix;

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
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool Add(nsCSSProperty aProperty, Value& aDest,
                    const Value& aValueToAdd, PRUint32 aCount) {
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
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ComputeDistance(nsCSSProperty aProperty,
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
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool Interpolate(nsCSSProperty aProperty,
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
   * @return PR_TRUE on success, PR_FALSE on failure.
   *
   * NOTE: Current callers always pass aCoeff1 and aCoeff2 >= 0.  They
   * are currently permitted to be negative; however, if, as we add
   * support more value types types, we find that this causes
   * difficulty, we might change this to restrict them to being
   * positive.
   */
  static PRBool AddWeighted(nsCSSProperty aProperty,
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
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ComputeValue(nsCSSProperty aProperty,
                             mozilla::dom::Element* aElement,
                             const nsAString& aSpecifiedValue,
                             PRBool aUseSVGMode,
                             Value& aComputedValue);

  /**
   * Creates a specified value for the given computed value.
   *
   * The first overload fills in an nsCSSValue object; the second
   * produces a string.  The nsCSSValue result may depend on objects
   * owned by the |aComputedValue| object, so users of that variant
   * must keep |aComputedValue| alive longer than |aSpecifiedValue|.
   *
   * @param aProperty      The property whose value we're uncomputing.
   * @param aPresContext   The presentation context for the document in
   *                       which we're working.
   * @param aComputedValue The computed value to be converted.
   * @param [out] aSpecifiedValue The resulting specified value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool UncomputeValue(nsCSSProperty aProperty,
                               nsPresContext* aPresContext,
                               const Value& aComputedValue,
                               nsCSSValue& aSpecifiedValue);
  static PRBool UncomputeValue(nsCSSProperty aProperty,
                               nsPresContext* aPresContext,
                               const Value& aComputedValue,
                               nsAString& aSpecifiedValue);

  /**
   * Gets the computed value for the given property from the given style
   * context.
   *
   * @param aProperty     The property whose value we're looking up.
   * @param aStyleContext The style context to check for the computed value.
   * @param [out] aComputedValue The resulting computed value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ExtractComputedValue(nsCSSProperty aProperty,
                                     nsStyleContext* aStyleContext,
                                     Value& aComputedValue);

   /**
    * Interpolates between 2 matrices by decomposing them.
    *
    * @param aMatrix1   First matrix, using CSS pixel units.
    * @param aCoeff1    Interpolation value in the range [0.0, 1.0]
    * @param aMatrix2   Second matrix, using CSS pixel units.
    * @param aCoeff2    Interpolation value in the range [0.0, 1.0]
    */
   static gfxMatrix InterpolateTransformMatrix(const gfxMatrix &aMatrix1, double aCoeff1,
                                               const gfxMatrix &aMatrix2, double aCoeff2);

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
    eUnit_CSSValuePairList, // nsCSSValuePairList* (never null)
    eUnit_UnparsedString // nsStringBuffer* (never null)
  };

  class Value {
  private:
    Unit mUnit;
    union {
      PRInt32 mInt;
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
    PRBool IsNull() const {
      return mUnit == eUnit_Null;
    }

    PRInt32 GetIntValue() const {
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
      PRUint32 len = NS_strlen(GetBufferValue(mValue.mString));
      mValue.mString->ToString(len, aBuffer);
    }

    explicit Value(Unit aUnit = eUnit_Null) : mUnit(aUnit) {
      NS_ASSERTION(aUnit == eUnit_Null || aUnit == eUnit_Normal ||
                   aUnit == eUnit_Auto || aUnit == eUnit_None,
                   "must be valueless unit");
    }
    Value(const Value& aOther) : mUnit(eUnit_Null) { *this = aOther; }
    enum IntegerConstructorType { IntegerConstructor };
    Value(PRInt32 aInt, Unit aUnit, IntegerConstructorType);
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
    void SetIntValue(PRInt32 aInt, Unit aUnit);
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

    PRBool operator==(const Value& aOther) const;
    PRBool operator!=(const Value& aOther) const
      { return !(*this == aOther); }

  private:
    void FreeValue();

    static const PRUnichar* GetBufferValue(nsStringBuffer* aBuffer) {
      return static_cast<PRUnichar*>(aBuffer->Data());
    }

    static PRBool IsIntUnit(Unit aUnit) {
      return aUnit == eUnit_Enumerated || aUnit == eUnit_Visibility ||
             aUnit == eUnit_Integer;
    }
    static PRBool IsCSSValueUnit(Unit aUnit) {
      return aUnit == eUnit_Calc;
    }
    static PRBool IsCSSValuePairUnit(Unit aUnit) {
      return aUnit == eUnit_CSSValuePair;
    }
    static PRBool IsCSSValueTripletUnit(Unit aUnit) {
      return aUnit == eUnit_CSSValueTriplet;
    }
    static PRBool IsCSSRectUnit(Unit aUnit) {
      return aUnit == eUnit_CSSRect;
    }
    static PRBool IsCSSValueListUnit(Unit aUnit) {
      return aUnit == eUnit_Dasharray || aUnit == eUnit_Shadow ||
             aUnit == eUnit_Transform;
    }
    static PRBool IsCSSValuePairListUnit(Unit aUnit) {
      return aUnit == eUnit_CSSValuePairList;
    }
    static PRBool IsStringUnit(Unit aUnit) {
      return aUnit == eUnit_UnparsedString;
    }
  };
};

#endif
