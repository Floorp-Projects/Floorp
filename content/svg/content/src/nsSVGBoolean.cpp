/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsSVGBoolean.h"
#include "nsSMILValue.h"
#include "SMILBoolType.h"
#include "SVGAnimatedBoolean.h"

using namespace mozilla;
using namespace mozilla::dom;

/* Implementation */

static nsSVGAttrTearoffTable<nsSVGBoolean, SVGAnimatedBoolean>
  sSVGAnimatedBooleanTearoffTable;

static nsresult
GetValueFromString(const nsAString &aValueAsString,
                   bool *aValue)
{
  if (aValueAsString.EqualsLiteral("true")) {
    *aValue = true;
    return NS_OK;
  }
  if (aValueAsString.EqualsLiteral("false")) {
    *aValue = false;
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

static nsresult
GetValueFromAtom(const nsIAtom* aValueAsAtom, bool *aValue)
{
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

nsresult
nsSVGBoolean::SetBaseValueAtom(const nsIAtom* aValue, nsSVGElement *aSVGElement)
{
  bool val;

  nsresult rv = GetValueFromAtom(aValue, &val);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mBaseVal = val;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.
  return NS_OK;
}

nsIAtom*
nsSVGBoolean::GetBaseValueAtom() const
{
  return mBaseVal ? nsGkAtoms::_true : nsGkAtoms::_false;
}

void
nsSVGBoolean::SetBaseValue(bool aValue, nsSVGElement *aSVGElement)
{
  NS_PRECONDITION(aValue == true || aValue == false, "Boolean out of range");

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

void
nsSVGBoolean::SetAnimValue(bool aValue, nsSVGElement *aSVGElement)
{
  if (mIsAnimated && mAnimVal == aValue) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateBoolean(mAttrEnum);
}

nsresult
nsSVGBoolean::ToDOMAnimatedBoolean(nsISupports **aResult,
                                   nsSVGElement *aSVGElement)
{
  nsRefPtr<SVGAnimatedBoolean> domAnimatedBoolean =
    sSVGAnimatedBooleanTearoffTable.GetTearoff(this);
  if (!domAnimatedBoolean) {
    domAnimatedBoolean = new SVGAnimatedBoolean(this, aSVGElement);
    sSVGAnimatedBooleanTearoffTable.AddTearoff(this, domAnimatedBoolean);
  }

  domAnimatedBoolean.forget(aResult);
  return NS_OK;
}

SVGAnimatedBoolean::~SVGAnimatedBoolean()
{
  sSVGAnimatedBooleanTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGBoolean::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILBool(this, aSVGElement);
}

nsresult
nsSVGBoolean::SMILBool::ValueFromString(const nsAString& aStr,
                                        const nsISMILAnimationElement* /*aSrcElement*/,
                                        nsSMILValue& aValue,
                                        bool& aPreventCachingOfSandwich) const
{
  bool value;
  nsresult rv = GetValueFromString(aStr, &value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue val(&SMILBoolType::sSingleton);
  val.mU.mBool = value;
  aValue = val;
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

nsSMILValue
nsSVGBoolean::SMILBool::GetBaseValue() const
{
  nsSMILValue val(&SMILBoolType::sSingleton);
  val.mU.mBool = mVal->mBaseVal;
  return val;
}

void
nsSVGBoolean::SMILBool::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateBoolean(mVal->mAttrEnum);
  }
}

nsresult
nsSVGBoolean::SMILBool::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILBoolType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILBoolType::sSingleton) {
    mVal->SetAnimValue(uint16_t(aValue.mU.mBool), mSVGElement);
  }
  return NS_OK;
}
