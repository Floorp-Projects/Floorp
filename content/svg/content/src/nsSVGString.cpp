/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGString.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsSMILValue.h"
#include "SMILStringType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGString::DOMAnimatedString, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGString::DOMAnimatedString)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGString::DOMAnimatedString)

DOMCI_DATA(SVGAnimatedString, nsSVGString::DOMAnimatedString)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGString::DOMAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedString)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedString)
NS_INTERFACE_MAP_END

static nsSVGAttrTearoffTable<nsSVGString, nsSVGString::DOMAnimatedString>
  sSVGAnimatedStringTearoffTable;

/* Implementation */

void
nsSVGString::SetBaseValue(const nsAString& aValue,
                          nsSVGElement *aSVGElement,
                          bool aDoSetAttr)
{
  NS_ASSERTION(aSVGElement, "Null element passed to SetBaseValue");

  mIsBaseSet = true;
  if (aDoSetAttr) {
    aSVGElement->SetStringBaseValue(mAttrEnum, aValue);
  }
  if (mAnimVal) {
    aSVGElement->AnimationNeedsResample();
  }

  aSVGElement->DidChangeString(mAttrEnum);
}

void
nsSVGString::GetAnimValue(nsAString& aResult, const nsSVGElement *aSVGElement) const
{
  if (mAnimVal) {
    aResult = *mAnimVal;
    return;
  }

  aSVGElement->GetStringBaseValue(mAttrEnum, aResult);
}

void
nsSVGString::SetAnimValue(const nsAString& aValue, nsSVGElement *aSVGElement)
{
  if (aSVGElement->IsStringAnimatable(mAttrEnum)) {
    if (mAnimVal && mAnimVal->Equals(aValue)) {
      return;
    }
    if (!mAnimVal) {
      mAnimVal = new nsString();
    }
    *mAnimVal = aValue;
    aSVGElement->DidAnimateString(mAttrEnum);
  }
}

nsresult
nsSVGString::ToDOMAnimatedString(nsIDOMSVGAnimatedString **aResult,
                                 nsSVGElement *aSVGElement)
{
  *aResult = ToDOMAnimatedString(aSVGElement).get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedString>
nsSVGString::ToDOMAnimatedString(nsSVGElement* aSVGElement)
{
  nsRefPtr<DOMAnimatedString> domAnimatedString =
    sSVGAnimatedStringTearoffTable.GetTearoff(this);
  if (!domAnimatedString) {
    domAnimatedString = new DOMAnimatedString(this, aSVGElement);
    sSVGAnimatedStringTearoffTable.AddTearoff(this, domAnimatedString);
  }

  return domAnimatedString.forget();
}

nsSVGString::DOMAnimatedString::~DOMAnimatedString()
{
  sSVGAnimatedStringTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGString::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILString(this, aSVGElement);
}

nsresult
nsSVGString::SMILString::ValueFromString(const nsAString& aStr,
                                         const dom::SVGAnimationElement* /*aSrcElement*/,
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
nsSVGString::SMILString::GetBaseValue() const
{
  nsSMILValue val(&SMILStringType::sSingleton);
  mSVGElement->GetStringBaseValue(mVal->mAttrEnum, *static_cast<nsAString*>(val.mU.mPtr));
  return val;
}

void
nsSVGString::SMILString::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nullptr;
    mSVGElement->DidAnimateString(mVal->mAttrEnum);
  }
}

nsresult
nsSVGString::SMILString::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILStringType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILStringType::sSingleton) {
    mVal->SetAnimValue(*static_cast<nsAString*>(aValue.mU.mPtr), mSVGElement);
  }
  return NS_OK;
}
