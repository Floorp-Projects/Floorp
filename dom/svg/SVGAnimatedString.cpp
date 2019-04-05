/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedString.h"

#include "mozilla/Move.h"
#include "mozilla/SMILValue.h"
#include "SMILStringType.h"
#include "SVGAttrTearoffTable.h"

using namespace mozilla::dom;

namespace mozilla {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(
    SVGAnimatedString::DOMAnimatedString, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedString::DOMAnimatedString)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedString::DOMAnimatedString)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedString::DOMAnimatedString)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static inline SVGAttrTearoffTable<SVGAnimatedString,
                                  SVGAnimatedString::DOMAnimatedString>&
SVGAnimatedStringTearoffTable() {
  static SVGAttrTearoffTable<SVGAnimatedString,
                             SVGAnimatedString::DOMAnimatedString>
      sSVGAnimatedStringTearoffTable;
  return sSVGAnimatedStringTearoffTable;
}

/* Implementation */

void SVGAnimatedString::SetBaseValue(const nsAString& aValue,
                                     SVGElement* aSVGElement, bool aDoSetAttr) {
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

void SVGAnimatedString::GetAnimValue(nsAString& aResult,
                                     const SVGElement* aSVGElement) const {
  if (mAnimVal) {
    aResult = *mAnimVal;
    return;
  }

  aSVGElement->GetStringBaseValue(mAttrEnum, aResult);
}

void SVGAnimatedString::SetAnimValue(const nsAString& aValue,
                                     SVGElement* aSVGElement) {
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

already_AddRefed<DOMSVGAnimatedString> SVGAnimatedString::ToDOMAnimatedString(
    SVGElement* aSVGElement) {
  RefPtr<DOMAnimatedString> domAnimatedString =
      SVGAnimatedStringTearoffTable().GetTearoff(this);
  if (!domAnimatedString) {
    domAnimatedString = new DOMAnimatedString(this, aSVGElement);
    SVGAnimatedStringTearoffTable().AddTearoff(this, domAnimatedString);
  }

  return domAnimatedString.forget();
}

SVGAnimatedString::DOMAnimatedString::~DOMAnimatedString() {
  SVGAnimatedStringTearoffTable().RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAnimatedString::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILString>(this, aSVGElement);
}

nsresult SVGAnimatedString::SMILString::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SMILValue val(SMILStringType::Singleton());

  *static_cast<nsAString*>(val.mU.mPtr) = aStr;
  aValue = std::move(val);
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

SMILValue SVGAnimatedString::SMILString::GetBaseValue() const {
  SMILValue val(SMILStringType::Singleton());
  mSVGElement->GetStringBaseValue(mVal->mAttrEnum,
                                  *static_cast<nsAString*>(val.mU.mPtr));
  return val;
}

void SVGAnimatedString::SMILString::ClearAnimValue() {
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nullptr;
    mSVGElement->DidAnimateString(mVal->mAttrEnum);
  }
}

nsresult SVGAnimatedString::SMILString::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILStringType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILStringType::Singleton()) {
    mVal->SetAnimValue(*static_cast<nsAString*>(aValue.mU.mPtr), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
