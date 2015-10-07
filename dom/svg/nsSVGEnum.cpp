/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsSVGEnum.h"
#include "nsIAtom.h"
#include "nsSVGElement.h"
#include "nsSMILValue.h"
#include "SMILEnumType.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsSVGAttrTearoffTable<nsSVGEnum, nsSVGEnum::DOMAnimatedEnum>
  sSVGAnimatedEnumTearoffTable;

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
        // nsSVGElement::ParseAttribute under Element::SetAttr,
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
nsSVGEnum::SetBaseValue(uint16_t aValue,
                        nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (mapping->mVal == aValue) {
      mIsBaseSet = true;
      if (mBaseVal != uint8_t(aValue)) {
        mBaseVal = uint8_t(aValue);
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
nsSVGEnum::SetAnimValue(uint16_t aValue, nsSVGElement *aSVGElement)
{
  if (mIsAnimated && aValue == mAnimVal) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateEnum(mAttrEnum);
}

already_AddRefed<SVGAnimatedEnumeration>
nsSVGEnum::ToDOMAnimatedEnum(nsSVGElement* aSVGElement)
{
  nsRefPtr<DOMAnimatedEnum> domAnimatedEnum =
    sSVGAnimatedEnumTearoffTable.GetTearoff(this);
  if (!domAnimatedEnum) {
    domAnimatedEnum = new DOMAnimatedEnum(this, aSVGElement);
    sSVGAnimatedEnumTearoffTable.AddTearoff(this, domAnimatedEnum);
  }

  return domAnimatedEnum.forget();
}

nsSVGEnum::DOMAnimatedEnum::~DOMAnimatedEnum()
{
  sSVGAnimatedEnumTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGEnum::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILEnum(this, aSVGElement);
}

nsresult
nsSVGEnum::SMILEnum::ValueFromString(const nsAString& aStr,
                                     const dom::SVGAnimationElement* /*aSrcElement*/,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const
{
  nsIAtom *valAtom = NS_GetStaticAtom(aStr);
  if (valAtom) {
    nsSVGEnumMapping *mapping = mVal->GetMapping(mSVGElement);

    while (mapping && mapping->mKey) {
      if (valAtom == *(mapping->mKey)) {
        nsSMILValue val(SMILEnumType::Singleton());
        val.mU.mUint = mapping->mVal;
        aValue = val;
        aPreventCachingOfSandwich = false;
        return NS_OK;
      }
      mapping++;
    }
  }
  
  // only a warning since authors may mistype attribute values
  NS_WARNING("unknown enumeration key");
  return NS_ERROR_FAILURE;
}

nsSMILValue
nsSVGEnum::SMILEnum::GetBaseValue() const
{
  nsSMILValue val(SMILEnumType::Singleton());
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
  NS_ASSERTION(aValue.mType == SMILEnumType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILEnumType::Singleton()) {
    MOZ_ASSERT(aValue.mU.mUint <= USHRT_MAX,
               "Very large enumerated value - too big for uint16_t");
    mVal->SetAnimValue(uint16_t(aValue.mU.mUint), mSVGElement);
  }
  return NS_OK;
}
