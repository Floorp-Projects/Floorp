/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedLengthList.h"

#include "DOMSVGAnimatedLengthList.h"
#include "mozilla/Move.h"
#include "nsSVGElement.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsSMILValue.h"
#include "SVGLengthListSMILType.h"

namespace mozilla {

nsresult
SVGAnimatedLengthList::SetBaseValueString(const nsAString& aValue)
{
  SVGLengthList newBaseValue;
  nsresult rv = newBaseValue.SetValueFromString(aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  DOMSVGAnimatedLengthList *domWrapper =
    DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(this);
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

  rv = mBaseVal.CopyFrom(newBaseValue);
  if (NS_FAILED(rv) && domWrapper) {
    // Attempting to increase mBaseVal's length failed - reduce domWrapper
    // back to the same length:
    domWrapper->InternalBaseValListWillChangeTo(mBaseVal);
  }
  return rv;
}

void
SVGAnimatedLengthList::ClearBaseValue(uint32_t aAttrEnum)
{
  DOMSVGAnimatedLengthList *domWrapper =
    DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! (See above.)
    domWrapper->InternalBaseValListWillChangeTo(SVGLengthList());
  }
  mBaseVal.Clear();
  // Caller notifies
}

nsresult
SVGAnimatedLengthList::SetAnimValue(const SVGLengthList& aNewAnimValue,
                                    nsSVGElement *aElement,
                                    uint32_t aAttrEnum)
{
  DOMSVGAnimatedLengthList *domWrapper =
    DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // A new animation may totally change the number of items in the animVal
    // list, replacing what was essentially a mirror of the baseVal list, or
    // else replacing and overriding an existing animation. When this happens
    // we must try and keep our animVal's DOM wrapper in sync (see the comment
    // in DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo).
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
    mAnimVal = new SVGLengthList();
  }
  nsresult rv = mAnimVal->CopyFrom(aNewAnimValue);
  if (NS_FAILED(rv)) {
    // OOM. We clear the animation, and, importantly, ClearAnimValue() ensures
    // that mAnimVal and its DOM wrapper (if any) will have the same length!
    ClearAnimValue(aElement, aAttrEnum);
    return rv;
  }
  aElement->DidAnimateLengthList(aAttrEnum);
  return NS_OK;
}

void
SVGAnimatedLengthList::ClearAnimValue(nsSVGElement *aElement,
                                      uint32_t aAttrEnum)
{
  DOMSVGAnimatedLengthList *domWrapper =
    DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // When all animation ends, animVal simply mirrors baseVal, which may have
    // a different number of items to the last active animated value. We must
    // keep the length of our animVal's DOM wrapper list in sync, and again we
    // must do that before touching mAnimVal. See comments above.
    //
    domWrapper->InternalAnimValListWillChangeTo(mBaseVal);
  }
  mAnimVal = nullptr;
  aElement->DidAnimateLengthList(aAttrEnum);
}

nsISMILAttr*
SVGAnimatedLengthList::ToSMILAttr(nsSVGElement *aSVGElement,
                                  uint8_t aAttrEnum,
                                  uint8_t aAxis,
                                  bool aCanZeroPadList)
{
  return new SMILAnimatedLengthList(this, aSVGElement, aAttrEnum, aAxis, aCanZeroPadList);
}

nsresult
SVGAnimatedLengthList::
  SMILAnimatedLengthList::ValueFromString(const nsAString& aStr,
                               const dom::SVGAnimationElement* /*aSrcElement*/,
                               nsSMILValue& aValue,
                               bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SVGLengthListSMILType::sSingleton);
  SVGLengthListAndInfo *llai = static_cast<SVGLengthListAndInfo*>(val.mU.mPtr);
  nsresult rv = llai->SetValueFromString(aStr);
  if (NS_SUCCEEDED(rv)) {
    llai->SetInfo(mElement, mAxis, mCanZeroPadList);
    aValue = Move(val);

    // If any of the lengths in the list depend on their context, then we must
    // prevent caching of the entire animation sandwich. This is because the
    // units of a length at a given index can change from sandwich layer to
    // layer, and indeed even be different within a single sandwich layer. If
    // any length in the result of an animation sandwich is the result of the
    // addition of lengths where one or more of those lengths is context
    // dependent, then naturally the resultant length is also context
    // dependent, regardless of whether its actual unit is context dependent or
    // not. Unfortunately normal invalidation mechanisms won't cause us to
    // recalculate the result of the sandwich if the context changes, so we
    // take the (substantial) performance hit of preventing caching of the
    // sandwich layer, causing the animation sandwich to be recalculated every
    // single sample.

    aPreventCachingOfSandwich = false;
    for (uint32_t i = 0; i < llai->Length(); ++i) {
      uint8_t unit = (*llai)[i].GetUnit();
      if (unit == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
          unit == nsIDOMSVGLength::SVG_LENGTHTYPE_EMS ||
          unit == nsIDOMSVGLength::SVG_LENGTHTYPE_EXS) {
        aPreventCachingOfSandwich = true;
        break;
      }
    }
  }
  return rv;
}

nsSMILValue
SVGAnimatedLengthList::SMILAnimatedLengthList::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue val;

  nsSMILValue tmp(&SVGLengthListSMILType::sSingleton);
  SVGLengthListAndInfo *llai = static_cast<SVGLengthListAndInfo*>(tmp.mU.mPtr);
  nsresult rv = llai->CopyFrom(mVal->mBaseVal);
  if (NS_SUCCEEDED(rv)) {
    llai->SetInfo(mElement, mAxis, mCanZeroPadList);
    val = Move(tmp);
  }
  return val;
}

nsresult
SVGAnimatedLengthList::SMILAnimatedLengthList::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGLengthListSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGLengthListSMILType::sSingleton) {
    mVal->SetAnimValue(*static_cast<SVGLengthListAndInfo*>(aValue.mU.mPtr),
                       mElement,
                       mAttrEnum);
  }
  return NS_OK;
}

void
SVGAnimatedLengthList::SMILAnimatedLengthList::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement, mAttrEnum);
  }
}

} // namespace mozilla
