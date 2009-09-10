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
    // XXXdholbert Fail on mismatches for now.
    // Eventually we'll need to be able to resolve conflicts here, e.g. for
    // rgb() colors vs. named colors.  Some conflicts can't be resolved,
    // e.g. percent vs coord, which we'll hopefully handle using CSS calc()
    // once that's implemented.
    NS_WARNING("start unit & end unit don't match -- need to resolve this "
               "and pick which one we want to use");
    return eStyleUnit_Null;
  }
  return aFirstUnit;
}

// CLASS METHODS
// -------------

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
    case eStyleUnit_Null:
      NS_WARNING("Unable to find a common unit for given values");
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
    case eStyleUnit_Null:
      NS_WARNING("Unable to find a common unit for given values");
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
    case eStyleUnit_Null:
      NS_WARNING("Unable to find a common unit for given values");
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
nsStyleAnimation::ExtractComputedValue(nsCSSProperty aProperty,
                                       nsStyleContext* aStyleContext,
                                       nsStyleCoord& aComputedValue)
{
  // XXXdholbert This is a simple implementation that only supports "font-size"
  // and "stroke-width for now, so that we can test the rest of the
  // functionality. A full implementation will require modifications to
  // nsCSSPropList.h, as described in bug 504652 comment 0.
  switch(aProperty) {
    case eCSSProperty_font_size: {
      const nsStyleFont* styleFont = aStyleContext->GetStyleFont();
      aComputedValue.SetCoordValue(styleFont->mFont.size);
      return PR_TRUE;
    }
    case eCSSProperty_stroke_width: {
      const nsStyleSVG* styleSVG = aStyleContext->GetStyleSVG();
      aComputedValue = styleSVG->mStrokeWidth;
      return PR_TRUE;
    }
    default:
      return PR_FALSE;
  }
}
