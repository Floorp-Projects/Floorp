/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGNumber2.h"
#include "nsSVGUtils.h"
#include "nsTextFormatter.h"
#include "prdtoa.h"
#include "nsMathUtils.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "nsSMILFloatType.h"
#endif // MOZ_SMIL

class DOMSVGNumber : public nsIDOMSVGNumber
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

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGNumber2::DOMAnimatedNumber, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGNumber2::DOMAnimatedNumber)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGNumber2::DOMAnimatedNumber)

NS_IMPL_ADDREF(DOMSVGNumber)
NS_IMPL_RELEASE(DOMSVGNumber)

DOMCI_DATA(SVGAnimatedNumber, nsSVGNumber2::DOMAnimatedNumber)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGNumber2::DOMAnimatedNumber)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedNumber)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedNumber)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN(DOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGNumber)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGNumber)
NS_INTERFACE_MAP_END

/* Implementation */

static nsresult
GetValueFromString(const nsAString &aValueAsString,
                   bool aPercentagesAllowed,
                   float *aValue)
{
  NS_ConvertUTF16toUTF8 value(aValueAsString);
  const char *str = value.get();

  if (NS_IsAsciiWhitespace(*str))
    return NS_ERROR_DOM_SYNTAX_ERR;
  
  char *rest;
  *aValue = float(PR_strtod(str, &rest));
  if (rest == str || !NS_finite(*aValue)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  if (*rest == '%' && aPercentagesAllowed) {
    *aValue /= 100;
    ++rest;
  }
  if (*rest == '\0') {
    return NS_OK;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

nsresult
nsSVGNumber2::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement)
{
  float val;

  nsresult rv = GetValueFromString(
    aValueAsString, aSVGElement->NumberAttrAllowsPercentage(mAttrEnum), &val);

  if (NS_FAILED(rv)) {
    return rv;
  }

  mBaseVal = val;
  mIsBaseSet = PR_TRUE;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
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
nsSVGNumber2::SetBaseValue(float aValue,
                           nsSVGElement *aSVGElement)
{
  mBaseVal = aValue;
  mIsBaseSet = PR_TRUE;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  aSVGElement->DidChangeNumber(mAttrEnum, true);
}

void
nsSVGNumber2::SetAnimValue(float aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = PR_TRUE;
  aSVGElement->DidAnimateNumber(mAttrEnum);
}

nsresult
nsSVGNumber2::ToDOMAnimatedNumber(nsIDOMSVGAnimatedNumber **aResult,
                                  nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedNumber(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

#ifdef MOZ_SMIL
nsISMILAttr*
nsSVGNumber2::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILNumber(this, aSVGElement);
}

nsresult
nsSVGNumber2::SMILNumber::ValueFromString(const nsAString& aStr,
                                          const nsISMILAnimationElement* /*aSrcElement*/,
                                          nsSMILValue& aValue,
                                          bool& aPreventCachingOfSandwich) const
{
  float value;

  nsresult rv = GetValueFromString(
    aStr, mSVGElement->NumberAttrAllowsPercentage(mVal->mAttrEnum), &value);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue val(&nsSMILFloatType::sSingleton);
  val.mU.mDouble = value;
  aValue = val;
  aPreventCachingOfSandwich = PR_FALSE;

  return NS_OK;
}

nsSMILValue
nsSVGNumber2::SMILNumber::GetBaseValue() const
{
  nsSMILValue val(&nsSMILFloatType::sSingleton);
  val.mU.mDouble = mVal->mBaseVal;
  return val;
}

void
nsSVGNumber2::SMILNumber::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->SetAnimValue(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = PR_FALSE;
  }
}

nsresult
nsSVGNumber2::SMILNumber::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &nsSMILFloatType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &nsSMILFloatType::sSingleton) {
    mVal->SetAnimValue(float(aValue.mU.mDouble), mSVGElement);
  }
  return NS_OK;
}
#endif // MOZ_SMIL
