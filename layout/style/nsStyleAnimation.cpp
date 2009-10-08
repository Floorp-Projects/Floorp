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
#include "nsStyleCoord.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"
#include "nsComputedDOMStyle.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsCSSDataBlock.h"
#include "nsCSSDeclaration.h"
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
 * If there's no sensible common unit, this method returns eStyleUnit_Null.
 *
 * @param   aFirstUnit One unit to resolve.
 * @param   aFirstUnit The other unit to resolve.
 * @return  A "common" unit that both source units can be converted into, or
 *          eStyleUnit_Null if that's not possible.
 */
static
nsStyleUnit
GetCommonUnit(nsStyleUnit aFirstUnit,
              nsStyleUnit aSecondUnit)
{
  // XXXdholbert Naive implementation for now: simply require that the input
  // units match.
  if (aFirstUnit != aSecondUnit) {
    // NOTE: Some unit-pairings will need special handling,
    // e.g. percent vs coord (bug 520234)
    return eStyleUnit_Null;
  }
  return aFirstUnit;
}

// CLASS METHODS
// -------------

#define MAX_PACKED_COLOR_COMPONENT 255

inline PRUint8 ClampColor(PRUint32 aColor)
{
  if (aColor >= MAX_PACKED_COLOR_COMPONENT)
    return MAX_PACKED_COLOR_COMPONENT;
  return aColor;
}

PRBool
nsStyleAnimation::Add(nsStyleCoord& aDest, const nsStyleCoord& aValueToAdd,
                      PRUint32 aCount)
{
  nsStyleUnit commonUnit = GetCommonUnit(aDest.GetUnit(),
                                         aValueToAdd.GetUnit());
  PRBool success = PR_TRUE;
  switch (commonUnit) {
    case eStyleUnit_Coord: {
      nscoord destCoord = aDest.GetCoordValue();
      nscoord valueToAddCoord = aValueToAdd.GetCoordValue();
      destCoord += aCount * valueToAddCoord;
      aDest.SetCoordValue(destCoord);
      break;
    }
    case eStyleUnit_Percent: {
      float destPct = aDest.GetPercentValue();
      float valueToAddPct = aValueToAdd.GetPercentValue();
      destPct += aCount * valueToAddPct;
      aDest.SetPercentValue(destPct);
      break;
    }
    case eStyleUnit_Color: {
      // Since nscolor doesn't allow out-of-sRGB values, by-animations
      // of colors don't make much sense in our implementation.
      // FIXME (bug 515919): Animation of colors should really use
      // floating point colors (and when it does, ClampColor and the
      // clamping of aCount should go away).
      // Also, given RGBA colors, it's not clear whether we want
      // premultiplication.  Probably we don't, given that's hard to
      // premultiply aValueToAdd since it's a difference rather than a
      // value.
      nscolor destColor = aDest.GetColorValue();
      nscolor colorToAdd = aValueToAdd.GetColorValue();
      if (aCount > MAX_PACKED_COLOR_COMPONENT) {
        // Given that we're using integers and clamping at 255, we can
        // clamp aCount to 255 since that's enough to saturate if we're
        // multiplying it by anything nonzero.
        aCount = MAX_PACKED_COLOR_COMPONENT;
      }
      PRUint8 resultR =
        ClampColor(NS_GET_R(destColor) + aCount * NS_GET_R(colorToAdd));
      PRUint8 resultG =
        ClampColor(NS_GET_G(destColor) + aCount * NS_GET_G(colorToAdd));
      PRUint8 resultB =
        ClampColor(NS_GET_B(destColor) + aCount * NS_GET_B(colorToAdd));
      PRUint8 resultA =
        ClampColor(NS_GET_A(destColor) + aCount * NS_GET_A(colorToAdd));
      aDest.SetColorValue(NS_RGBA(resultR, resultG, resultB, resultA));
      break;
    }
    case eStyleUnit_Null:
      success = PR_FALSE;
      break;
    default:
      NS_NOTREACHED("Can't add nsStyleCoords using the given common unit");
      success = PR_FALSE;
      break;
  }
  return success;
}

