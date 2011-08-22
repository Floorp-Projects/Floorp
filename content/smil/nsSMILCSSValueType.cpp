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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

/* representation of a value for a SMIL-animated CSS property */

#include "nsSMILCSSValueType.h"
#include "nsString.h"
#include "nsStyleAnimation.h"
#include "nsSMILParserUtils.h"
#include "nsSMILValue.h"
#include "nsCSSValue.h"
#include "nsColor.h"
#include "nsPresContext.h"
#include "mozilla/dom/Element.h"
#include "nsDebug.h"

using namespace mozilla::dom;

/*static*/ nsSMILCSSValueType nsSMILCSSValueType::sSingleton;

struct ValueWrapper {
  ValueWrapper(nsCSSProperty aPropID, const nsStyleAnimation::Value& aValue,
               nsPresContext* aPresContext) :
    mPropID(aPropID), mCSSValue(aValue), mPresContext(aPresContext) {}

  nsCSSProperty mPropID;
  nsStyleAnimation::Value mCSSValue;
  nsPresContext* mPresContext;
};

// Helper "zero" values of various types
// -------------------------------------
static const nsStyleAnimation::Value
  sZeroCoord(0, nsStyleAnimation::Value::CoordConstructor);
static const nsStyleAnimation::Value
  sZeroPercent(0.0f, nsStyleAnimation::Value::PercentConstructor);
static const nsStyleAnimation::Value
  sZeroFloat(0.0f,  nsStyleAnimation::Value::FloatConstructor);
static const nsStyleAnimation::Value
  sZeroColor(NS_RGB(0,0,0), nsStyleAnimation::Value::ColorConstructor);

// Helper Methods
// --------------
static const nsStyleAnimation::Value*
GetZeroValueForUnit(nsStyleAnimation::Unit aUnit)
{
  NS_ABORT_IF_FALSE(aUnit != nsStyleAnimation::eUnit_Null,
                    "Need non-null unit for a zero value");
  switch (aUnit) {
    case nsStyleAnimation::eUnit_Coord:
      return &sZeroCoord;
    case nsStyleAnimation::eUnit_Percent:
      return &sZeroPercent;
    case nsStyleAnimation::eUnit_Float:
      return &sZeroFloat;
    case nsStyleAnimation::eUnit_Color:
      return &sZeroColor;
    default:
      return nsnull;
  }
}

// This method requires at least one of its arguments to be non-null.
//
// If one argument is null, this method updates it to point to "zero"
// for the other argument's Unit (if applicable; otherwise, we return PR_FALSE).
//
// If neither argument is null, this method generally does nothing, though it
// may apply a workaround for the special case where a 0 length-value is mixed
// with a eUnit_Float value.  (See comment below.)
//
// Returns PR_TRUE on success, or PR_FALSE.
static const PRBool
FinalizeStyleAnimationValues(const nsStyleAnimation::Value*& aValue1,
                             const nsStyleAnimation::Value*& aValue2)
{
  NS_ABORT_IF_FALSE(aValue1 || aValue2,
                    "expecting at least one non-null value");

  // Are we missing either val? (If so, it's an implied 0 in other val's units)
  if (!aValue1) {
    aValue1 = GetZeroValueForUnit(aValue2->GetUnit());
    return !!aValue1; // Fail if we have no zero value for this unit.
  }
  if (!aValue2) {
    aValue2 = GetZeroValueForUnit(aValue1->GetUnit());
    return !!aValue2; // Fail if we have no zero value for this unit.
  }

  // Ok, both values were specified.
  // Need to handle a special-case, though: unitless nonzero length (parsed as
  // eUnit_Float) mixed with unitless 0 length (parsed as eUnit_Coord).  These
  // won't interoperate in nsStyleAnimation, since their Units don't match.
  // In this case, we replace the eUnit_Coord 0 value with eUnit_Float 0 value.
  if (*aValue1 == sZeroCoord &&
      aValue2->GetUnit() == nsStyleAnimation::eUnit_Float) {
    aValue1 = &sZeroFloat;
  } else if (*aValue2 == sZeroCoord &&
             aValue1->GetUnit() == nsStyleAnimation::eUnit_Float) {
    aValue2 = &sZeroFloat;
  }

  return PR_TRUE;
}

