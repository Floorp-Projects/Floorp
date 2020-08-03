/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedInteger.h"

#include "nsError.h"
#include "SMILIntegerType.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"

using namespace mozilla::dom;

namespace mozilla {

/* Implementation */

static SVGAttrTearoffTable<SVGAnimatedInteger,
                           SVGAnimatedInteger::DOMAnimatedInteger>
    sSVGAnimatedIntegerTearoffTable;

nsresult SVGAnimatedInteger::SetBaseValueString(const nsAString& aValueAsString,
                                                SVGElement* aSVGElement) {
  bool success;
  auto token = SVGContentUtils::GetAndEnsureOneToken(aValueAsString, success);

  if (!success) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  int32_t value;

  if (!SVGContentUtils::ParseInteger(token, value)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  mIsBaseSet = true;
  mBaseVal = value;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  return NS_OK;
}

void SVGAnimatedInteger::GetBaseValueString(nsAString& aValueAsString) {
  aValueAsString.Truncate();
  aValueAsString.AppendInt(mBaseVal);
}

void SVGAnimatedInteger::SetBaseValue(int aValue, SVGElement* aSVGElement) {
  // We can't just rely on SetParsedAttrValue (as called by DidChangeInteger)
  // detecting redundant changes since it will compare false if the existing
  // attribute value has an associated serialized version (a string value) even
  // if the integers match due to the way integers are stored in nsAttrValue.
  if (aValue == mBaseVal && mIsBaseSet) {
    return;
  }

  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeInteger(mAttrEnum);
}

void SVGAnimatedInteger::SetAnimValue(int aValue, SVGElement* aSVGElement) {
  if (mIsAnimated && aValue == mAnimVal) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateInteger(mAttrEnum);
}

already_AddRefed<DOMSVGAnimatedInteger>
SVGAnimatedInteger::ToDOMAnimatedInteger(SVGElement* aSVGElement) {
  RefPtr<DOMAnimatedInteger> domAnimatedInteger =
      sSVGAnimatedIntegerTearoffTable.GetTearoff(this);
  if (!domAnimatedInteger) {
    domAnimatedInteger = new DOMAnimatedInteger(this, aSVGElement);
    sSVGAnimatedIntegerTearoffTable.AddTearoff(this, domAnimatedInteger);
  }

  return domAnimatedInteger.forget();
}

SVGAnimatedInteger::DOMAnimatedInteger::~DOMAnimatedInteger() {
  sSVGAnimatedIntegerTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAnimatedInteger::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILInteger>(this, aSVGElement);
}

nsresult SVGAnimatedInteger::SMILInteger::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  int32_t val;

  if (!SVGContentUtils::ParseInteger(aStr, val)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  SMILValue smilVal(SMILIntegerType::Singleton());
  smilVal.mU.mInt = val;
  aValue = smilVal;
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

SMILValue SVGAnimatedInteger::SMILInteger::GetBaseValue() const {
  SMILValue val(SMILIntegerType::Singleton());
  val.mU.mInt = mVal->mBaseVal;
  return val;
}

void SVGAnimatedInteger::SMILInteger::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateInteger(mVal->mAttrEnum);
  }
}

nsresult SVGAnimatedInteger::SMILInteger::SetAnimValue(
    const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILIntegerType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILIntegerType::Singleton()) {
    mVal->SetAnimValue(int(aValue.mU.mInt), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