PRBool
nsStyleAnimation::ComputeDistance(const nsStyleCoord& aStartValue,
                                  const nsStyleCoord& aEndValue,
                                  double& aDistance)
{
  nsStyleUnit commonUnit = GetCommonUnit(aStartValue.GetUnit(),
                                         aEndValue.GetUnit());

  PRBool success = PR_TRUE;
  switch (commonUnit) {
    case eStyleUnit_Coord: {
      nscoord startCoord = aStartValue.GetCoordValue();
      nscoord endCoord = aEndValue.GetCoordValue();
      aDistance = fabs(double(endCoord - startCoord));
      break;
    }
    case eStyleUnit_Percent: {
      float startPct = aStartValue.GetPercentValue();
      float endPct = aEndValue.GetPercentValue();
      aDistance = fabs(double(endPct - startPct));
      break;
    }
    case eStyleUnit_Color: {
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
    case eStyleUnit_Null:
      success = PR_FALSE;
      break;
    default:
      NS_NOTREACHED("Can't compute distance using the given common unit");
      success = PR_FALSE;
      break;
  }
  return success;
}

PRBool
nsStyleAnimation::Interpolate(const nsStyleCoord& aStartValue,
                              const nsStyleCoord& aEndValue,
                              double aPortion,
                              nsStyleCoord& aResultValue)
{
  NS_ABORT_IF_FALSE(aPortion >= 0.0 && aPortion <= 1.0,
                    "aPortion out of bounds");
  nsStyleUnit commonUnit = GetCommonUnit(aStartValue.GetUnit(),
                                         aEndValue.GetUnit());
  // Maybe need a followup method to convert the inputs into the common
  // unit-type, if they don't already match it. (Or would it make sense to do
  // that in GetCommonUnit? in which case maybe ConvertToCommonUnit would be
  // better.)

  PRBool success = PR_TRUE;
  switch (commonUnit) {
    case eStyleUnit_Coord: {
      nscoord startCoord = aStartValue.GetCoordValue();
      nscoord endCoord = aEndValue.GetCoordValue();
      nscoord resultCoord = startCoord +
        NSToCoordRound(aPortion * (endCoord - startCoord));
      aResultValue.SetCoordValue(resultCoord);
      break;
    }
    case eStyleUnit_Percent: {
      float startPct = aStartValue.GetPercentValue();
      float endPct = aEndValue.GetPercentValue();
      float resultPct = startPct + aPortion * (endPct - startPct);
      aResultValue.SetPercentValue(resultPct);
      break;
    }
    case eStyleUnit_Color: {
      double inv = 1.0 - aPortion;
      nscolor startColor = aStartValue.GetColorValue();
      nscolor endColor = aEndValue.GetColorValue();
      // FIXME (spec): The CSS transitions spec doesn't say whether
      // colors are premultiplied, but things work better when they are,
      // so use premultiplication.  Spec issue is still open per
      // http://lists.w3.org/Archives/Public/www-style/2009Jul/0050.html

      // To save some math, scale the alpha down to a 0-1 scale, but
      // leave the color components on a 0-255 scale.
      double startA = NS_GET_A(startColor) * (1.0 / 255.0);
      double startR = NS_GET_R(startColor) * startA;
      double startG = NS_GET_G(startColor) * startA;
      double startB = NS_GET_B(startColor) * startA;
      double endA = NS_GET_A(endColor) * (1.0 / 255.0);
      double endR = NS_GET_R(endColor) * endA;
      double endG = NS_GET_G(endColor) * endA;
      double endB = NS_GET_B(endColor) * endA;
      double resAf = (startA * inv + endA * aPortion);
      nscolor resultColor;
      if (resAf == 0.0) {
        resultColor = NS_RGBA(0, 0, 0, 0);
      } else {
        double factor = 1.0 / resAf;
        PRUint8 resA = NSToIntRound(resAf * 255.0);
        PRUint8 resR = NSToIntRound((startR * inv + endR * aPortion) * factor);
        PRUint8 resG = NSToIntRound((startG * inv + endG * aPortion) * factor);
        PRUint8 resB = NSToIntRound((startB * inv + endB * aPortion) * factor);
        resultColor = NS_RGBA(resR, resG, resB, resA);
      }
      aResultValue.SetColorValue(resultColor);
      break;
    }
    case eStyleUnit_Null:
      success = PR_FALSE;
      break;
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
                               nsStyleCoord& aComputedValue)
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
                                   styleContext->GetPseudoType(),
                                   styleContext->GetRuleNode(), ruleArray);

  // Extract computed value of our property from the temporary style rule
  return ExtractComputedValue(aProperty, tmpStyleContext, aComputedValue);
}

