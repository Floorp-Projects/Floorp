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

#include "nsStyleAnimation.h"
#include "nsCOMArray.h"
#include "nsIStyleRule.h"
#include "nsICSSStyleRule.h"
#include "nsString.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "nsComputedDOMStyle.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsCSSDataBlock.h"
#include "nsCSSDeclaration.h"
#include "nsCSSStruct.h"
#include "prlog.h"
#include <math.h>

// HELPER METHODS
// --------------
/*
 * Given two units, this method returns a common unit that they can both be
 * converted into, if possible.  This is intended to facilitate
 * interpolation, distance-computation, and addition between "similar" units.
 *
 * The ordering of the arguments should not affect the output of this method.
 *
 * If there's no sensible common unit, this method returns eUnit_Null.
 *
 * @param   aFirstUnit One unit to resolve.
 * @param   aFirstUnit The other unit to resolve.
 * @return  A "common" unit that both source units can be converted into, or
 *          eUnit_Null if that's not possible.
 */
static
nsStyleAnimation::Unit
GetCommonUnit(nsStyleAnimation::Unit aFirstUnit,
              nsStyleAnimation::Unit aSecondUnit)
{
  // XXXdholbert Naive implementation for now: simply require that the input
  // units match.
  if (aFirstUnit != aSecondUnit) {
    // NOTE: Some unit-pairings will need special handling,
    // e.g. percent vs coord (bug 520234)
    return nsStyleAnimation::eUnit_Null;
  }
  return aFirstUnit;
}

// Greatest Common Divisor
static PRUint32
gcd(PRUint32 a, PRUint32 b)
{
  // Euclid's algorithm; O(N) in the worst case.  (There are better
  // ways, but we don't need them for stroke-dasharray animation.)
  NS_ABORT_IF_FALSE(a > 0 && b > 0, "positive numbers expected");

  while (a != b) {
    if (a > b) {
      a = a - b;
    } else {
      b = b - a;
    }
  }

  return a;
}

// Least Common Multiple
static PRUint32
lcm(PRUint32 a, PRUint32 b)
{
  // Divide first to reduce overflow risk.
  return (a / gcd(a, b)) * b;
}

// CLASS METHODS
// -------------

