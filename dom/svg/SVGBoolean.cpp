/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGBoolean.h"

#include "nsError.h"
#include "SMILBoolType.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/SMILValue.h"
#include "mozilla/dom/SVGAnimatedBoolean.h"

using namespace mozilla::dom;

namespace mozilla {

/* Implementation */

//----------------------------------------------------------------------
// Helper class: AutoChangeBooleanNotifier
// Stack-based helper class to ensure DidChangeBoolean is called.
class MOZ_RAII AutoChangeBooleanNotifier {
 public:
  AutoChangeBooleanNotifier(SVGBoolean* aBoolean,
                            SVGElement* aSVGElement
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mBoolean(aBoolean), mSVGElement(aSVGElement) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    MOZ_ASSERT(mBoolean, "Expecting non-null boolean");
    MOZ_ASSERT(mSVGElement, "Expecting non-null element");
  }

  ~AutoChangeBooleanNotifier() {
    mSVGElement->DidChangeBoolean(mBoolean->mAttrEnum);
    if (mBoolean->mIsAnimated) {
      mSVGElement->AnimationNeedsResample();
    }
  }

 private:
  SVGBoolean* const mBoolean;
  SVGElement* const mSVGElement;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

static inline SVGAttrTearoffTable<SVGBoolean, SVGAnimatedBoolean>&
SVGAnimatedBooleanTearoffTable() {
  static SVGAttrTearoffTable<SVGBoolean, SVGAnimatedBoolean>
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

nsresult SVGBoolean::SetBaseValueAtom(const nsAtom* aValue,
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

nsAtom* SVGBoolean::GetBaseValueAtom() const {
  return mBaseVal ? nsGkAtoms::_true : nsGkAtoms::_false;
}

void SVGBoolean::SetBaseValue(bool aValue, SVGElement* aSVGElement) {
  if (aValue == mBaseVal) {
    return;
  }

  AutoChangeBooleanNotifier notifier(this, aSVGElement);

  mBaseVal = aValue;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
}

void SVGBoolean::SetAnimValue(bool aValue, SVGElement* aSVGElement) {
  if (mIsAnimated && mAnimVal == aValue) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateBoolean(mAttrEnum);
}

already_AddRefed<SVGAnimatedBoolean> SVGBoolean::ToDOMAnimatedBoolean(
    SVGElement* aSVGElement) {
  RefPtr<SVGAnimatedBoolean> domAnimatedBoolean =
      SVGAnimatedBooleanTearoffTable().GetTearoff(this);
  if (!domAnimatedBoolean) {
    domAnimatedBoolean = new SVGAnimatedBoolean(this, aSVGElement);
    SVGAnimatedBooleanTearoffTable().AddTearoff(this, domAnimatedBoolean);
  }

  return domAnimatedBoolean.forget();
}

SVGAnimatedBoolean::~SVGAnimatedBoolean() {
  SVGAnimatedBooleanTearoffTable().RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGBoolean::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILBool>(this, aSVGElement);
}

nsresult SVGBoolean::SMILBool::ValueFromString(
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

SMILValue SVGBoolean::SMILBool::GetBaseValue() const {
  SMILValue val(SMILBoolType::Singleton());
  val.mU.mBool = mVal->mBaseVal;
  return val;
}

void SVGBoolean::SMILBool::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateBoolean(mVal->mAttrEnum);
  }
}

nsresult SVGBoolean::SMILBool::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILBoolType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILBoolType::Singleton()) {
    mVal->SetAnimValue(uint16_t(aValue.mU.mBool), mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