PRBool
nsStyleAnimation::UncomputeValue(nsCSSProperty aProperty,
                                 nsPresContext* aPresContext,
                                 const nsStyleCoord& aComputedValue,
                                 void* aSpecifiedValue)
{
  NS_ABORT_IF_FALSE(aPresContext, "null pres context");

  switch (aComputedValue.GetUnit()) {
    case eStyleUnit_None:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValuePair, "type mismatch");
      static_cast<nsCSSValuePair*>(aSpecifiedValue)->
        SetBothValuesTo(nsCSSValue(eCSSUnit_None));
      break;
    case eStyleUnit_Coord: {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      float pxVal = aPresContext->AppUnitsToFloatCSSPixels(
                                    aComputedValue.GetCoordValue());
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetFloatValue(pxVal, eCSSUnit_Pixel);
      break;
    }
    case eStyleUnit_Percent:
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      static_cast<nsCSSValue*>(aSpecifiedValue)->
        SetPercentValue(aComputedValue.GetPercentValue());
      break;
    case eStyleUnit_Color:
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
    default:
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsStyleAnimation::UncomputeValue(nsCSSProperty aProperty,
                                 nsPresContext* aPresContext,
                                 const nsStyleCoord& aComputedValue,
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

PRBool
nsStyleAnimation::ExtractComputedValue(nsCSSProperty aProperty,
                                       nsStyleContext* aStyleContext,
                                       nsStyleCoord& aComputedValue)
{
  NS_ABORT_IF_FALSE(0 <= aProperty &&
                    aProperty < eCSSProperty_COUNT_no_shorthands,
                    "bad property");
  const void* styleStruct =
    aStyleContext->GetStyleData(nsCSSProps::kSIDTable[aProperty]);
  ptrdiff_t ssOffset = nsCSSProps::kStyleStructOffsetTable[aProperty];
  NS_ABORT_IF_FALSE(0 <= ssOffset, "must be dealing with animatable property");
  nsStyleAnimType animType = nsCSSProps::kAnimTypeTable[aProperty];
  switch (animType) {
    case eStyleAnimType_Coord:
      aComputedValue = *static_cast<const nsStyleCoord*>(
        StyleDataAtOffset(styleStruct, ssOffset));
      return PR_TRUE;
    case eStyleAnimType_Sides_Top:
    case eStyleAnimType_Sides_Right:
    case eStyleAnimType_Sides_Bottom:
    case eStyleAnimType_Sides_Left:
      PR_STATIC_ASSERT(0 == NS_SIDE_TOP);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Right - eStyleAnimType_Sides_Top
                         == NS_SIDE_RIGHT);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Bottom - eStyleAnimType_Sides_Top
                         == NS_SIDE_BOTTOM);
      PR_STATIC_ASSERT(eStyleAnimType_Sides_Left - eStyleAnimType_Sides_Top
                         == NS_SIDE_LEFT);
      aComputedValue = static_cast<const nsStyleSides*>(
        StyleDataAtOffset(styleStruct, ssOffset))->
          Get(animType - eStyleAnimType_Sides_Top);
      return PR_TRUE;
    case eStyleAnimType_nscoord:
      aComputedValue.SetCoordValue(*static_cast<const nscoord*>(
        StyleDataAtOffset(styleStruct, ssOffset)));
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
    case eStyleAnimType_None:
      NS_NOTREACHED("shouldn't use on non-animatable properties");
  }
  return PR_FALSE;
}
