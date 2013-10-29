/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGNumber2.h"
#include "mozilla/Attributes.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "nsIDOMSVGNumber.h"
#include "nsSMILFloatType.h"
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

class DOMSVGNumber MOZ_FINAL : public nsIDOMSVGNumber
{
public:
  NS_DECL_ISUPPORTS

  DOMSVGNumber() 
    : mVal(0) {}
    
  NS_IMETHOD GetValue(float* aResult)
    { *aResult = mVal; return NS_OK; }
  NS_IMETHOD SetValue(float aValue)
    { NS_ENSURE_FINITE(aValue, NS_ERROR_ILLEGAL_VALUE);
      mVal = aValue;
      return NS_OK; }

private:
  float mVal;
};

NS_IMPL_ADDREF(DOMSVGNumber)
NS_IMPL_RELEASE(DOMSVGNumber)

NS_INTERFACE_MAP_BEGIN(DOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGNumber)
NS_INTERFACE_MAP_END

/* Implementation */

static nsSVGAttrTearoffTable<nsSVGNumber2, nsSVGNumber2::DOMAnimatedNumber>
  sSVGAnimatedNumberTearoffTable;

static bool
GetValueFromString(const nsAString& aValueAsString,
                   bool aPercentagesAllowed,
                   float& aValue)
{
  nsAutoString units;

  if (!SVGContentUtils::ParseNumber(aValueAsString, aValue, units)) {
    return false;
  }

  if (aPercentagesAllowed && units.EqualsLiteral("%")) {
    aValue /= 100;
    return true;
  }

  return units.IsEmpty();
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
