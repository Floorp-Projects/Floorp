/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable mapped attribute on an element */
#include "nsSMILMappedAttribute.h"
#include "nsPropertyTable.h"
#include "nsContentErrors.h" // For NS_PROPTABLE_PROP_OVERWRITTEN
#include "nsSMILValue.h"
#include "nsSMILCSSValueType.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsCSSProps.h"
#include "mozilla/dom/Element.h"

// Callback function, for freeing string buffers stored in property table
static void
ReleaseStringBufferPropertyValue(void*    aObject,       /* unused */
                                 nsIAtom* aPropertyName, /* unused */
                                 void*    aPropertyValue,
                                 void*    aData          /* unused */)
{
  nsStringBuffer* buf = static_cast<nsStringBuffer*>(aPropertyValue);
  buf->Release();
}


nsresult
nsSMILMappedAttribute::ValueFromString(const nsAString& aStr,
                                       const nsISMILAnimationElement* aSrcElement,
                                       nsSMILValue& aValue,
                                       bool& aPreventCachingOfSandwich) const
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);

  nsSMILCSSValueType::ValueFromString(mPropID, mElement, aStr, aValue,
                                      &aPreventCachingOfSandwich);
  return aValue.IsNull() ? NS_ERROR_FAILURE : NS_OK;
}

nsSMILValue
nsSMILMappedAttribute::GetBaseValue() const
{
  nsAutoString baseStringValue;
  nsRefPtr<nsIAtom> attrName = GetAttrNameAtom();
  bool success = mElement->GetAttr(kNameSpaceID_None, attrName,
                                     baseStringValue);
  nsSMILValue baseValue;
  if (success) {
    // For base values, we don't need to worry whether the value returned is
    // context-sensitive or not since the compositor will take care of comparing
    // the returned (computed) base value and its cached value and determining
    // if an update is required or not.
    nsSMILCSSValueType::ValueFromString(mPropID, mElement,
                                        baseStringValue, baseValue, nsnull);
  } else {
    // Attribute is unset -- use computed value.
    // FIRST: Temporarily clear animated value, to make sure it doesn't pollute
    // the computed value. (We want base value, _without_ animations applied.)
    void* buf = mElement->UnsetProperty(SMIL_MAPPED_ATTR_ANIMVAL,
                                        attrName, nsnull);
    FlushChangesToTargetAttr();

    // SECOND: we use nsSMILCSSProperty::GetBaseValue to look up the property's
    // computed value.  NOTE: This call will temporarily clear the SMIL
    // override-style for the corresponding CSS property on our target element.
    // This prevents any animations that target the CSS property from affecting
    // animations that target the mapped attribute.
    baseValue = nsSMILCSSProperty::GetBaseValue();

    // FINALLY: If we originally had an animated value set, then set it again.
    if (buf) {
      mElement->SetProperty(SMIL_MAPPED_ATTR_ANIMVAL, attrName, buf,
                            ReleaseStringBufferPropertyValue);
      FlushChangesToTargetAttr();
    }
  }
  return baseValue;
}

nsresult
nsSMILMappedAttribute::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ENSURE_TRUE(IsPropertyAnimatable(mPropID), NS_ERROR_FAILURE);

  // Convert nsSMILValue to string
  nsAutoString valStr;
  if (!nsSMILCSSValueType::ValueToString(aValue, valStr)) {
    NS_WARNING("Failed to convert nsSMILValue for mapped attr into a string");
    return NS_ERROR_FAILURE;
  }

  // Set the string as this mapped attribute's animated value.
  nsStringBuffer* valStrBuf =
    nsCSSValue::BufferFromString(nsString(valStr)).get();
  nsRefPtr<nsIAtom> attrName = GetAttrNameAtom();
  nsresult rv = mElement->SetProperty(SMIL_MAPPED_ATTR_ANIMVAL,
                                      attrName, valStrBuf,
                                      ReleaseStringBufferPropertyValue);
  if (rv == NS_PROPTABLE_PROP_OVERWRITTEN) {
    rv = NS_OK;
  }
  FlushChangesToTargetAttr();

  return rv;
}

void
nsSMILMappedAttribute::ClearAnimValue()
{
  nsRefPtr<nsIAtom> attrName = GetAttrNameAtom();
  mElement->DeleteProperty(SMIL_MAPPED_ATTR_ANIMVAL, attrName);
  FlushChangesToTargetAttr();
}

void
nsSMILMappedAttribute::FlushChangesToTargetAttr() const
{
  // Clear animated content-style-rule
  mElement->DeleteProperty(SMIL_MAPPED_ATTR_ANIMVAL,
                           SMIL_MAPPED_ATTR_STYLERULE_ATOM);
  nsIDocument* doc = mElement->GetCurrentDoc();

  // Request animation restyle
  if (doc) {
    nsIPresShell* shell = doc->GetShell();
    if (shell) {
      shell->RestyleForAnimation(mElement, eRestyle_Self);
    }
  }
}

already_AddRefed<nsIAtom>
nsSMILMappedAttribute::GetAttrNameAtom() const
{
  return do_GetAtom(nsCSSProps::GetStringValue(mPropID));
}