PRBool
nsStyleAnimation::ComputeDistance(const Value& aStartValue,
                                  const Value& aEndValue,
                                  double& aDistance)
{
  Unit commonUnit = GetCommonUnit(aStartValue.GetUnit(), aEndValue.GetUnit());

  PRBool success = PR_TRUE;
  switch (commonUnit) {
    case eUnit_Null:
    case eUnit_None:
    case eUnit_Enumerated:
      success = PR_FALSE;
      break;
    case eUnit_Integer: {
      PRInt32 startInt = aStartValue.GetIntValue();
      PRInt32 endInt = aEndValue.GetIntValue();
      aDistance = PR_ABS(endInt - startInt);
      break;
    }
    case eUnit_Coord: {
      nscoord startCoord = aStartValue.GetCoordValue();
      nscoord endCoord = aEndValue.GetCoordValue();
      aDistance = fabs(double(endCoord - startCoord));
      break;
    }
    case eUnit_Percent: {
      float startPct = aStartValue.GetPercentValue();
      float endPct = aEndValue.GetPercentValue();
      aDistance = fabs(double(endPct - startPct));
      break;
    }
    case eUnit_Float: {
      float startFloat = aStartValue.GetFloatValue();
      float endFloat = aEndValue.GetFloatValue();
      aDistance = fabs(double(endFloat - startFloat));
      break;
    }
    case eUnit_Color: {
      // http://www.w3.org/TR/smil-animation/#animateColorElement says
      // that we should use Euclidean RGB cube distance.  However, we
      // have to extend that to RGBA.  For now, we'll just use the
      // Euclidean distance in the (part of the) 4-cube of premultiplied
      // colors.
      // FIXME (spec): The CSS transitions spec doesn't say whether
      // colors are premultiplied, but things work better when they are,
      // so use premultiplication.  Spec issue is still open per
      // http://lists.w3.org/Archives/Public/www-style/2009Jul/0050.html
      nscolor startColor = aStartValue.GetColorValue();
      nscolor endColor = aEndValue.GetColorValue();

      // Get a color component on a 0-1 scale, which is much easier to
      // deal with when working with alpha.
      #define GET_COMPONENT(component_, color_) \
        (NS_GET_##component_(color_) * (1.0 / 255.0))

      double startA = GET_COMPONENT(A, startColor);
      double startR = GET_COMPONENT(R, startColor) * startA;
      double startG = GET_COMPONENT(G, startColor) * startA;
      double startB = GET_COMPONENT(B, startColor) * startA;
      double endA = GET_COMPONENT(A, endColor);
      double endR = GET_COMPONENT(R, endColor) * endA;
      double endG = GET_COMPONENT(G, endColor) * endA;
      double endB = GET_COMPONENT(B, endColor) * endA;

      #undef GET_COMPONENT

      double diffA = startA - endA;
      double diffR = startR - endR;
      double diffG = startG - endG;
      double diffB = startB - endB;
      aDistance = sqrt(diffA * diffA + diffR * diffR +
                       diffG * diffG + diffB * diffB);
      break;
    }
    case eUnit_CSSValuePair: {
      const nsCSSValuePair *pair1 = aStartValue.GetCSSValuePairValue();
      const nsCSSValuePair *pair2 = aEndValue.GetCSSValuePairValue();
      if (pair1->mXValue.GetUnit() != pair2->mXValue.GetUnit() ||
          pair1->mYValue.GetUnit() != pair2->mYValue.GetUnit()) {
        // At least until we have calc()
        return PR_FALSE;
      }

      double squareDistance = 0.0;
      static nsCSSValue nsCSSValuePair::* const pairValues[] = {
        &nsCSSValuePair::mXValue, &nsCSSValuePair::mYValue
      };
      for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(pairValues); ++i) {
        nsCSSValue nsCSSValuePair::*member = pairValues[i];
        NS_ABORT_IF_FALSE((pair1->*member).GetUnit() ==
                            (pair2->*member).GetUnit(),
                          "should have returned above");
        double diff;
        switch ((pair1->*member).GetUnit()) {
          case eCSSUnit_Pixel:
            diff = (pair1->*member).GetFloatValue() -
                   (pair2->*member).GetFloatValue();
            break;
          case eCSSUnit_Percent:
            diff = (pair1->*member).GetPercentValue() -
                   (pair2->*member).GetPercentValue();
            break;
          default:
            NS_ABORT_IF_FALSE(PR_FALSE, "unexpected unit");
            return PR_FALSE;
        }
        squareDistance += diff * diff;
      }

      aDistance = sqrt(squareDistance);
      break;
    }
    case eUnit_Dasharray: {
      // NOTE: This produces results on substantially different scales
      // for length values and percentage values, which might even be
      // mixed in the same property value.  This means the result isn't
      // particularly useful for paced animation.

      // Call AddWeighted to make us lists of the same length.
      Value normValue1, normValue2;
      if (!AddWeighted(1.0, aStartValue, 0.0, aEndValue, normValue1) ||
          !AddWeighted(0.0, aStartValue, 1.0, aEndValue, normValue2)) {
        success = PR_FALSE;
        break;
      }

      double squareDistance = 0.0;
      const nsCSSValueList *list1 = normValue1.GetCSSValueListValue();
      const nsCSSValueList *list2 = normValue2.GetCSSValueListValue();

      NS_ABORT_IF_FALSE(!list1 == !list2, "lists should be same length");
      while (list1) {
        const nsCSSValue &val1 = list1->mValue;
        const nsCSSValue &val2 = list2->mValue;

        NS_ABORT_IF_FALSE(val1.GetUnit() == val2.GetUnit(),
                          "unit match should be assured by AddWeighted");
        double diff;
        switch (val1.GetUnit()) {
          case eCSSUnit_Percent:
            diff = val1.GetPercentValue() - val2.GetPercentValue();
            break;
          case eCSSUnit_Number:
            diff = val1.GetFloatValue() - val2.GetFloatValue();
            break;
          default:
            NS_ABORT_IF_FALSE(PR_FALSE, "unexpected unit");
            return PR_FALSE;
        }
        squareDistance += diff * diff;

        list1 = list1->mNext;
        list2 = list2->mNext;
        NS_ABORT_IF_FALSE(!list1 == !list2, "lists should be same length");
      }

      aDistance = sqrt(squareDistance);
      break;
    }
    case eUnit_Shadow: {
      // Call AddWeighted to make us lists of the same length.
      Value normValue1, normValue2;
      if (!AddWeighted(1.0, aStartValue, 0.0, aEndValue, normValue1) ||
          !AddWeighted(0.0, aStartValue, 1.0, aEndValue, normValue2)) {
        success = PR_FALSE;
        break;
      }

      const nsCSSValueList *shadow1 = normValue1.GetCSSValueListValue();
      const nsCSSValueList *shadow2 = normValue2.GetCSSValueListValue();

      double squareDistance = 0.0f;
      NS_ABORT_IF_FALSE(!shadow1 == !shadow2, "lists should be same length");
      while (shadow1) {
        nsCSSValue::Array *array1 = shadow1->mValue.GetArrayValue();
        nsCSSValue::Array *array2 = shadow2->mValue.GetArrayValue();
        for (PRUint32 i = 0; i < 4; ++i) {
          NS_ABORT_IF_FALSE(array1->Item(i).GetUnit() == eCSSUnit_Pixel,
                            "unexpected unit");
          NS_ABORT_IF_FALSE(array2->Item(i).GetUnit() == eCSSUnit_Pixel,
                            "unexpected unit");
          double diff = array1->Item(i).GetFloatValue() -
                        array2->Item(i).GetFloatValue();
          squareDistance += diff * diff;
        }

        const nsCSSValue &color1 = array1->Item(4);
        const nsCSSValue &color2 = array2->Item(4);
        const nsCSSValue &inset1 = array1->Item(5);
        const nsCSSValue &inset2 = array2->Item(5);
        // There are only two possible states of the inset value:
        //  (1) GetUnit() == eCSSUnit_Null
        //  (2) GetUnit() == eCSSUnit_Enumerated &&
        //      GetIntValue() == NS_STYLE_BOX_SHADOW_INSET
        NS_ABORT_IF_FALSE(color1.GetUnit() == color2.GetUnit() &&
                          inset1 == inset2,
                          "AddWeighted should have failed");

        if (color1.GetUnit() != eCSSUnit_Null) {
          nsStyleAnimation::Value color1Value
            (color1.GetColorValue(), nsStyleAnimation::Value::ColorConstructor);
          nsStyleAnimation::Value color2Value
            (color2.GetColorValue(), nsStyleAnimation::Value::ColorConstructor);
          double colorDistance;

        #ifdef DEBUG
          PRBool ok =
        #endif
            nsStyleAnimation::ComputeDistance(color1Value, color2Value,
                                              colorDistance);
          NS_ABORT_IF_FALSE(ok, "should not fail");
          squareDistance += colorDistance * colorDistance;
        }

        shadow1 = shadow1->mNext;
        shadow2 = shadow2->mNext;
        NS_ABORT_IF_FALSE(!shadow1 == !shadow2, "lists should be same length");
      }
      aDistance = sqrt(squareDistance);
      break;
    }
    default:
      NS_NOTREACHED("Can't compute distance using the given common unit");
      success = PR_FALSE;
      break;
  }
  return success;
}

#define MAX_PACKED_COLOR_COMPONENT 255

inline PRUint8 ClampColor(double aColor)
{
  if (aColor >= MAX_PACKED_COLOR_COMPONENT)
    return MAX_PACKED_COLOR_COMPONENT;
  if (aColor <= 0.0)
    return 0;
  return NSToIntRound(aColor);
}