static void
InvertSign(nsStyleAnimation::Value& aValue)
{
  switch (aValue.GetUnit()) {
    case nsStyleAnimation::eUnit_Coord:
      aValue.SetCoordValue(-aValue.GetCoordValue());
      break;
    case nsStyleAnimation::eUnit_Percent:
      aValue.SetPercentValue(-aValue.GetPercentValue());
      break;
    case nsStyleAnimation::eUnit_Float:
      aValue.SetFloatValue(-aValue.GetFloatValue());
      break;
    default:
      NS_NOTREACHED("Calling InvertSign with an unsupported unit");
      break;
  }
}

static ValueWrapper*
ExtractValueWrapper(nsSMILValue& aValue)
{
  return static_cast<ValueWrapper*>(aValue.mU.mPtr);
}

static const ValueWrapper*
ExtractValueWrapper(const nsSMILValue& aValue)
{
  return static_cast<const ValueWrapper*>(aValue.mU.mPtr);
}

// Class methods
// -------------
void
nsSMILCSSValueType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected SMIL value type");

  aValue.mU.mPtr = nsnull;
  aValue.mType = this;
}

void
nsSMILCSSValueType::Destroy(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<ValueWrapper*>(aValue.mU.mPtr);
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
nsSMILCSSValueType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_ABORT_IF_FALSE(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_ABORT_IF_FALSE(aDest.mType == this, "Unexpected SMIL value type");
  const ValueWrapper* srcWrapper = ExtractValueWrapper(aSrc);
  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);

  if (srcWrapper) {
    if (!destWrapper) {
      // barely-initialized dest -- need to alloc & copy
      aDest.mU.mPtr = new ValueWrapper(*srcWrapper);
    } else {
      // both already fully-initialized -- just copy straight across
      *destWrapper = *srcWrapper;
    }
  } else if (destWrapper) {
    // fully-initialized dest, barely-initialized src -- clear dest
    delete destWrapper;
    aDest.mU.mPtr = destWrapper = nsnull;
  } // else, both are barely-initialized -- nothing to do.

  return NS_OK;
}

