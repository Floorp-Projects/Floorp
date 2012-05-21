/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGEnum.h"
#include "nsIAtom.h"
#include "nsSVGElement.h"
#include "nsSMILValue.h"
#include "SMILEnumType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGEnum::DOMAnimatedEnum, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGEnum::DOMAnimatedEnum)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGEnum::DOMAnimatedEnum)

DOMCI_DATA(SVGAnimatedEnumeration, nsSVGEnum::DOMAnimatedEnum)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGEnum::DOMAnimatedEnum)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedEnumeration)
NS_INTERFACE_MAP_END

nsSVGEnumMapping *
nsSVGEnum::GetMapping(nsSVGElement *aSVGElement)
{
  nsSVGElement::EnumAttributesInfo info = aSVGElement->GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0 && mAttrEnum < info.mEnumCount,
               "mapping request for a non-attrib enum");

  return info.mEnumInfo[mAttrEnum].mMapping;
}

nsresult
nsSVGEnum::SetBaseValueAtom(const nsIAtom* aValue, nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (aValue == *(mapping->mKey)) {
      mIsBaseSet = true;
      if (mBaseVal != mapping->mVal) {
        mBaseVal = mapping->mVal;
        if (!mIsAnimated) {
          mAnimVal = mBaseVal;
        }
        else {
          aSVGElement->AnimationNeedsResample();
        }
        // We don't need to call DidChange* here - we're only called by
        // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
        // which takes care of notifying.
      }
      return NS_OK;
    }
    mapping++;
  }

  // only a warning since authors may mistype attribute values
  NS_WARNING("unknown enumeration key");
  return NS_ERROR_DOM_SYNTAX_ERR;
}

nsIAtom*
nsSVGEnum::GetBaseValueAtom(nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (mBaseVal == mapping->mVal) {
      return *mapping->mKey;
    }
    mapping++;
  }
  NS_ERROR("unknown enumeration value");
  return nsGkAtoms::_empty;
}

nsresult
nsSVGEnum::SetBaseValue(PRUint16 aValue,
                        nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (mapping->mVal == aValue) {
      mIsBaseSet = true;
      if (mBaseVal != PRUint8(aValue)) {
        mBaseVal = PRUint8(aValue);
        if (!mIsAnimated) {
          mAnimVal = mBaseVal;
        }
        else {
          aSVGElement->AnimationNeedsResample();
        }
        aSVGElement->DidChangeEnum(mAttrEnum);
      }
      return NS_OK;
    }
    mapping++;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

void
nsSVGEnum::SetAnimValue(PRUint16 aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateEnum(mAttrEnum);
}

nsresult
nsSVGEnum::ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                             nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedEnum(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsISMILAttr*
nsSVGEnum::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILEnum(this, aSVGElement);
}

nsresult
nsSVGEnum::SMILEnum::ValueFromString(const nsAString& aStr,
                                     const nsISMILAnimationElement* /*aSrcElement*/,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const
{
  nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aStr);
  nsSVGEnumMapping *mapping = mVal->GetMapping(mSVGElement);

  while (mapping && mapping->mKey) {
    if (valAtom == *(mapping->mKey)) {
      nsSMILValue val(&SMILEnumType::sSingleton);
      val.mU.mUint = mapping->mVal;
      aValue = val;
      aPreventCachingOfSandwich = false;
      return NS_OK;
    }
    mapping++;
  }
  
  // only a warning since authors may mistype attribute values
  NS_WARNING("unknown enumeration key");
  return NS_ERROR_FAILURE;
}

nsSMILValue
nsSVGEnum::SMILEnum::GetBaseValue() const
{
  nsSMILValue val(&SMILEnumType::sSingleton);
  val.mU.mUint = mVal->mBaseVal;
  return val;
}

void
nsSVGEnum::SMILEnum::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateEnum(mVal->mAttrEnum);
  }
}

nsresult
nsSVGEnum::SMILEnum::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILEnumType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILEnumType::sSingleton) {
    NS_ABORT_IF_FALSE(aValue.mU.mUint <= USHRT_MAX,
                      "Very large enumerated value - too big for PRUint16");
    mVal->SetAnimValue(PRUint16(aValue.mU.mUint), mSVGElement);
  }
  return NS_OK;
}
