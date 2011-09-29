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

#include "nsSVGInteger.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "SMILIntegerType.h"
#endif // MOZ_SMIL

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
                                 nsSVGElement *aSVGElement,
                                 bool aDoSetAttr)
{
  PRInt32 value;

  nsresult rv = GetValueFromString(aValueAsString, &value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mIsBaseSet = PR_TRUE;
  mBaseVal = value;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  return NS_OK;
}

void
nsSVGInteger::GetBaseValueString(nsAString & aValueAsString)
{
  aValueAsString.Truncate();
  aValueAsString.AppendInt(mBaseVal);
}

void
nsSVGInteger::SetBaseValue(int aValue,
                           nsSVGElement *aSVGElement,
                           bool aDoSetAttr)
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
  aSVGElement->DidChangeInteger(mAttrEnum, aDoSetAttr);
}

void
nsSVGInteger::SetAnimValue(int aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = PR_TRUE;
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

#ifdef MOZ_SMIL
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
  aPreventCachingOfSandwich = PR_FALSE;
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
    mVal->SetAnimValue(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = PR_FALSE;
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
#endif // MOZ_SMIL