static PRBool
AddShadowItems(double aCoeff1, const nsCSSValue &aValue1,
               double aCoeff2, const nsCSSValue &aValue2,
               nsCSSValueList **&aResultTail)
{
  // X, Y, Radius, Spread, Color, Inset
  NS_ABORT_IF_FALSE(aValue1.GetUnit() == eCSSUnit_Array,
                    "wrong unit");
  NS_ABORT_IF_FALSE(aValue2.GetUnit() == eCSSUnit_Array,
                    "wrong unit");
  nsCSSValue::Array *array1 = aValue1.GetArrayValue();
  nsCSSValue::Array *array2 = aValue2.GetArrayValue();
  nsRefPtr<nsCSSValue::Array> resultArray = nsCSSValue::Array::Create(6);
  if (!resultArray) {
    return PR_FALSE;
  }

  for (PRUint32 i = 0; i < 4; ++i) {
    NS_ABORT_IF_FALSE(array1->Item(i).GetUnit() == eCSSUnit_Pixel,
                      "unexpected unit");
    NS_ABORT_IF_FALSE(array2->Item(i).GetUnit() == eCSSUnit_Pixel,
                      "unexpected unit");
    double pixel1 = array1->Item(i).GetFloatValue();
    double pixel2 = array2->Item(i).GetFloatValue();
    resultArray->Item(i).SetFloatValue(aCoeff1 * pixel1 + aCoeff2 * pixel2,
                                       eCSSUnit_Pixel);
  }

  const nsCSSValue& color1 = array1->Item(4);
  const nsCSSValue& color2 = array2->Item(4);
  const nsCSSValue& inset1 = array1->Item(5);
  const nsCSSValue& inset2 = array2->Item(5);
  if (color1.GetUnit() != color2.GetUnit() ||
      inset1.GetUnit() != inset2.GetUnit()) {
    // We don't know how to animate between color and no-color, or
    // between inset and not-inset.
    return PR_FALSE;
  }

  if (color1.GetUnit() != eCSSUnit_Null) {
    nsStyleAnimation::Value color1Value
      (color1.GetColorValue(), nsStyleAnimation::Value::ColorConstructor);
    nsStyleAnimation::Value color2Value
      (color2.GetColorValue(), nsStyleAnimation::Value::ColorConstructor);
    nsStyleAnimation::Value resultColorValue;
  #ifdef DEBUG
    PRBool ok =
  #endif
      nsStyleAnimation::AddWeighted(aCoeff1, color1Value, aCoeff2, color2Value,
                                    resultColorValue);
    NS_ABORT_IF_FALSE(ok, "should not fail");
    resultArray->Item(4).SetColorValue(resultColorValue.GetColorValue());
  }

  NS_ABORT_IF_FALSE(inset1 == inset2, "should match");
  resultArray->Item(5) = inset1;

  nsCSSValueList *resultItem = new nsCSSValueList;
  if (!resultItem) {
    return PR_FALSE;
  }
  resultItem->mValue.SetArrayValue(resultArray, eCSSUnit_Array);
  *aResultTail = resultItem;
  aResultTail = &resultItem->mNext;
  return PR_TRUE;
}

PRBool
nsStyleAnimation::AddWeighted(double aCoeff1, const Value& aValue1,
                              double aCoeff2, const Value& aValue2,
                              Value& aResultValue)
{
  Unit commonUnit = GetCommonUnit(aValue1.GetUnit(), aValue2.GetUnit());
  // Maybe need a followup method to convert the inputs into the common
  // unit-type, if they don't already match it. (Or would it make sense to do
  // that in GetCommonUnit? in which case maybe ConvertToCommonUnit would be
  // better.)

  PRBool success = PR_TRUE;
  switch (commonUnit) {
    case eUnit_Null:
    case eUnit_None:
    case eUnit_Enumerated:
      success = PR_FALSE;
      break;
    case eUnit_Integer: {
      // http://dev.w3.org/csswg/css3-transitions/#animation-of-property-types-
      // says we should use floor
      aResultValue.SetIntValue(NS_floor(
          aCoeff1 * double(aValue1.GetIntValue()) +
          aCoeff2 * double(aValue2.GetIntValue())),
        eUnit_Integer);
      break;
    }
    case eUnit_Coord: {
      aResultValue.SetCoordValue(NSToCoordRound(
        aCoeff1 * aValue1.GetCoordValue() +
        aCoeff2 * aValue2.GetCoordValue()));
      break;
    }
    case eUnit_Percent: {
      aResultValue.SetPercentValue(
        aCoeff1 * aValue1.GetPercentValue() +
        aCoeff2 * aValue2.GetPercentValue());
      break;
    }
    case eUnit_Float: {
      aResultValue.SetFloatValue(
        aCoeff1 * aValue1.GetFloatValue() +
        aCoeff2 * aValue2.GetFloatValue());
      break;
    }
    case eUnit_Color: {
      nscolor color1 = aValue1.GetColorValue();
      nscolor color2 = aValue2.GetColorValue();
      // FIXME (spec): The CSS transitions spec doesn't say whether
      // colors are premultiplied, but things work better when they are,
      // so use premultiplication.  Spec issue is still open per
      // http://lists.w3.org/Archives/Public/www-style/2009Jul/0050.html

      // To save some math, scale the alpha down to a 0-1 scale, but
      // leave the color components on a 0-255 scale.
      double A1 = NS_GET_A(color1) * (1.0 / 255.0);
      double R1 = NS_GET_R(color1) * A1;
      double G1 = NS_GET_G(color1) * A1;
      double B1 = NS_GET_B(color1) * A1;
      double A2 = NS_GET_A(color2) * (1.0 / 255.0);
      double R2 = NS_GET_R(color2) * A2;
      double G2 = NS_GET_G(color2) * A2;
      double B2 = NS_GET_B(color2) * A2;
      double Aresf = (A1 * aCoeff1 + A2 * aCoeff2);
      nscolor resultColor;
      if (Aresf <= 0.0) {
        resultColor = NS_RGBA(0, 0, 0, 0);
      } else {
        if (Aresf > 1.0) {
          Aresf = 1.0;
        }
        double factor = 1.0 / Aresf;
        PRUint8 Ares = NSToIntRound(Aresf * 255.0);
        PRUint8 Rres = ClampColor((R1 * aCoeff1 + R2 * aCoeff2) * factor);
        PRUint8 Gres = ClampColor((G1 * aCoeff1 + G2 * aCoeff2) * factor);
        PRUint8 Bres = ClampColor((B1 * aCoeff1 + B2 * aCoeff2) * factor);
        resultColor = NS_RGBA(Rres, Gres, Bres, Ares);
      }
      aResultValue.SetColorValue(resultColor);
      break;
    }
    case eUnit_CSSValuePair: {
      const nsCSSValuePair *pair1 = aValue1.GetCSSValuePairValue();
      const nsCSSValuePair *pair2 = aValue2.GetCSSValuePairValue();
      if (pair1->mXValue.GetUnit() != pair2->mXValue.GetUnit() ||
          pair1->mYValue.GetUnit() != pair2->mYValue.GetUnit()) {
        // At least until we have calc()
        return PR_FALSE;
      }

      nsAutoPtr<nsCSSValuePair> result(new nsCSSValuePair);
      if (!result) {
        return PR_FALSE;
      }
      static nsCSSValue nsCSSValuePair::* const pairValues[] = {
        &nsCSSValuePair::mXValue, &nsCSSValuePair::mYValue
      };
      for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(pairValues); ++i) {
        nsCSSValue nsCSSValuePair::*member = pairValues[i];
        NS_ABORT_IF_FALSE((pair1->*member).GetUnit() ==
                            (pair2->*member).GetUnit(),
                          "should have returned above");
        switch ((pair1->*member).GetUnit()) {
          case eCSSUnit_Pixel:
            (result->*member).SetFloatValue(
              aCoeff1 * (pair1->*member).GetFloatValue() +
              aCoeff2 * (pair2->*member).GetFloatValue(),
              eCSSUnit_Pixel);
            break;
          case eCSSUnit_Percent:
            (result->*member).SetPercentValue(
              aCoeff1 * (pair1->*member).GetPercentValue() +
              aCoeff2 * (pair2->*member).GetPercentValue());
            break;
          default:
            NS_ABORT_IF_FALSE(PR_FALSE, "unexpected unit");
            return PR_FALSE;
        }
      }

      aResultValue.SetAndAdoptCSSValuePairValue(result.forget(),
                                                eUnit_CSSValuePair);
      break;
    }
    case eUnit_Dasharray: {
      const nsCSSValueList *list1 = aValue1.GetCSSValueListValue();
      const nsCSSValueList *list2 = aValue2.GetCSSValueListValue();

      PRUint32 len1 = 0, len2 = 0;
      for (const nsCSSValueList *v = list1; v; v = v->mNext) {
        ++len1;
      }
      for (const nsCSSValueList *v = list2; v; v = v->mNext) {
        ++len2;
      }
      NS_ABORT_IF_FALSE(len1 > 0 && len2 > 0, "unexpected length");

      nsAutoPtr<nsCSSValueList> result;
      nsCSSValueList **resultTail = getter_Transfers(result);
      for (PRUint32 i = 0, i_end = lcm(len1, len2); i != i_end; ++i) {
        const nsCSSValue &v1 = list1->mValue;
        const nsCSSValue &v2 = list2->mValue;
        NS_ABORT_IF_FALSE(v1.GetUnit() == eCSSUnit_Number ||
                          v1.GetUnit() == eCSSUnit_Percent, "unexpected");
        NS_ABORT_IF_FALSE(v2.GetUnit() == eCSSUnit_Number ||
                          v2.GetUnit() == eCSSUnit_Percent, "unexpected");
        if (v1.GetUnit() != v2.GetUnit()) {
          // Can't animate between lengths and percentages (until calc()).
          return PR_FALSE;
        }

        nsCSSValueList *item = new nsCSSValueList;
        if (!item) {
          return PR_FALSE;
        }
        *resultTail = item;
        resultTail = &item->mNext;

        if (v1.GetUnit() == eCSSUnit_Number) {
          item->mValue.SetFloatValue(aCoeff1 * v1.GetFloatValue() +
                                     aCoeff2 * v2.GetFloatValue(),
                                     eCSSUnit_Number);
        } else {
          NS_ABORT_IF_FALSE(v1.GetUnit() == eCSSUnit_Percent, "unexpected");
          item->mValue.SetPercentValue(aCoeff1 * v1.GetPercentValue() +
                                       aCoeff2 * v2.GetPercentValue());
        }

        list1 = list1->mNext;
        if (!list1) {
          list1 = aValue1.GetCSSValueListValue();
        }
        list2 = list2->mNext;
        if (!list2) {
          list2 = aValue2.GetCSSValueListValue();
        }
      }

      aResultValue.SetAndAdoptCSSValueListValue(result.forget(),
                                                eUnit_Dasharray);
      break;
    }
    case eUnit_Shadow: {
      // This is implemented according to:
      // http://dev.w3.org/csswg/css3-transitions/#animation-of-property-types-
      // and the third item in the summary of:
      // http://lists.w3.org/Archives/Public/www-style/2009Jul/0050.html
      const nsCSSValueList *shadow1 = aValue1.GetCSSValueListValue();
      const nsCSSValueList *shadow2 = aValue2.GetCSSValueListValue();
      nsAutoPtr<nsCSSValueList> result;
      nsCSSValueList **resultTail = getter_Transfers(result);
      while (shadow1 && shadow2) {
        if (!AddShadowItems(aCoeff1, shadow1->mValue,
                            aCoeff2, shadow2->mValue,
                            resultTail)) {
          return PR_FALSE;
        }
        shadow1 = shadow1->mNext;
        shadow2 = shadow2->mNext;
      }
      if (shadow1 || shadow2) {
        const nsCSSValueList *longShadow;
        double longCoeff;
        if (shadow1) {
          longShadow = shadow1;
          longCoeff = aCoeff1;
        } else {
          longShadow = shadow2;
          longCoeff = aCoeff2;
        }

        while (longShadow) {
          // Passing coefficients that add to less than 1 produces the
          // desired result of interpolating "0 0 0 transparent" with
          // the current shadow.
          if (!AddShadowItems(longCoeff, longShadow->mValue,
                              0.0, longShadow->mValue,
                              resultTail)) {
            return PR_FALSE;
            break;
          }

          longShadow = longShadow->mNext;
        }
      }
      aResultValue.SetAndAdoptCSSValueListValue(result.forget(), eUnit_Shadow);
      break;
    }
    default:
      NS_NOTREACHED("Can't interpolate using the given common unit");
      success = PR_FALSE;
      break;
  }
  return success;
}

