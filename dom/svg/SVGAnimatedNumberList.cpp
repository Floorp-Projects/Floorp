/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGElement.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsSMILValue.h"
#include "SVGNumberListSMILType.h"

namespace mozilla {

nsresult
SVGAnimatedNumberList::SetBaseValueString(const nsAString& aValue)
{
  SVGNumberList newBaseValue;
  nsresult rv = newBaseValue.SetValueFromString(aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  DOMSVGAnimatedNumberList *domWrapper =
    DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! If the length
    // of our baseVal is being reduced, our baseVal's DOM wrapper list may have
    // to remove DOM items from itself, and any removed DOM items need to copy
    // their internal counterpart values *before* we change them.
    //
    domWrapper->InternalBaseValListWillChangeTo(newBaseValue);
  }

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.

  mIsBaseSet = true;
  rv = mBaseVal.CopyFrom(newBaseValue);
  if (NS_FAILED(rv) && domWrapper) {
    // Attempting to increase mBaseVal's length failed - reduce domWrapper
    // back to the same length:
    domWrapper->InternalBaseValListWillChangeTo(mBaseVal);
  }
  return rv;
}

void
SVGAnimatedNumberList::ClearBaseValue(uint32_t aAttrEnum)
{
  DOMSVGAnimatedNumberList *domWrapper =
    DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! (See above.)
    domWrapper->InternalBaseValListWillChangeTo(SVGNumberList());
  }
  mBaseVal.Clear();
  mIsBaseSet = false;
  // Caller notifies
}

nsresult
SVGAnimatedNumberList::SetAnimValue(const SVGNumberList& aNewAnimValue,
                                    nsSVGElement *aElement,
                                    uint32_t aAttrEnum)
{
  DOMSVGAnimatedNumberList *domWrapper =
    DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // A new animation may totally change the number of items in the animVal
    // list, replacing what was essentially a mirror of the baseVal list, or
    // else replacing and overriding an existing animation. When this happens
    // we must try and keep our animVal's DOM wrapper in sync (see the comment
    // in DOMSVGAnimatedNumberList::InternalBaseValListWillChangeTo).
    //
    // It's not possible for us to reliably distinguish between calls to this
    // method that are setting a new sample for an existing animation, and
    // calls that are setting the first sample of an animation that will
    // override an existing animation. Happily it's cheap to just blindly
    // notify our animVal's DOM wrapper of its internal counterpart's new value
    // each time this method is called, so that's what we do.
    //
    // Note that we must send this notification *before* setting or changing
    // mAnimVal! (See the comment in SetBaseValueString above.)
    //
    domWrapper->InternalAnimValListWillChangeTo(aNewAnimValue);
  }
  if (!mAnimVal) {
    mAnimVal = new SVGNumberList();
  }
  nsresult rv = mAnimVal->CopyFrom(aNewAnimValue);
  if (NS_FAILED(rv)) {
    // OOM. We clear the animation, and, importantly, ClearAnimValue() ensures
    // that mAnimVal and its DOM wrapper (if any) will have the same length!
    ClearAnimValue(aElement, aAttrEnum);
    return rv;
  }
  aElement->DidAnimateNumberList(aAttrEnum);
  return NS_OK;
}

void
SVGAnimatedNumberList::ClearAnimValue(nsSVGElement *aElement,
                                      uint32_t aAttrEnum)
{
  DOMSVGAnimatedNumberList *domWrapper =
    DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // When all animation ends, animVal simply mirrors baseVal, which may have
    // a different number of items to the last active animated value. We must
    // keep the length of our animVal's DOM wrapper list in sync, and again we
    // must do that before touching mAnimVal. See comments above.
    //
    domWrapper->InternalAnimValListWillChangeTo(mBaseVal);
  }
  mAnimVal = nullptr;
  aElement->DidAnimateNumberList(aAttrEnum);
}

nsISMILAttr*
SVGAnimatedNumberList::ToSMILAttr(nsSVGElement *aSVGElement,
                                  uint8_t aAttrEnum)
{
  return new SMILAnimatedNumberList(this, aSVGElement, aAttrEnum);
}

nsresult
SVGAnimatedNumberList::
  SMILAnimatedNumberList::ValueFromString(const nsAString& aStr,
                               const dom::SVGAnimationElement* /*aSrcElement*/,
                               nsSMILValue& aValue,
                               bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SVGNumberListSMILType::sSingleton);
  SVGNumberListAndInfo *nlai = static_cast<SVGNumberListAndInfo*>(val.mU.mPtr);
  nsresult rv = nlai->SetValueFromString(aStr);
  if (NS_SUCCEEDED(rv)) {
    nlai->SetInfo(mElement);
    aValue.Swap(val);
  }
  aPreventCachingOfSandwich = false;
  return rv;
}

nsSMILValue
SVGAnimatedNumberList::SMILAnimatedNumberList::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue val;

  nsSMILValue tmp(&SVGNumberListSMILType::sSingleton);
  SVGNumberListAndInfo *nlai = static_cast<SVGNumberListAndInfo*>(tmp.mU.mPtr);
  nsresult rv = nlai->CopyFrom(mVal->mBaseVal);
  if (NS_SUCCEEDED(rv)) {
    nlai->SetInfo(mElement);
    val.Swap(tmp);
  }
  return val;
}

nsresult
SVGAnimatedNumberList::SMILAnimatedNumberList::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGNumberListSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGNumberListSMILType::sSingleton) {
    mVal->SetAnimValue(*static_cast<SVGNumberListAndInfo*>(aValue.mU.mPtr),
                       mElement,
                       mAttrEnum);
  }
  return NS_OK;
}

void
SVGAnimatedNumberList::SMILAnimatedNumberList::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement, mAttrEnum);
  }
}

} // namespace mozilla
