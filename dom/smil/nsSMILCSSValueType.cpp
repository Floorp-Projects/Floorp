/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a value for a SMIL-animated CSS property */

#include "nsSMILCSSValueType.h"
#include "nsString.h"
#include "nsSMILParserUtils.h"
#include "nsSMILValue.h"
#include "nsCSSValue.h"
#include "nsColor.h"
#include "nsPresContext.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/Element.h"
#include "nsDebug.h"
#include "nsStyleUtil.h"
#include "nsIDocument.h"

using namespace mozilla::dom;
using mozilla::StyleAnimationValue;

/*static*/ nsSMILCSSValueType nsSMILCSSValueType::sSingleton;

struct ValueWrapper {
  ValueWrapper(nsCSSProperty aPropID, const StyleAnimationValue& aValue) :
    mPropID(aPropID), mCSSValue(aValue) {}

  nsCSSProperty mPropID;
  StyleAnimationValue mCSSValue;
};

// Helper Methods
// --------------
static const StyleAnimationValue*
GetZeroValueForUnit(StyleAnimationValue::Unit aUnit)
{
  static const StyleAnimationValue
    sZeroCoord(0, StyleAnimationValue::CoordConstructor);
  static const StyleAnimationValue
    sZeroPercent(0.0f, StyleAnimationValue::PercentConstructor);
  static const StyleAnimationValue
    sZeroFloat(0.0f,  StyleAnimationValue::FloatConstructor);
  static const StyleAnimationValue
    sZeroColor(NS_RGB(0,0,0), StyleAnimationValue::ColorConstructor);

  MOZ_ASSERT(aUnit != StyleAnimationValue::eUnit_Null,
             "Need non-null unit for a zero value");
  switch (aUnit) {
    case StyleAnimationValue::eUnit_Coord:
      return &sZeroCoord;
    case StyleAnimationValue::eUnit_Percent:
      return &sZeroPercent;
    case StyleAnimationValue::eUnit_Float:
      return &sZeroFloat;
    case StyleAnimationValue::eUnit_Color:
      return &sZeroColor;
    default:
      return nullptr;
  }
}

// This method requires at least one of its arguments to be non-null.
//
// If one argument is null, this method updates it to point to "zero"
// for the other argument's Unit (if applicable; otherwise, we return false).
//
// If neither argument is null, this method generally does nothing, though it
// may apply a workaround for the special case where a 0 length-value is mixed
// with a eUnit_Float value.  (See comment below.)
//
// Returns true on success, or false.
static const bool
FinalizeStyleAnimationValues(const StyleAnimationValue*& aValue1,
                             const StyleAnimationValue*& aValue2)
{
  MOZ_ASSERT(aValue1 || aValue2,
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
  // won't interoperate in StyleAnimationValue, since their Units don't match.
  // In this case, we replace the eUnit_Coord 0 value with eUnit_Float 0 value.
  const StyleAnimationValue& zeroCoord =
    *GetZeroValueForUnit(StyleAnimationValue::eUnit_Coord);
  if (*aValue1 == zeroCoord &&
      aValue2->GetUnit() == StyleAnimationValue::eUnit_Float) {
    aValue1 = GetZeroValueForUnit(StyleAnimationValue::eUnit_Float);
  } else if (*aValue2 == zeroCoord &&
             aValue1->GetUnit() == StyleAnimationValue::eUnit_Float) {
    aValue2 = GetZeroValueForUnit(StyleAnimationValue::eUnit_Float);
  }

  return true;
}

static void
InvertSign(StyleAnimationValue& aValue)
{
  switch (aValue.GetUnit()) {
    case StyleAnimationValue::eUnit_Coord:
      aValue.SetCoordValue(-aValue.GetCoordValue());
      break;
    case StyleAnimationValue::eUnit_Percent:
      aValue.SetPercentValue(-aValue.GetPercentValue());
      break;
    case StyleAnimationValue::eUnit_Float:
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
  MOZ_ASSERT(aValue.IsNull(), "Unexpected SMIL value type");

  aValue.mU.mPtr = nullptr;
  aValue.mType = this;
}

void
nsSMILCSSValueType::Destroy(nsSMILValue& aValue) const
{
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<ValueWrapper*>(aValue.mU.mPtr);
  aValue.mType = nsSMILNullType::Singleton();
}

nsresult
nsSMILCSSValueType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value type");
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
    aDest.mU.mPtr = destWrapper = nullptr;
  } // else, both are barely-initialized -- nothing to do.

  return NS_OK;
}