already_AddRefed<nsICSSStyleRule>
BuildStyleRule(nsCSSProperty aProperty,
               nsIContent* aTargetElement,
               const nsAString& aSpecifiedValue)
{
  // Set up an empty CSS Declaration
  nsCSSDeclaration* declaration = new nsCSSDeclaration();
  if (!declaration) {
    NS_WARNING("Failed to allocate nsCSSDeclaration");
    return nsnull;
  }

  PRBool changed; // ignored, but needed as outparam for ParseProperty
  nsIDocument* doc = aTargetElement->GetOwnerDoc();
  nsCOMPtr<nsIURI> baseURI = aTargetElement->GetBaseURI();
  nsCOMPtr<nsICSSParser> parser;
  nsCOMPtr<nsICSSStyleRule> styleRule;

  // The next statement performs the following, in sequence: Get parser, use
  // parser to parse property, check that parsing succeeded, and build a rule
  // for the resulting declaration.  If any of these steps fails, we bail out
  // and delete the declaration.
  if (!declaration->InitializeEmpty() ||
      NS_FAILED(doc->CSSLoader()->GetParserFor(nsnull,
                                               getter_AddRefs(parser))) ||
      NS_FAILED(parser->ParseProperty(aProperty, aSpecifiedValue,
                                      doc->GetDocumentURI(), baseURI,
                                      aTargetElement->NodePrincipal(),
                                      declaration, &changed)) ||
      // SlotForValue checks whether property parsed w/out CSS parsing errors
      !declaration->SlotForValue(aProperty) ||
      NS_FAILED(NS_NewCSSStyleRule(getter_AddRefs(styleRule), nsnull,
                                   declaration))) {
    NS_WARNING("failure in BuildStyleRule");
    declaration->RuleAbort();  // deletes declaration
    return nsnull;
  }

  return styleRule.forget();
}

