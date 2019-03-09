/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGInteger.h"

#include "nsError.h"
#include "SMILIntegerType.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"

using namespace mozilla::dom;

namespace mozilla {

/* Implementation */

//----------------------------------------------------------------------
// Helper class: AutoChangeIntegerNotifier
// Stack-based helper class ensure DidChangeInteger is called.
class MOZ_RAII AutoChangeIntegerNotifier {
 public:
  AutoChangeIntegerNotifier(SVGInteger *aInteger,
                            SVGElement *aSVGElement
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mInteger(aInteger), mSVGElement(aSVGElement) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mInteger, "Expecting non-null integer");
    MOZ_ASSERT(mSVGElement, "Expecting non-null element");
  }

  ~AutoChangeIntegerNotifier() {
    mSVGElement->DidChangeInteger(mInteger->mAttrEnum);
    if (mInteger->mIsAnimated) {
      mSVGElement->AnimationNeedsResample();
    }
  }

 private:
  SVGInteger *const mInteger;
  SVGElement *const mSVGElement;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

static SVGAttrTearoffTable<SVGInteger, SVGInteger::DOMAnimatedInteger>
    sSVGAnimatedIntegerTearoffTable;

nsresult SVGInteger::SetBaseValueString(const nsAString &aValueAsString,
                                        SVGElement *aSVGElement) {
  int32_t value;

  if (!SVGContentUtils::ParseInteger(aValueAsString, value)) {
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

void SVGInteger::GetBaseValueString(nsAString &aValueAsString) {
  aValueAsString.Truncate();
  aValueAsString.AppendInt(mBaseVal);
}

void SVGInteger::SetBaseValue(int aValue, SVGElement *aSVGElement) {
  // We can't just rely on SetParsedAttrValue (as called by DidChangeInteger)
  // detecting redundant changes since it will compare false if the existing
  // attribute value has an associated serialized version (a string value) even
  // if the integers match due to the way integers are stored in nsAttrValue.
  if (aValue == mBaseVal && mIsBaseSet) {
    return;
  }

  AutoChangeIntegerNotifier notifier(this, aSVGElement);

  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
}

void SVGInteger::SetAnimValue(int aValue, SVGElement *aSVGElement) {
  if (mIsAnimated && aValue == mAnimVal) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateInteger(mAttrEnum);
}

already_AddRefed<SVGAnimatedInteger> SVGInteger::ToDOMAnimatedInteger(
    SVGElement *aSVGElement) {
  RefPtr<DOMAnimatedInteger> domAnimatedInteger =
      sSVGAnimatedIntegerTearoffTable.GetTearoff(this);
  if (!domAnimatedInteger) {
    domAnimatedInteger = new DOMAnimatedInteger(this, aSVGElement);
    sSVGAnimatedIntegerTearoffTable.AddTearoff(this, domAnimatedInteger);
  }

  return domAnimatedInteger.forget();
}

SVGInteger::DOMAnimatedInteger::~DOMAnimatedInteger() {
  sSVGAnimatedIntegerTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGInteger::ToSMILAttr(SVGElement *aSVGElement) {
  return MakeUnique<SMILInteger>(this, aSVGElement);
}

nsresult SVGInteger::SMILInteger::ValueFromString(
    const nsAString &aStr, const dom::SVGAnimationElement * /*aSrcElement*/,
    SMILValue &aValue, bool &aPreventCachingOfSandwich) const {
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

SMILValue SVGInteger::SMILInteger::GetBaseValue() const {
  SMILValue val(SMILIntegerType::Singleton());
  val.mU.mInt = mVal->mBaseVal;
  return val;
}

void SVGInteger::SMILInteger::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateInteger(mVal->mAttrEnum);
  }
}

nsresult SVGInteger::SMILInteger::SetAnimValue(const SMILValue &aValue) {
  NS_ASSERTION(aValue.mType == SMILIntegerType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILIntegerType::Singleton()) {
    mVal->SetAnimValue(int(aValue.mU.mInt), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