PRBool
nsSMILCSSValueType::IsEqual(const nsSMILValue& aLeft,
                            const nsSMILValue& aRight) const
{
  NS_ABORT_IF_FALSE(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_ABORT_IF_FALSE(aLeft.mType == this, "Unexpected SMIL value");
  const ValueWrapper* leftWrapper = ExtractValueWrapper(aLeft);
  const ValueWrapper* rightWrapper = ExtractValueWrapper(aRight);

  if (leftWrapper) {
    if (rightWrapper) {
      // Both non-null
      NS_WARN_IF_FALSE(leftWrapper != rightWrapper,
                       "Two nsSMILValues with matching ValueWrapper ptr");
      // mPresContext doesn't really matter for equality comparison
      return (leftWrapper->mPropID == rightWrapper->mPropID &&
              leftWrapper->mCSSValue == rightWrapper->mCSSValue);
    }
    // Left non-null, right null
    return PR_FALSE;
  }
  if (rightWrapper) {
    // Left null, right non-null
    return PR_FALSE;
  }
  // Both null
  return PR_TRUE;
}

nsresult
nsSMILCSSValueType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                        PRUint32 aCount) const
{
  NS_ABORT_IF_FALSE(aValueToAdd.mType == aDest.mType,
                    "Trying to add invalid types");
  NS_ABORT_IF_FALSE(aValueToAdd.mType == this, "Unexpected source type");

  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);
  const ValueWrapper* valueToAddWrapper = ExtractValueWrapper(aValueToAdd);
  NS_ABORT_IF_FALSE(destWrapper || valueToAddWrapper,
                    "need at least one fully-initialized value");

  nsCSSProperty property = (valueToAddWrapper ? valueToAddWrapper->mPropID :
                            destWrapper->mPropID);
  // Special case: font-size-adjust and stroke-dasharray are explicitly
  // non-additive (even though nsStyleAnimation *could* support adding them)
  if (property == eCSSProperty_font_size_adjust ||
      property == eCSSProperty_stroke_dasharray) {
    return NS_ERROR_FAILURE;
  }

  const nsStyleAnimation::Value* valueToAdd = valueToAddWrapper ?
    &valueToAddWrapper->mCSSValue : nsnull;
  const nsStyleAnimation::Value* destValue = destWrapper ?
    &destWrapper->mCSSValue : nsnull;
  if (!FinalizeStyleAnimationValues(valueToAdd, destValue)) {
    return NS_ERROR_FAILURE;
  }
  // Did FinalizeStyleAnimationValues change destValue?
  // If so, update outparam to use the new value.
  if (destWrapper && &destWrapper->mCSSValue != destValue) {
    destWrapper->mCSSValue = *destValue;
  }

  // Handle barely-initialized "zero" destination.
  if (!destWrapper) {
    aDest.mU.mPtr = destWrapper =
      new ValueWrapper(property, *destValue, valueToAddWrapper->mPresContext);
  }

  return nsStyleAnimation::Add(property,
                               destWrapper->mCSSValue, *valueToAdd, aCount) ?
    NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSValueType::ComputeDistance(const nsSMILValue& aFrom,
                                    const nsSMILValue& aTo,
                                    double& aDistance) const
{
  NS_ABORT_IF_FALSE(aFrom.mType == aTo.mType,
                    "Trying to compare different types");
  NS_ABORT_IF_FALSE(aFrom.mType == this, "Unexpected source type");

  const ValueWrapper* fromWrapper = ExtractValueWrapper(aFrom);
  const ValueWrapper* toWrapper = ExtractValueWrapper(aTo);
  NS_ABORT_IF_FALSE(toWrapper, "expecting non-null endpoint");

  const nsStyleAnimation::Value* fromCSSValue = fromWrapper ?
    &fromWrapper->mCSSValue : nsnull;
  const nsStyleAnimation::Value* toCSSValue = &toWrapper->mCSSValue;
  if (!FinalizeStyleAnimationValues(fromCSSValue, toCSSValue)) {
    return NS_ERROR_FAILURE;
  }

  return nsStyleAnimation::ComputeDistance(toWrapper->mPropID,
                                           *fromCSSValue, *toCSSValue,
                                           aDistance) ?
    NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSValueType::Interpolate(const nsSMILValue& aStartVal,
                                const nsSMILValue& aEndVal,
                                double aUnitDistance,
                                nsSMILValue& aResult) const
{
  NS_ABORT_IF_FALSE(aStartVal.mType == aEndVal.mType,
                    "Trying to interpolate different types");
  NS_ABORT_IF_FALSE(aStartVal.mType == this,
                    "Unexpected types for interpolation");
  NS_ABORT_IF_FALSE(aResult.mType == this, "Unexpected result type");
  NS_ABORT_IF_FALSE(aUnitDistance >= 0.0 && aUnitDistance <= 1.0,
                    "unit distance value out of bounds");
  NS_ABORT_IF_FALSE(!aResult.mU.mPtr, "expecting barely-initialized outparam");

  const ValueWrapper* startWrapper = ExtractValueWrapper(aStartVal);
  const ValueWrapper* endWrapper = ExtractValueWrapper(aEndVal);
  NS_ABORT_IF_FALSE(endWrapper, "expecting non-null endpoint");

  const nsStyleAnimation::Value* startCSSValue = startWrapper ?
    &startWrapper->mCSSValue : nsnull;
  const nsStyleAnimation::Value* endCSSValue = &endWrapper->mCSSValue;
  if (!FinalizeStyleAnimationValues(startCSSValue, endCSSValue)) {
    return NS_ERROR_FAILURE;
  }

  nsStyleAnimation::Value resultValue;
  if (nsStyleAnimation::Interpolate(endWrapper->mPropID,
                                    *startCSSValue, *endCSSValue,
                                    aUnitDistance, resultValue)) {
    aResult.mU.mPtr = new ValueWrapper(endWrapper->mPropID, resultValue,
                                       endWrapper->mPresContext);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// Helper function to extract presContext
static nsPresContext*
GetPresContextForElement(Element* aElem)
{
  nsIDocument* doc = aElem->GetCurrentDoc();
  if (!doc) {
    // This can happen if we process certain types of restyles mid-sample
    // and remove anonymous animated content from the document as a result.
    // See bug 534975.
    return nsnull;
  }
  nsIPresShell* shell = doc->GetShell();
  return shell ? shell->GetPresContext() : nsnull;
}

// Helper function to parse a string into a nsStyleAnimation::Value
static PRBool
ValueFromStringHelper(nsCSSProperty aPropID,
                      Element* aTargetElement,
                      nsPresContext* aPresContext,
                      const nsAString& aString,
                      nsStyleAnimation::Value& aStyleAnimValue,
                      PRBool* aIsContextSensitive)
{
  // If value is negative, we'll strip off the "-" so the CSS parser won't
  // barf, and then manually make the parsed value negative.
  // (This is a partial solution to let us accept some otherwise out-of-bounds
  // CSS values. Bug 501188 will provide a more complete fix.)
  PRBool isNegative = PR_FALSE;
  PRUint32 subStringBegin = 0;
  PRInt32 absValuePos = nsSMILParserUtils::CheckForNegativeNumber(aString);
  if (absValuePos > 0) {
    isNegative = PR_TRUE;
    subStringBegin = (PRUint32)absValuePos; // Start parsing after '-' sign
  }
  nsDependentSubstring subString(aString, subStringBegin);
  if (!nsStyleAnimation::ComputeValue(aPropID, aTargetElement, subString,
                                      PR_TRUE, aStyleAnimValue)) {
    return PR_FALSE;
  }
  if (aIsContextSensitive) {
    // XXX
    *aIsContextSensitive = PR_TRUE;
  }
  if (isNegative) {
    InvertSign(aStyleAnimValue);
  }
  
  if (aPropID == eCSSProperty_font_size) {
    // Divide out text-zoom, since SVG is supposed to ignore it
    NS_ABORT_IF_FALSE(aStyleAnimValue.GetUnit() ==
                        nsStyleAnimation::eUnit_Coord,
                      "'font-size' value with unexpected style unit");
    aStyleAnimValue.SetCoordValue(aStyleAnimValue.GetCoordValue() /
                                  aPresContext->TextZoom());
  }
  return PR_TRUE;
}

// static
void
nsSMILCSSValueType::ValueFromString(nsCSSProperty aPropID,
                                    Element* aTargetElement,
                                    const nsAString& aString,
                                    nsSMILValue& aValue,
                                    PRBool* aIsContextSensitive)
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Outparam should be null-typed");
  nsPresContext* presContext = GetPresContextForElement(aTargetElement);
  if (!presContext) {
    NS_WARNING("Not parsing animation value; unable to get PresContext");
    return;
  }

  nsStyleAnimation::Value parsedValue;
  if (ValueFromStringHelper(aPropID, aTargetElement, presContext,
                            aString, parsedValue, aIsContextSensitive)) {
    sSingleton.Init(aValue);
    aValue.mU.mPtr = new ValueWrapper(aPropID, parsedValue, presContext);
  }
}

// static
PRBool
nsSMILCSSValueType::ValueToString(const nsSMILValue& aValue,
                                  nsAString& aString)
{
  NS_ABORT_IF_FALSE(aValue.mType == &nsSMILCSSValueType::sSingleton,
                    "Unexpected SMIL value type");
  const ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  return !wrapper ||
    nsStyleAnimation::UncomputeValue(wrapper->mPropID, wrapper->mPresContext,
                                     wrapper->mCSSValue, aString);
}