inline
already_AddRefed<nsStyleContext>
LookupStyleContext(nsIContent* aElement)
{
  nsIDocument* doc = aElement->GetCurrentDoc();
  nsIPresShell* shell = doc->GetPrimaryShell();
  if (!shell) {
    return nsnull;
  }
  return nsComputedDOMStyle::GetStyleContextForContent(aElement, nsnull, shell);
}

PRBool
nsStyleAnimation::ComputeValue(nsCSSProperty aProperty,
                               nsIContent* aTargetElement,
                               const nsAString& aSpecifiedValue,
                               Value& aComputedValue)
{
  NS_ABORT_IF_FALSE(aTargetElement, "null target element");
  NS_ABORT_IF_FALSE(aTargetElement->GetCurrentDoc(),
                    "we should only be able to actively animate nodes that "
                    "are in a document");

  // Look up style context for our target element
  nsRefPtr<nsStyleContext> styleContext = LookupStyleContext(aTargetElement);
  if (!styleContext) {
    return PR_FALSE;
  }

  // Parse specified value into a temporary nsICSSStyleRule
  nsCOMPtr<nsICSSStyleRule> styleRule =
    BuildStyleRule(aProperty, aTargetElement, aSpecifiedValue);
  if (!styleRule) {
    return PR_FALSE;
  }

  // Create a temporary nsStyleContext for the style rule
  nsCOMArray<nsIStyleRule> ruleArray;
  ruleArray.AppendObject(styleRule);
  nsStyleSet* styleSet = styleContext->PresContext()->StyleSet();
  nsRefPtr<nsStyleContext> tmpStyleContext =
    styleSet->ResolveStyleForRules(styleContext->GetParent(),
                                   styleContext->GetPseudo(),
                                   styleContext->GetRuleNode(), ruleArray);

  // Extract computed value of our property from the temporary style rule
  return ExtractComputedValue(aProperty, tmpStyleContext, aComputedValue);
}

PRBool
nsStyleAnimation::UncomputeValue(nsCSSProperty aProperty,
                                 nsPresContext* aPresContext,
                                 const Value& aComputedValue,
                                 void* aSpecifiedValue)
{
  NS_ABORT_IF_FALSE(aPresContext, "null pres context");

  switch (aComputedValue.GetUnit()) {
    case eUnit_None:
      if (nsCSSProps::kAnimTypeTable[aProperty] == eStyleAnimType_PaintServer) {
        NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                            eCSSType_ValuePair, "type mismatch");
        static_cast<nsCSSValuePair*>(aSpecifiedValue)->
          SetBothValuesTo(nsCSSValue(eCSSUnit_None));
      } else {
        NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                          "type mismatch");
        static_cast<nsCSSValue*>(aSpecifiedValue)->SetNoneValue();
      }
      break;
    case eUnit_Enumerated:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetIntValue(aComputedValue.GetIntValue(), eCSSUnit_Enumerated);
      break;
    case eUnit_Integer:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetIntValue(aComputedValue.GetIntValue(), eCSSUnit_Integer);
      break;
    case eUnit_Coord: {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      float pxVal = aPresContext->AppUnitsToFloatCSSPixels(
                                    aComputedValue.GetCoordValue());
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetFloatValue(pxVal, eCSSUnit_Pixel);
      break;
    }
    case eUnit_Percent:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetPercentValue(aComputedValue.GetPercentValue());
      break;
    case eUnit_Float:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetFloatValue(aComputedValue.GetFloatValue(), eCSSUnit_Number);
      break;
    case eUnit_Color:
      // colors can be alone, or part of a paint server
      if (nsCSSProps::kAnimTypeTable[aProperty] == eStyleAnimType_PaintServer) {
        NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                            eCSSType_ValuePair, "type mismatch");
        nsCSSValue val;
        val.SetColorValue(aComputedValue.GetColorValue());
        static_cast<nsCSSValuePair*>(aSpecifiedValue)->
          SetBothValuesTo(val);
      } else {
        NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                          "type mismatch");
        static_cast<nsCSSValue*>(aSpecifiedValue)->
          SetColorValue(aComputedValue.GetColorValue());
      }
      break;
    case eUnit_CSSValuePair:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValuePair, "type mismatch");
      *static_cast<nsCSSValuePair*>(aSpecifiedValue) =
        *aComputedValue.GetCSSValuePairValue();
      break;
    case eUnit_Dasharray:
    case eUnit_Shadow:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValueList, "type mismatch");
      *static_cast<nsCSSValueList**>(aSpecifiedValue) =
        aComputedValue.GetCSSValueListValue();
      break;
    default:
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsStyleAnimation::UncomputeValue(nsCSSProperty aProperty,
                                 nsPresContext* aPresContext,
                                 const Value& aComputedValue,
                                 nsAString& aSpecifiedValue)
{
  NS_ABORT_IF_FALSE(aPresContext, "null pres context");
  aSpecifiedValue.Truncate(); // Clear outparam, if it's not already empty

  nsCSSValuePair vp;
  nsCSSRect rect;
  void *ptr = nsnull;
  void *storage;
  switch (nsCSSProps::kTypeTable[aProperty]) {
    case eCSSType_Value:
      storage = &vp.mXValue;
      break;
    case eCSSType_Rect:
      storage = &rect;
      break;
    case eCSSType_ValuePair: 
      storage = &vp;
      break;
    case eCSSType_ValueList:
    case eCSSType_ValuePairList:
      storage = &ptr;
      break;
    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "unexpected case");
      storage = nsnull;
      break;
  }

  nsCSSValue value;
  if (!nsStyleAnimation::UncomputeValue(aProperty, aPresContext,
                                        aComputedValue, storage)) {
    return PR_FALSE;
  }
  return nsCSSDeclaration::AppendStorageToString(aProperty, storage,
                                                 aSpecifiedValue);
}

inline const void*
StyleDataAtOffset(const void* aStyleStruct, ptrdiff_t aOffset)
{
  return reinterpret_cast<const char*>(aStyleStruct) + aOffset;
}

inline void*
StyleDataAtOffset(void* aStyleStruct, ptrdiff_t aOffset)
{
  return reinterpret_cast<char*>(aStyleStruct) + aOffset;
}

static void
ExtractBorderColor(nsStyleContext* aStyleContext, const void* aStyleBorder,
                   PRUint8 aSide, nsStyleAnimation::Value& aComputedValue)
{
  nscolor color; 
  PRBool foreground;
  static_cast<const nsStyleBorder*>(aStyleBorder)->
    GetBorderColor(aSide, color, foreground);
  if (foreground) {
    // FIXME: should add test for this
    color = aStyleContext->GetStyleColor()->mColor;
  }
  aComputedValue.SetColorValue(color);
}