bool
nsSMILCSSValueType::IsEqual(const nsSMILValue& aLeft,
                            const nsSMILValue& aRight) const
{
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected SMIL value");
  const ValueWrapper* leftWrapper = ExtractValueWrapper(aLeft);
  const ValueWrapper* rightWrapper = ExtractValueWrapper(aRight);

  if (leftWrapper) {
    if (rightWrapper) {
      // Both non-null
      NS_WARN_IF_FALSE(leftWrapper != rightWrapper,
                       "Two nsSMILValues with matching ValueWrapper ptr");
      return (leftWrapper->mPropID == rightWrapper->mPropID &&
              leftWrapper->mCSSValue == rightWrapper->mCSSValue);
    }
    // Left non-null, right null
    return false;
  }
  if (rightWrapper) {
    // Left null, right non-null
    return false;
  }
  // Both null
  return true;
}

nsresult
nsSMILCSSValueType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                        uint32_t aCount) const
{
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType,
             "Trying to add invalid types");
  MOZ_ASSERT(aValueToAdd.mType == this, "Unexpected source type");

  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);
  const ValueWrapper* valueToAddWrapper = ExtractValueWrapper(aValueToAdd);
  MOZ_ASSERT(destWrapper || valueToAddWrapper,
             "need at least one fully-initialized value");

  nsCSSProperty property = (valueToAddWrapper ? valueToAddWrapper->mPropID :
                            destWrapper->mPropID);
  // Special case: font-size-adjust and stroke-dasharray are explicitly
  // non-additive (even though StyleAnimationValue *could* support adding them)
  if (property == eCSSProperty_font_size_adjust ||
      property == eCSSProperty_stroke_dasharray) {
    return NS_ERROR_FAILURE;
  }

  const StyleAnimationValue* valueToAdd = valueToAddWrapper ?
    &valueToAddWrapper->mCSSValue : nullptr;
  const StyleAnimationValue* destValue = destWrapper ?
    &destWrapper->mCSSValue : nullptr;
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
      new ValueWrapper(property, *destValue);
  }

  return StyleAnimationValue::Add(property,
                                  destWrapper->mCSSValue, *valueToAdd, aCount) ?
    NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSValueType::ComputeDistance(const nsSMILValue& aFrom,
                                    const nsSMILValue& aTo,
                                    double& aDistance) const
{
  MOZ_ASSERT(aFrom.mType == aTo.mType,
             "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  const ValueWrapper* fromWrapper = ExtractValueWrapper(aFrom);
  const ValueWrapper* toWrapper = ExtractValueWrapper(aTo);
  MOZ_ASSERT(toWrapper, "expecting non-null endpoint");

  const StyleAnimationValue* fromCSSValue = fromWrapper ?
    &fromWrapper->mCSSValue : nullptr;
  const StyleAnimationValue* toCSSValue = &toWrapper->mCSSValue;
  if (!FinalizeStyleAnimationValues(fromCSSValue, toCSSValue)) {
    return NS_ERROR_FAILURE;
  }

  return StyleAnimationValue::ComputeDistance(toWrapper->mPropID,
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
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this,
             "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");
  MOZ_ASSERT(aUnitDistance >= 0.0 && aUnitDistance <= 1.0,
             "unit distance value out of bounds");
  MOZ_ASSERT(!aResult.mU.mPtr, "expecting barely-initialized outparam");

  const ValueWrapper* startWrapper = ExtractValueWrapper(aStartVal);
  const ValueWrapper* endWrapper = ExtractValueWrapper(aEndVal);
  MOZ_ASSERT(endWrapper, "expecting non-null endpoint");

  const StyleAnimationValue* startCSSValue = startWrapper ?
    &startWrapper->mCSSValue : nullptr;
  const StyleAnimationValue* endCSSValue = &endWrapper->mCSSValue;
  if (!FinalizeStyleAnimationValues(startCSSValue, endCSSValue)) {
    return NS_ERROR_FAILURE;
  }

  StyleAnimationValue resultValue;
  if (StyleAnimationValue::Interpolate(endWrapper->mPropID,
                                       *startCSSValue, *endCSSValue,
                                       aUnitDistance, resultValue)) {
    aResult.mU.mPtr = new ValueWrapper(endWrapper->mPropID, resultValue);
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
    return nullptr;
  }
  nsIPresShell* shell = doc->GetShell();
  return shell ? shell->GetPresContext() : nullptr;
}

// Helper function to parse a string into a StyleAnimationValue
static bool
ValueFromStringHelper(nsCSSProperty aPropID,
                      Element* aTargetElement,
                      nsPresContext* aPresContext,
                      const nsAString& aString,
                      StyleAnimationValue& aStyleAnimValue,
                      bool* aIsContextSensitive)
{
  // If value is negative, we'll strip off the "-" so the CSS parser won't
  // barf, and then manually make the parsed value negative.
  // (This is a partial solution to let us accept some otherwise out-of-bounds
  // CSS values. Bug 501188 will provide a more complete fix.)
  bool isNegative = false;
  uint32_t subStringBegin = 0;

  // NOTE: We need to opt-out 'stroke-dasharray' from the negative-number
  // check.  Its values might look negative (e.g. by starting with "-1"), but
  // they're more complicated than our simple negation logic here can handle.
  if (aPropID != eCSSProperty_stroke_dasharray) {
    int32_t absValuePos = nsSMILParserUtils::CheckForNegativeNumber(aString);
    if (absValuePos > 0) {
      isNegative = true;
      subStringBegin = (uint32_t)absValuePos; // Start parsing after '-' sign
    }
  }
  nsDependentSubstring subString(aString, subStringBegin);
  if (!StyleAnimationValue::ComputeValue(aPropID, aTargetElement, subString,
                                         true, aStyleAnimValue,
                                         aIsContextSensitive)) {
    return false;
  }
  if (isNegative) {
    InvertSign(aStyleAnimValue);
  }

  if (aPropID == eCSSProperty_font_size) {
    // Divide out text-zoom, since SVG is supposed to ignore it
    MOZ_ASSERT(aStyleAnimValue.GetUnit() == StyleAnimationValue::eUnit_Coord,
               "'font-size' value with unexpected style unit");
    aStyleAnimValue.SetCoordValue(aStyleAnimValue.GetCoordValue() /
                                  aPresContext->TextZoom());
  }
  return true;
}

// static
void
nsSMILCSSValueType::ValueFromString(nsCSSProperty aPropID,
                                    Element* aTargetElement,
                                    const nsAString& aString,
                                    nsSMILValue& aValue,
                                    bool* aIsContextSensitive)
{
  MOZ_ASSERT(aValue.IsNull(), "Outparam should be null-typed");
  nsPresContext* presContext = GetPresContextForElement(aTargetElement);
  if (!presContext) {
    NS_WARNING("Not parsing animation value; unable to get PresContext");
    return;
  }

  nsIDocument* doc = aTargetElement->GetCurrentDoc();
  if (doc && !nsStyleUtil::CSPAllowsInlineStyle(nullptr,
                                                doc->NodePrincipal(),
                                                doc->GetDocumentURI(),
                                                0, aString, nullptr)) {
    return;
  }

  StyleAnimationValue parsedValue;
  if (ValueFromStringHelper(aPropID, aTargetElement, presContext,
                            aString, parsedValue, aIsContextSensitive)) {
    sSingleton.Init(aValue);
    aValue.mU.mPtr = new ValueWrapper(aPropID, parsedValue);
  }
}

// static
bool
nsSMILCSSValueType::ValueToString(const nsSMILValue& aValue,
                                  nsAString& aString)
{
  MOZ_ASSERT(aValue.mType == &nsSMILCSSValueType::sSingleton,
             "Unexpected SMIL value type");
  const ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  return !wrapper ||
    StyleAnimationValue::UncomputeValue(wrapper->mPropID,
                                        wrapper->mCSSValue, aString);
}
