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

#include "SVGAnimatedPathSegList.h"
#include "DOMSVGPathSegList.h"
#include "nsSVGElement.h"
#include "nsSVGAttrTearoffTable.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "SVGPathSegListSMILType.h"
#endif // MOZ_SMIL

// See the comments in this file's header!

namespace mozilla {

nsresult
SVGAnimatedPathSegList::SetBaseValueString(const nsAString& aValue)
{
  SVGPathData newBaseValue;

  // The spec says that the path data is parsed and accepted up to the first
  // error encountered, so we don't return early if an error occurs. However,
  // we do want to throw any error code from setAttribute if there's a problem.

  nsresult rv = newBaseValue.SetValueFromString(aValue);

  // We must send these notifications *before* changing mBaseVal! Our baseVal's
  // DOM wrapper list may have to remove DOM items from itself, and any removed
  // DOM items need to copy their internal counterpart's values *before* we
  // change them. See the comments in
  // DOMSVGPathSegList::InternalListWillChangeTo().

  DOMSVGPathSegList *baseValWrapper =
    DOMSVGPathSegList::GetDOMWrapperIfExists(GetBaseValKey());
  if (baseValWrapper) {
    baseValWrapper->InternalListWillChangeTo(newBaseValue);
  }

  DOMSVGPathSegList* animValWrapper = nsnull;
  if (!IsAnimating()) {  // DOM anim val wraps our base val too!
    animValWrapper = DOMSVGPathSegList::GetDOMWrapperIfExists(GetAnimValKey());
    if (animValWrapper) {
      animValWrapper->InternalListWillChangeTo(newBaseValue);
    }
  }

  // Only now may we modify mBaseVal!

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
  // which takes care of notifying.

  nsresult rv2 = mBaseVal.CopyFrom(newBaseValue);
  if (NS_FAILED(rv2)) {
    // Attempting to increase mBaseVal's length failed (mBaseVal is left
    // unmodified). We MUST keep any DOM wrappers in sync:
    if (baseValWrapper) {
      baseValWrapper->InternalListWillChangeTo(mBaseVal);
    }
    if (animValWrapper) {
      animValWrapper->InternalListWillChangeTo(mBaseVal);
    }
    return rv2;
  }
  return rv;
}

void
SVGAnimatedPathSegList::ClearBaseValue()
{
  // We must send these notifications *before* changing mBaseVal! (See above.)

  DOMSVGPathSegList *baseValWrapper =
    DOMSVGPathSegList::GetDOMWrapperIfExists(GetBaseValKey());
  if (baseValWrapper) {
    baseValWrapper->InternalListWillChangeTo(SVGPathData());
  }

  if (!IsAnimating()) { // DOM anim val wraps our base val too!
    DOMSVGPathSegList *animValWrapper =
      DOMSVGPathSegList::GetDOMWrapperIfExists(GetAnimValKey());
    if (animValWrapper) {
      animValWrapper->InternalListWillChangeTo(SVGPathData());
    }
  }

  mBaseVal.Clear();
  // Caller notifies
}

nsresult
SVGAnimatedPathSegList::SetAnimValue(const SVGPathData& aNewAnimValue,
                                     nsSVGElement *aElement)
{
  // Note that a new animation may totally change the number of items in the
  // animVal list, either replacing what was essentially a mirror of the
  // baseVal list, or else replacing and overriding an existing animation.
  // Unfortunately it is not possible for us to reliably distinguish between
  // calls to this method that are setting a new sample for an existing
  // animation, and calls that are setting the first sample of an animation
  // that will override an existing animation. In the case of DOMSVGPathSegList
  // the InternalListWillChangeTo method is not virtually free as it is for the
  // other DOM list classes, so this is a shame. We'd quite like to be able to
  // skip the call if possible.

  // We must send these notifications *before* changing mAnimVal! (See above.)

  DOMSVGPathSegList *domWrapper =
    DOMSVGPathSegList::GetDOMWrapperIfExists(GetAnimValKey());
  if (domWrapper) {
    domWrapper->InternalListWillChangeTo(aNewAnimValue);
  }
  if (!mAnimVal) {
    mAnimVal = new SVGPathData();
  }
  nsresult rv = mAnimVal->CopyFrom(aNewAnimValue);
  if (NS_FAILED(rv)) {
    // OOM. We clear the animation and, importantly, ClearAnimValue() ensures
    // that mAnimVal's DOM wrapper (if any) is kept in sync!
    ClearAnimValue(aElement);
  }
  aElement->DidAnimatePathSegList();
  return rv;
}

void
SVGAnimatedPathSegList::ClearAnimValue(nsSVGElement *aElement)
{
  // We must send these notifications *before* changing mAnimVal! (See above.)

  DOMSVGPathSegList *domWrapper =
    DOMSVGPathSegList::GetDOMWrapperIfExists(GetAnimValKey());
  if (domWrapper) {
    // When all animation ends, animVal simply mirrors baseVal, which may have
    // a different number of items to the last active animated value.
    //
    domWrapper->InternalListWillChangeTo(mBaseVal);
  }
  mAnimVal = nsnull;
  aElement->DidAnimatePathSegList();
}

#ifdef MOZ_SMIL
nsISMILAttr*
SVGAnimatedPathSegList::ToSMILAttr(nsSVGElement *aElement)
{
  return new SMILAnimatedPathSegList(this, aElement);
}

nsresult
SVGAnimatedPathSegList::
  SMILAnimatedPathSegList::ValueFromString(const nsAString& aStr,
                               const nsISMILAnimationElement* /*aSrcElement*/,
                               nsSMILValue& aValue,
                               bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SVGPathSegListSMILType::sSingleton);
  SVGPathDataAndOwner *list = static_cast<SVGPathDataAndOwner*>(val.mU.mPtr);
  nsresult rv = list->SetValueFromString(aStr);
  if (NS_SUCCEEDED(rv)) {
    list->SetElement(mElement);
    aValue.Swap(val);
  }
  aPreventCachingOfSandwich = PR_FALSE;
  return rv;
}

nsSMILValue
SVGAnimatedPathSegList::SMILAnimatedPathSegList::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue val;

  nsSMILValue tmp(&SVGPathSegListSMILType::sSingleton);
  SVGPathDataAndOwner *list = static_cast<SVGPathDataAndOwner*>(tmp.mU.mPtr);
  nsresult rv = list->CopyFrom(mVal->mBaseVal);
  if (NS_SUCCEEDED(rv)) {
    list->SetElement(mElement);
    val.Swap(tmp);
  }
  return val;
}

nsresult
SVGAnimatedPathSegList::SMILAnimatedPathSegList::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGPathSegListSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGPathSegListSMILType::sSingleton) {
    mVal->SetAnimValue(*static_cast<SVGPathDataAndOwner*>(aValue.mU.mPtr),
                       mElement);
  }
  return NS_OK;
}

void
SVGAnimatedPathSegList::SMILAnimatedPathSegList::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement);
  }
}
#endif // MOZ_SMIL

} // namespace mozilla
