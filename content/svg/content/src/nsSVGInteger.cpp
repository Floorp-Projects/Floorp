/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMError.h"
#include "nsSVGInteger.h"
#include "nsSMILValue.h"
#include "SMILIntegerType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGInteger::DOMAnimatedInteger, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGInteger::DOMAnimatedInteger)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGInteger::DOMAnimatedInteger)

DOMCI_DATA(SVGAnimatedInteger, nsSVGInteger::DOMAnimatedInteger)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGInteger::DOMAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedInteger)
NS_INTERFACE_MAP_END

/* Implementation */

static nsresult
GetValueFromString(const nsAString &aValueAsString,
                   PRInt32 *aValue)
{
  NS_ConvertUTF16toUTF8 value(aValueAsString);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_DOM_SYNTAX_ERR;
  
  char *rest;
  *aValue = strtol(str, &rest, 10);
  if (rest == str || *rest != '\0') {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (*rest == '\0') {
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

nsresult
nsSVGInteger::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement)
{
  PRInt32 value;

  nsresult rv = GetValueFromString(aValueAsString, &value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mIsBaseSet = true;
  mBaseVal = value;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  return NS_OK;
}

void
nsSVGInteger::GetBaseValueString(nsAString & aValueAsString)
{
  aValueAsString.Truncate();
  aValueAsString.AppendInt(mBaseVal);
}

void
nsSVGInteger::SetBaseValue(int aValue, nsSVGElement *aSVGElement)
{
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
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeInteger(mAttrEnum);
}

void
nsSVGInteger::SetAnimValue(int aValue, nsSVGElement *aSVGElement)
{
  if (mIsAnimated && aValue == mAnimVal) {
    return;
  }
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateInteger(mAttrEnum);
}

nsresult
nsSVGInteger::ToDOMAnimatedInteger(nsIDOMSVGAnimatedInteger **aResult,
                                   nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedInteger(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsISMILAttr*
nsSVGInteger::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILInteger(this, aSVGElement);
}

nsresult
nsSVGInteger::SMILInteger::ValueFromString(const nsAString& aStr,
                                           const nsISMILAnimationElement* /*aSrcElement*/,
                                           nsSMILValue& aValue,
                                           bool& aPreventCachingOfSandwich) const
{
  PRInt32 val;

  nsresult rv = GetValueFromString(aStr, &val);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue smilVal(&SMILIntegerType::sSingleton);
  smilVal.mU.mInt = val;
  aValue = smilVal;
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

nsSMILValue
nsSVGInteger::SMILInteger::GetBaseValue() const
{
  nsSMILValue val(&SMILIntegerType::sSingleton);
  val.mU.mInt = mVal->mBaseVal;
  return val;
}

void
nsSVGInteger::SMILInteger::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimateInteger(mVal->mAttrEnum);
  }
}

nsresult
nsSVGInteger::SMILInteger::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILIntegerType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILIntegerType::sSingleton) {
    mVal->SetAnimValue(int(aValue.mU.mInt), mSVGElement);
  }
  return NS_OK;
}