static PRBool
StyleCoordToValue(const nsStyleCoord& aCoord, nsStyleAnimation::Value& aValue)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Null:
      return PR_FALSE;
    case eStyleUnit_Normal:
      aValue.SetNormalValue();
      break;
    case eStyleUnit_Auto:
      aValue.SetAutoValue();
      break;
    case eStyleUnit_None:
      aValue.SetNoneValue();
      break;
    case eStyleUnit_Percent:
      aValue.SetPercentValue(aCoord.GetPercentValue());
      break;
    case eStyleUnit_Factor:
      aValue.SetFloatValue(aCoord.GetFactorValue());
      break;
    case eStyleUnit_Coord:
      aValue.SetCoordValue(aCoord.GetCoordValue());
      break;
    case eStyleUnit_Enumerated:
      aValue.SetIntValue(aCoord.GetIntValue(),
                         nsStyleAnimation::eUnit_Enumerated);
      break;
    case eStyleUnit_Integer:
      aValue.SetIntValue(aCoord.GetIntValue(),
                         nsStyleAnimation::eUnit_Integer);
      break;
  }
  return PR_TRUE;
}

static void
StyleCoordToCSSValue(const nsStyleCoord& aCoord, nsCSSValue& aCSSValue)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Coord:
      aCSSValue.SetFloatValue(nsPresContext::AppUnitsToFloatCSSPixels(
                                aCoord.GetCoordValue()),
                              eCSSUnit_Pixel);
      break;
    case eStyleUnit_Percent:
      aCSSValue.SetPercentValue(aCoord.GetPercentValue());
      break;
    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "unexpected unit");
  }
}

