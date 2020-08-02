/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedBoolean.h"

#include "DOMSVGAnimatedBoolean.h"
#include "nsError.h"
#include "SMILBoolType.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/SMILValue.h"

using namespace mozilla::dom;

namespace mozilla {

/* Implementation */

static inline SVGAttrTearoffTable<SVGAnimatedBoolean, DOMSVGAnimatedBoolean>&
SVGAnimatedBooleanTearoffTable() {
  static SVGAttrTearoffTable<SVGAnimatedBoolean, DOMSVGAnimatedBoolean>
      sSVGAnimatedBooleanTearoffTable;
  return sSVGAnimatedBooleanTearoffTable;
}

static bool GetValueFromString(const nsAString& aValueAsString, bool& aValue) {
  if (aValueAsString.EqualsLiteral("true")) {
    aValue = true;
    return true;
  }
  if (aValueAsString.EqualsLiteral("false")) {
    aValue = false;
    return true;
  }
  return false;
}

static nsresult GetValueFromAtom(const nsAtom* aValueAsAtom, bool* aValue) {
  if (aValueAsAtom == nsGkAtoms::_true) {
    *aValue = true;
    return NS_OK;
  }
  if (aValueAsAtom == nsGkAtoms::_false) {
    *aValue = false;
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

nsresult SVGAnimatedBoolean::SetBaseValueAtom(const nsAtom* aValue,
                                              SVGElement* aSVGElement) {
  bool val = false;

  nsresult rv = GetValueFromAtom(aValue, &val);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mBaseVal = val;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }

  // We don't need to call DidChange* here - we're only called by
  // SVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.
  return NS_OK;
}

nsAtom* SVGAnimatedBoolean::GetBaseValueAtom() const {
  return mBaseVal ? nsGkAtoms::_true : nsGkAtoms::_false;
}

void SVGAnimatedBoolean::SetBaseValue(bool aValue, SVGElement* aSVGElement) {
  if (aValue == mBaseVal) {
    return;
  }

  mBaseVal = aValue;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  } else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeBoolean(mAttrEnum);
}

void SVGAnimatedBoolean::SetAnimValue(bool aValue, SVGElement* aSVGElement) {
  if (mIsAnimated && mAnimVal == aValue) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateBoolean(mAttrEnum);
}

already_AddRefed<DOMSVGAnimatedBoolean>
SVGAnimatedBoolean::ToDOMAnimatedBoolean(SVGElement* aSVGElement) {
  RefPtr<DOMSVGAnimatedBoolean> domAnimatedBoolean =
      SVGAnimatedBooleanTearoffTable().GetTearoff(this);
  if (!domAnimatedBoolean) {
    domAnimatedBoolean = new DOMSVGAnimatedBoolean(this, aSVGElement);
    SVGAnimatedBooleanTearoffTable().AddTearoff(this, domAnimatedBoolean);
  }

  return domAnimatedBoolean.forget();
}

DOMSVGAnimatedBoolean::~DOMSVGAnimatedBoolean() {
  SVGAnimatedBooleanTearoffTable().RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAnimatedBoolean::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILBool>(this, aSVGElement);
}

nsresult SVGAnimatedBoolean::SMILBool::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  bool value;
  if (!GetValueFromString(aStr, value)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  SMILValue val(SMILBoolType::Singleton());
  val.mU.mBool = value;
  aValue = val;
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

SMILValue SVGAnimatedBoolean::SMILBool::GetBaseValue() const {
  SMILValue val(SMILBoolType::Singleton());
  val.mU.mBool = mVal->mBaseVal;
  return val;
}

void SVGAnimatedBoolean::SMILBool::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateBoolean(mVal->mAttrEnum);
  }
}

nsresult SVGAnimatedBoolean::SMILBool::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILBoolType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILBoolType::Singleton()) {
    mVal->SetAnimValue(uint16_t(aValue.mU.mBool), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
