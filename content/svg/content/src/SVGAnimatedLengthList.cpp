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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "nsSVGAttrTearoffTable.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "SVGLengthListSMILType.h"
#endif // MOZ_SMIL

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
  // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
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
SVGAnimatedLengthList::ClearBaseValue(PRUint32 aAttrEnum)
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
                                    PRUint32 aAttrEnum)
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
                                      PRUint32 aAttrEnum)
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
  mAnimVal = nsnull;
  aElement->DidAnimateLengthList(aAttrEnum);
}

#ifdef MOZ_SMIL
nsISMILAttr*
SVGAnimatedLengthList::ToSMILAttr(nsSVGElement *aSVGElement,
                                  PRUint8 aAttrEnum,
                                  PRUint8 aAxis,
                                  bool aCanZeroPadList)
{
  return new SMILAnimatedLengthList(this, aSVGElement, aAttrEnum, aAxis, aCanZeroPadList);
}

nsresult
SVGAnimatedLengthList::
  SMILAnimatedLengthList::ValueFromString(const nsAString& aStr,
                               const nsISMILAnimationElement* /*aSrcElement*/,
                               nsSMILValue& aValue,
                               bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SVGLengthListSMILType::sSingleton);
  SVGLengthListAndInfo *llai = static_cast<SVGLengthListAndInfo*>(val.mU.mPtr);
  nsresult rv = llai->SetValueFromString(aStr);
  if (NS_SUCCEEDED(rv)) {
    llai->SetInfo(mElement, mAxis, mCanZeroPadList);
    aValue.Swap(val);

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

    aPreventCachingOfSandwich = PR_FALSE;
    for (PRUint32 i = 0; i < llai->Length(); ++i) {
      PRUint8 unit = (*llai)[i].GetUnit();
      if (unit == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
          unit == nsIDOMSVGLength::SVG_LENGTHTYPE_EMS ||
          unit == nsIDOMSVGLength::SVG_LENGTHTYPE_EXS) {
        aPreventCachingOfSandwich = PR_TRUE;
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
    val.Swap(tmp);
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
#endif // MOZ_SMIL

} // namespace mozilla