PRBool
nsStyleAnimation::ExtractComputedValue(nsCSSProperty aProperty,
                                       nsStyleContext* aStyleContext,
                                       Value& aComputedValue)
{
  NS_ABORT_IF_FALSE(0 <= aProperty &&
                    aProperty < eCSSProperty_COUNT_no_shorthands,
                    "bad property");
  const void* styleStruct =
    aStyleContext->GetStyleData(nsCSSProps::kSIDTable[aProperty]);
  ptrdiff_t ssOffset = nsCSSProps::kStyleStructOffsetTable[aProperty];
  nsStyleAnimType animType = nsCSSProps::kAnimTypeTable[aProperty];
  NS_ABORT_IF_FALSE(0 <= ssOffset || animType == eStyleAnimType_Custom,
                    "must be dealing with animatable property");
  switch (animType) {
    case eStyleAnimType_Custom:
      switch (aProperty) {
        // For border-width, ignore the border-image business (which
        // only exists until we update our implementation to the current
        // spec) and use GetComputedBorder
        
        #define BORDER_WIDTH_CASE(prop_, side_)                               \
        case prop_:                                                           \
          aComputedValue.SetCoordValue(                                       \
            static_cast<const nsStyleBorder*>(styleStruct)->                  \
              GetComputedBorder().side_);                                     \
          break;
        BORDER_WIDTH_CASE(eCSSProperty_border_bottom_width, bottom)
        BORDER_WIDTH_CASE(eCSSProperty_border_left_width_value, left)
        BORDER_WIDTH_CASE(eCSSProperty_border_right_width_value, right)
        BORDER_WIDTH_CASE(eCSSProperty_border_top_width, top)
        #undef BORDER_WIDTH_CASE

        case eCSSProperty_border_bottom_color:
          ExtractBorderColor(aStyleContext, styleStruct, NS_SIDE_BOTTOM,
                             aComputedValue);
          break;
        case eCSSProperty_border_left_color_value:
          ExtractBorderColor(aStyleContext, styleStruct, NS_SIDE_LEFT,
                             aComputedValue);
          break;
        case eCSSProperty_border_right_color_value:
          ExtractBorderColor(aStyleContext, styleStruct, NS_SIDE_RIGHT,
                             aComputedValue);
          break;
        case eCSSProperty_border_top_color:
          ExtractBorderColor(aStyleContext, styleStruct, NS_SIDE_TOP,
                             aComputedValue);
          break;

        case eCSSProperty_outline_color: {
          const nsStyleOutline *styleOutline =
            static_cast<const nsStyleOutline*>(styleStruct);
          nscolor color;
        #ifdef GFX_HAS_INVERT
          styleOutline->GetOutlineColor(color);
        #else
          if (!styleOutline->GetOutlineColor(color))
            color = aStyleContext->GetStyleColor()->mColor;
        #endif
          aComputedValue.SetColorValue(color);
          break;
        }

        case eCSSProperty_stroke_dasharray: {
          const nsStyleSVG *svg = static_cast<const nsStyleSVG*>(styleStruct);
          NS_ABORT_IF_FALSE((svg->mStrokeDasharray != nsnull) ==
                            (svg->mStrokeDasharrayLength != 0),
                            "pointer/length mismatch");
          if (svg->mStrokeDasharray) {
            nsAutoPtr<nsCSSValueList> result;
            nsCSSValueList **resultTail = getter_Transfers(result);
            for (PRUint32 i = 0, i_end = svg->mStrokeDasharrayLength;
                 i != i_end; ++i) {
              nsCSSValueList *item = new nsCSSValueList;
              if (!item) {
                return PR_FALSE;
              }
              *resultTail = item;
              resultTail = &item->mNext;

              const nsStyleCoord &coord = svg->mStrokeDasharray[i];
              nsCSSValue &value = item->mValue;
              switch (coord.GetUnit()) {
                case eStyleUnit_Coord:
                  // Number means the same thing as length; we want to
                  // animate them the same way.  Normalize both to number
                  // since it has more accuracy (float vs nscoord).
                  value.SetFloatValue(nsPresContext::
                    AppUnitsToFloatCSSPixels(coord.GetCoordValue()),
                    eCSSUnit_Number);
                  break;
                case eStyleUnit_Factor:
                  value.SetFloatValue(coord.GetFactorValue(),
                                      eCSSUnit_Number);
                  break;
                case eStyleUnit_Percent:
                  value.SetPercentValue(coord.GetPercentValue());
                  break;
                default:
                  NS_ABORT_IF_FALSE(PR_FALSE, "unexpected unit");
                  return PR_FALSE;
              }
            }
            aComputedValue.SetAndAdoptCSSValueListValue(result.forget(),
                                                        eUnit_Dasharray);
          } else {
            aComputedValue.SetNoneValue();
          }
          break;
        }

        default:
          NS_ABORT_IF_FALSE(PR_FALSE, "missing property implementation");
          return PR_FALSE;
      };
      return PR_TRUE;
    case eStyleAnimType_Coord:
      return StyleCoordToValue(*static_cast<const nsStyleCoord*>(
        StyleDataAtOffset(styleStruct, ssOffset)), aComputedValue);
    case eStyleAnimType_Sides_Top:
    case eStyleAnimType_Sides_Right:
    case eStyleAnimType_Sides_Bottom:
    case eStyleAnimType_Sides_Left: {
      PR_STATIC_ASSERT(0 == NS_SIDE_TOP);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Right - eStyleAnimType_Sides_Top
                         == NS_SIDE_RIGHT);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Bottom - eStyleAnimType_Sides_Top
                         == NS_SIDE_BOTTOM);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Left - eStyleAnimType_Sides_Top
                         == NS_SIDE_LEFT);
      const nsStyleCoord &coord = static_cast<const nsStyleSides*>(
        StyleDataAtOffset(styleStruct, ssOffset))->
          Get(animType - eStyleAnimType_Sides_Top);
      return StyleCoordToValue(coord, aComputedValue);
    }
    case eStyleAnimType_Corner_TopLeft:
    case eStyleAnimType_Corner_TopRight:
    case eStyleAnimType_Corner_BottomRight:
    case eStyleAnimType_Corner_BottomLeft: {
      PR_STATIC_ASSERT(0 == NS_CORNER_TOP_LEFT);
      PR_STATIC_ASSERT(eStyleAnimType_Corner_TopRight -
                         eStyleAnimType_Corner_TopLeft
                       == NS_CORNER_TOP_RIGHT);
      PR_STATIC_ASSERT(eStyleAnimType_Corner_BottomRight -
                         eStyleAnimType_Corner_TopLeft
                       == NS_CORNER_BOTTOM_RIGHT);
      PR_STATIC_ASSERT(eStyleAnimType_Corner_BottomLeft -
                         eStyleAnimType_Corner_TopLeft
                       == NS_CORNER_BOTTOM_LEFT);
      const nsStyleCorners *corners = static_cast<const nsStyleCorners*>(
        StyleDataAtOffset(styleStruct, ssOffset));
      PRUint8 fullCorner = animType - eStyleAnimType_Corner_TopLeft;
      const nsStyleCoord &horiz =
        corners->Get(NS_FULL_TO_HALF_CORNER(fullCorner, PR_FALSE));
      const nsStyleCoord &vert =
        corners->Get(NS_FULL_TO_HALF_CORNER(fullCorner, PR_TRUE));
      nsCSSValuePair *pair = new nsCSSValuePair;
      if (!pair) {
        return PR_FALSE;
      }
      StyleCoordToCSSValue(horiz, pair->mXValue);
      StyleCoordToCSSValue(vert, pair->mYValue);
      aComputedValue.SetAndAdoptCSSValuePairValue(pair, eUnit_CSSValuePair);
      return PR_TRUE;
    }
    case eStyleAnimType_nscoord:
      aComputedValue.SetCoordValue(*static_cast<const nscoord*>(
        StyleDataAtOffset(styleStruct, ssOffset)));
      return PR_TRUE;
    case eStyleAnimType_EnumU8:
      aComputedValue.SetIntValue(*static_cast<const PRUint8*>(
        StyleDataAtOffset(styleStruct, ssOffset)), eUnit_Enumerated);
      return PR_TRUE;
    case eStyleAnimType_float:
      aComputedValue.SetFloatValue(*static_cast<const float*>(
        StyleDataAtOffset(styleStruct, ssOffset)));
      if (aProperty == eCSSProperty_font_size_adjust &&
          aComputedValue.GetFloatValue() == 0.0f) {
        // In nsStyleFont, we set mFont.sizeAdjust to 0 to represent
        // font-size-adjust: none.  Here, we have to treat this as a keyword
        // instead of a float value, to make sure we don't end up doing
        // interpolation with it.
        aComputedValue.SetNoneValue();
      }
      return PR_TRUE;
    case eStyleAnimType_Color:
      aComputedValue.SetColorValue(*static_cast<const nscolor*>(
        StyleDataAtOffset(styleStruct, ssOffset)));
      return PR_TRUE;
    case eStyleAnimType_PaintServer: {
      const nsStyleSVGPaint &paint = *static_cast<const nsStyleSVGPaint*>(
        StyleDataAtOffset(styleStruct, ssOffset));
      // FIXME: At some point in the future, we should animate gradients.
      if (paint.mType == eStyleSVGPaintType_Color) {
        aComputedValue.SetColorValue(paint.mPaint.mColor);
        return PR_TRUE;
      }
      if (paint.mType == eStyleSVGPaintType_None) {
        aComputedValue.SetNoneValue();
        return PR_TRUE;
      }
      return PR_FALSE;
    }
    case eStyleAnimType_Shadow: {
      const nsCSSShadowArray *shadowArray =
        *static_cast<const nsRefPtr<nsCSSShadowArray>*>(
          StyleDataAtOffset(styleStruct, ssOffset));
      if (!shadowArray) {
        aComputedValue.SetAndAdoptCSSValueListValue(nsnull, eUnit_Shadow);
        return PR_TRUE;
      }
      nsAutoPtr<nsCSSValueList> result;
      nsCSSValueList **resultTail = getter_Transfers(result);
      for (PRUint32 i = 0, i_end = shadowArray->Length(); i < i_end; ++i) {
        const nsCSSShadowItem *shadow = shadowArray->ShadowAt(i);
        // X, Y, Radius, Spread, Color, Inset
        nsRefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(6);
        arr->Item(0).SetFloatValue(
          nsPresContext::AppUnitsToFloatCSSPixels(shadow->mXOffset),
          eCSSUnit_Pixel);
        arr->Item(1).SetFloatValue(
          nsPresContext::AppUnitsToFloatCSSPixels(shadow->mYOffset),
          eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(
          nsPresContext::AppUnitsToFloatCSSPixels(shadow->mRadius),
          eCSSUnit_Pixel);
        // NOTE: This code sometimes stores mSpread: 0 even when
        // the parser would be required to leave it null.
        arr->Item(3).SetFloatValue(
          nsPresContext::AppUnitsToFloatCSSPixels(shadow->mSpread),
          eCSSUnit_Pixel);
        if (shadow->mHasColor) {
          arr->Item(4).SetColorValue(shadow->mColor);
        }
        if (shadow->mInset) {
          arr->Item(5).SetIntValue(NS_STYLE_BOX_SHADOW_INSET,
                                   eCSSUnit_Enumerated);
        }

        nsCSSValueList *resultItem = new nsCSSValueList;
        if (!resultItem) {
          return PR_FALSE;
        }
        resultItem->mValue.SetArrayValue(arr, eCSSUnit_Array);
        *resultTail = resultItem;
        resultTail = &resultItem->mNext;
      }
      aComputedValue.SetAndAdoptCSSValueListValue(result.forget(),
                                                  eUnit_Shadow);
      return PR_TRUE;
    }
    case eStyleAnimType_None:
      NS_NOTREACHED("shouldn't use on non-animatable properties");
  }
  return PR_FALSE;
}

