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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "nsSVGClass.h"
#include "nsSVGStylableElement.h"
#include "nsSMILValue.h"
#include "SMILStringType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGClass::DOMAnimatedString, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGClass::DOMAnimatedString)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGClass::DOMAnimatedString)

DOMCI_DATA(SVGAnimatedClass, nsSVGClass::DOMAnimatedString)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGClass::DOMAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedString)
NS_INTERFACE_MAP_END

/* Implementation */

void
nsSVGClass::SetBaseValue(const nsAString& aValue,
                         nsSVGStylableElement *aSVGElement,
                         bool aDoSetAttr)
{
  NS_ASSERTION(aSVGElement, "Null element passed to SetBaseValue");

  aSVGElement->SetFlags(NODE_MAY_HAVE_CLASS);
  if (aDoSetAttr) {
    aSVGElement->SetAttr(kNameSpaceID_None, nsGkAtoms::_class, aValue, true);
  }
  if (mAnimVal) {
    aSVGElement->AnimationNeedsResample();
  }
}

void
nsSVGClass::GetBaseValue(nsAString& aValue, const nsSVGStylableElement *aSVGElement) const
{
  aSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aValue);
}

void
nsSVGClass::GetAnimValue(nsAString& aResult, const nsSVGStylableElement *aSVGElement) const
{
  if (mAnimVal) {
    aResult = *mAnimVal;
    return;
  }

  aSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aResult);
}

void
nsSVGClass::SetAnimValue(const nsAString& aValue, nsSVGStylableElement *aSVGElement)
{
  if (!mAnimVal) {
    mAnimVal = new nsString();
  }
  *mAnimVal = aValue;
  aSVGElement->SetFlags(NODE_MAY_HAVE_CLASS);
  aSVGElement->DidAnimateClass();
}

nsresult
nsSVGClass::ToDOMAnimatedString(nsIDOMSVGAnimatedString **aResult,
                                nsSVGStylableElement *aSVGElement)
{
  *aResult = new DOMAnimatedString(this, aSVGElement);
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGClass::DOMAnimatedString::GetAnimVal(nsAString& aResult)
{ 
  mSVGElement->FlushAnimations();
  mVal->GetAnimValue(aResult, mSVGElement);
  return NS_OK;
}

nsISMILAttr*
nsSVGClass::ToSMILAttr(nsSVGStylableElement *aSVGElement)
{
  return new SMILString(this, aSVGElement);
}

nsresult
nsSVGClass::SMILString::ValueFromString(const nsAString& aStr,
                                        const nsISMILAnimationElement* /*aSrcElement*/,
                                        nsSMILValue& aValue,
                                        bool& aPreventCachingOfSandwich) const
{
  nsSMILValue val(&SMILStringType::sSingleton);

  *static_cast<nsAString*>(val.mU.mPtr) = aStr;
  aValue.Swap(val);
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

nsSMILValue
nsSVGClass::SMILString::GetBaseValue() const
{
  nsSMILValue val(&SMILStringType::sSingleton);
  mSVGElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                       *static_cast<nsAString*>(val.mU.mPtr));
  return val;
}

void
nsSVGClass::SMILString::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nsnull;
    mSVGElement->DidAnimateClass();
  }
}

nsresult
nsSVGClass::SMILString::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILStringType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILStringType::sSingleton) {
    mVal->SetAnimValue(*static_cast<nsAString*>(aValue.mU.mPtr), mSVGElement);
  }
  return NS_OK;
}
