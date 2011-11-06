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
 * The Initial Developer of the Original Code is Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "nsSVGBoolean.h"
#include "nsSMILValue.h"
#include "SMILBoolType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGBoolean::DOMAnimatedBoolean, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGBoolean::DOMAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGBoolean::DOMAnimatedBoolean)

DOMCI_DATA(SVGAnimatedBoolean, nsSVGBoolean::DOMAnimatedBoolean)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGBoolean::DOMAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedBoolean)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedBoolean)
NS_INTERFACE_MAP_END

/* Implementation */

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

nsresult
nsSVGBoolean::SetBaseValueString(const nsAString &aValueAsString,
                                 nsSVGElement *aSVGElement)
{
  bool val;

  nsresult rv = GetValueFromString(aValueAsString, &val);
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
  // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
  // which takes care of notifying.
  return NS_OK;
}

void
nsSVGBoolean::GetBaseValueString(nsAString & aValueAsString)
{
  aValueAsString.Assign(mBaseVal
                        ? NS_LITERAL_STRING("true")
                        : NS_LITERAL_STRING("false"));
}

void
nsSVGBoolean::SetBaseValue(bool aValue,
                           nsSVGElement *aSVGElement)
{
  NS_PRECONDITION(aValue == true || aValue == false, "Boolean out of range");

  if (aValue != mBaseVal) {
    mBaseVal = aValue;
    if (!mIsAnimated) {
      mAnimVal = mBaseVal;
    }
    else {
      aSVGElement->AnimationNeedsResample();
    }
    aSVGElement->DidChangeBoolean(mAttrEnum, true);
  }
}

void
nsSVGBoolean::SetAnimValue(bool aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = true;
  aSVGElement->DidAnimateBoolean(mAttrEnum);
}

nsresult
nsSVGBoolean::ToDOMAnimatedBoolean(nsIDOMSVGAnimatedBoolean **aResult,
                                   nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedBoolean(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
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
    mVal->SetAnimValue(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = false;
  }
}

nsresult
nsSVGBoolean::SMILBool::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILBoolType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILBoolType::sSingleton) {
    mVal->SetAnimValue(PRUint16(aValue.mU.mBool), mSVGElement);
  }
  return NS_OK;
}
