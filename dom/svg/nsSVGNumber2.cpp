/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGNumber2.h"
#include "mozilla/Attributes.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "nsSMILFloatType.h"
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

/* Implementation */

static nsSVGAttrTearoffTable<nsSVGNumber2, nsSVGNumber2::DOMAnimatedNumber>
  sSVGAnimatedNumberTearoffTable;

static bool
GetValueFromString(const nsAString& aString,
                   bool aPercentagesAllowed,
                   float& aValue)
{
  RangedPtr<const char16_t> iter =
    SVGContentUtils::GetStartRangedPtr(aString);
  const RangedPtr<const char16_t> end =
    SVGContentUtils::GetEndRangedPtr(aString);

  if (!SVGContentUtils::ParseNumber(iter, end, aValue)) {
    return false;
  }

  if (aPercentagesAllowed) {
    const nsAString& units = Substring(iter.get(), end.get());
    if (units.EqualsLiteral("%")) {
      aValue /= 100;
      return true;
    }
  }

  return iter == end;
}

nsresult
nsSVGNumber2::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement)
{
  float val;

  if (!GetValueFromString(aValueAsString,
                          aSVGElement->NumberAttrAllowsPercentage(mAttrEnum),
                          val)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  mBaseVal = val;
  mIsBaseSet = true;
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

void
nsSVGNumber2::GetBaseValueString(nsAString & aValueAsString)
{
  aValueAsString.Truncate();
  aValueAsString.AppendFloat(mBaseVal);
}

void
nsSVGNumber2::SetBaseValue(float aValue, nsSVGElement *aSVGElement)
{
  if (mIsBaseSet && aValue == mBaseVal) {
    return;
  }

  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeNumber(mAttrEnum);
}

void
nsSVGNumber2::SetAnimValue(float aValue, nsSVGElement *aSVGElement)
{
  if (mIsAnimated && aValue == mAnimVal) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateNumber(mAttrEnum);
}

already_AddRefed<SVGAnimatedNumber>
nsSVGNumber2::ToDOMAnimatedNumber(nsSVGElement* aSVGElement)
{
  nsRefPtr<DOMAnimatedNumber> domAnimatedNumber =
    sSVGAnimatedNumberTearoffTable.GetTearoff(this);
  if (!domAnimatedNumber) {
    domAnimatedNumber = new DOMAnimatedNumber(this, aSVGElement);
    sSVGAnimatedNumberTearoffTable.AddTearoff(this, domAnimatedNumber);
  }

  return domAnimatedNumber.forget();
}

nsSVGNumber2::DOMAnimatedNumber::~DOMAnimatedNumber()
{
  sSVGAnimatedNumberTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
nsSVGNumber2::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILNumber(this, aSVGElement);
}

nsresult
nsSVGNumber2::SMILNumber::ValueFromString(const nsAString& aStr,
                                          const mozilla::dom::SVGAnimationElement* /*aSrcElement*/,
                                          nsSMILValue& aValue,
                                          bool& aPreventCachingOfSandwich) const
{
  float value;

  if (!GetValueFromString(aStr,
                          mSVGElement->NumberAttrAllowsPercentage(mVal->mAttrEnum),
                          value)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsSMILValue val(nsSMILFloatType::Singleton());
  val.mU.mDouble = value;
  aValue = val;
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

nsSMILValue
nsSVGNumber2::SMILNumber::GetBaseValue() const
{
  nsSMILValue val(nsSMILFloatType::Singleton());
  val.mU.mDouble = mVal->mBaseVal;
  return val;
}

void
nsSVGNumber2::SMILNumber::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateNumber(mVal->mAttrEnum);
  }
}

nsresult
nsSVGNumber2::SMILNumber::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == nsSMILFloatType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == nsSMILFloatType::Singleton()) {
    mVal->SetAnimValue(float(aValue.mU.mDouble), mSVGElement);
  }
  return NS_OK;
}