nsStyleAnimation::Value::Value(PRInt32 aInt, Unit aUnit,
                               IntegerConstructorType)
{
  NS_ASSERTION(IsIntUnit(aUnit), "unit must be of integer type");
  mUnit = aUnit;
  mValue.mInt = aInt;
}

nsStyleAnimation::Value::Value(nscoord aLength, CoordConstructorType)
{
  mUnit = eUnit_Coord;
  mValue.mCoord = aLength;
}

nsStyleAnimation::Value::Value(float aPercent, PercentConstructorType)
{
  mUnit = eUnit_Percent;
  mValue.mFloat = aPercent;
}

nsStyleAnimation::Value::Value(float aFloat, FloatConstructorType)
{
  mUnit = eUnit_Float;
  mValue.mFloat = aFloat;
}

nsStyleAnimation::Value::Value(nscolor aColor, ColorConstructorType)
{
  mUnit = eUnit_Color;
  mValue.mColor = aColor;
}

nsStyleAnimation::Value&
nsStyleAnimation::Value::operator=(const Value& aOther)
{
  FreeValue();

  mUnit = aOther.mUnit;
  switch (mUnit) {
    case eUnit_Null:
    case eUnit_Normal:
    case eUnit_Auto:
    case eUnit_None:
      break;
    case eUnit_Enumerated:
    case eUnit_Integer:
      mValue.mInt = aOther.mValue.mInt;
      break;
    case eUnit_Coord:
      mValue.mCoord = aOther.mValue.mCoord;
      break;
    case eUnit_Percent:
    case eUnit_Float:
      mValue.mFloat = aOther.mValue.mFloat;
      break;
    case eUnit_Color:
      mValue.mColor = aOther.mValue.mColor;
      break;
    case eUnit_CSSValuePair:
      NS_ABORT_IF_FALSE(aOther.mValue.mCSSValuePair,
                        "value pairs may not be null");
      mValue.mCSSValuePair = new nsCSSValuePair(*aOther.mValue.mCSSValuePair);
      if (!mValue.mCSSValuePair) {
        mUnit = eUnit_Null;
      }
      break;
    case eUnit_Dasharray:
    case eUnit_Shadow:
      NS_ABORT_IF_FALSE(mUnit != eUnit_Dasharray || aOther.mValue.mCSSValueList,
                        "dasharrays may not be null");
      if (aOther.mValue.mCSSValueList) {
        mValue.mCSSValueList = aOther.mValue.mCSSValueList->Clone();
        if (!mValue.mCSSValueList) {
          mUnit = eUnit_Null;
        }
      } else {
        mValue.mCSSValueList = nsnull;
      }
      break;
  }

  return *this;
}

void
nsStyleAnimation::Value::SetNormalValue()
{
  FreeValue();
  mUnit = eUnit_Normal;
}

void
nsStyleAnimation::Value::SetAutoValue()
{
  FreeValue();
  mUnit = eUnit_Auto;
}

void
nsStyleAnimation::Value::SetNoneValue()
{
  FreeValue();
  mUnit = eUnit_None;
}

void
nsStyleAnimation::Value::SetIntValue(PRInt32 aInt, Unit aUnit)
{
  NS_ASSERTION(IsIntUnit(aUnit), "unit must be of integer type");
  FreeValue();
  mUnit = aUnit;
  mValue.mInt = aInt;
}

void
nsStyleAnimation::Value::SetCoordValue(nscoord aLength)
{
  FreeValue();
  mUnit = eUnit_Coord;
  mValue.mCoord = aLength;
}

void
nsStyleAnimation::Value::SetPercentValue(float aPercent)
{
  FreeValue();
  mUnit = eUnit_Percent;
  mValue.mFloat = aPercent;
}

void
nsStyleAnimation::Value::SetFloatValue(float aFloat)
{
  FreeValue();
  mUnit = eUnit_Float;
  mValue.mFloat = aFloat;
}

void
nsStyleAnimation::Value::SetColorValue(nscolor aColor)
{
  FreeValue();
  mUnit = eUnit_Color;
  mValue.mColor = aColor;
}

void
nsStyleAnimation::Value::SetAndAdoptCSSValuePairValue(
                           nsCSSValuePair *aValuePair, Unit aUnit)
{
  FreeValue();
  NS_ABORT_IF_FALSE(IsCSSValuePairUnit(aUnit), "bad unit");
  NS_ABORT_IF_FALSE(aValuePair != nsnull, "value pairs may not be null");
  mUnit = aUnit;
  mValue.mCSSValuePair = aValuePair; // take ownership
}

void
nsStyleAnimation::Value::SetAndAdoptCSSValueListValue(
                           nsCSSValueList *aValueList, Unit aUnit)
{
  FreeValue();
  NS_ABORT_IF_FALSE(IsCSSValueListUnit(aUnit), "bad unit");
  NS_ABORT_IF_FALSE(aUnit != eUnit_Dasharray || aValueList != nsnull,
                    "dasharrays may not be null");
  mUnit = aUnit;
  mValue.mCSSValueList = aValueList; // take ownership
}

void
nsStyleAnimation::Value::FreeValue()
{
  if (IsCSSValueListUnit(mUnit)) {
    delete mValue.mCSSValueList;
  } else if (IsCSSValuePairUnit(mUnit)) {
    delete mValue.mCSSValuePair;
  }
}

PRBool
nsStyleAnimation::Value::operator==(const Value& aOther) const
{
  if (mUnit != aOther.mUnit) {
    return PR_FALSE;
  }

  switch (mUnit) {
    case eUnit_Null:
    case eUnit_Normal:
    case eUnit_Auto:
    case eUnit_None:
      return PR_TRUE;
    case eUnit_Enumerated:
    case eUnit_Integer:
      return mValue.mInt == aOther.mValue.mInt;
    case eUnit_Coord:
      return mValue.mCoord == aOther.mValue.mCoord;
    case eUnit_Percent:
    case eUnit_Float:
      return mValue.mFloat == aOther.mValue.mFloat;
    case eUnit_Color:
      return mValue.mColor == aOther.mValue.mColor;
    case eUnit_CSSValuePair:
      return *mValue.mCSSValuePair == *aOther.mValue.mCSSValuePair;
    case eUnit_Dasharray:
    case eUnit_Shadow:
      return nsCSSValueList::Equal(mValue.mCSSValueList,
                                   aOther.mValue.mCSSValueList);
  }

  NS_NOTREACHED("incomplete case");
  return PR_FALSE;
}

