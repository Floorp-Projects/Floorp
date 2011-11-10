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
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "nsSVGIntegerPair.h"
#include "nsSVGUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsDOMError.h"
#include "nsMathUtils.h"
#include "nsSMILValue.h"
#include "SVGIntegerPairSMILType.h"

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGIntegerPair::DOMAnimatedInteger, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGIntegerPair::DOMAnimatedInteger)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGIntegerPair::DOMAnimatedInteger)

DOMCI_DATA(SVGAnimatedIntegerPair, nsSVGIntegerPair::DOMAnimatedInteger)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGIntegerPair::DOMAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedInteger)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedInteger)
NS_INTERFACE_MAP_END

/* Implementation */

static nsresult
ParseIntegerOptionalInteger(const nsAString& aValue,
                            PRInt32 aValues[2])
{
  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aValue, ',',
              nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
  if (tokenizer.firstTokenBeganWithWhitespace()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  PRUint32 i;
  for (i = 0; i < 2 && tokenizer.hasMoreTokens(); ++i) {
    NS_ConvertUTF16toUTF8 utf8Token(tokenizer.nextToken());
    const char *token = utf8Token.get();
    if (*token == '\0') {
      return NS_ERROR_DOM_SYNTAX_ERR; // empty string (e.g. two commas in a row)
    }

    char *end;
    aValues[i] = strtol(token, &end, 10);
    if (*end != '\0' || !NS_finite(aValues[i])) {
      return NS_ERROR_DOM_SYNTAX_ERR; // parse error
    }
  }
  if (i == 1) {
    aValues[1] = aValues[0];
  }

  if (i == 0                    ||                // Too few values.
      tokenizer.hasMoreTokens() ||                // Too many values.
      tokenizer.lastTokenEndedWithWhitespace() || // Trailing whitespace.
      tokenizer.lastTokenEndedWithSeparator()) {  // Trailing comma.
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  return NS_OK;
}

nsresult
nsSVGIntegerPair::SetBaseValueString(const nsAString &aValueAsString,
                                     nsSVGElement *aSVGElement)
{
  PRInt32 val[2];

  nsresult rv = ParseIntegerOptionalInteger(aValueAsString, val);

  if (NS_FAILED(rv)) {
    return rv;
  }

  mBaseVal[0] = val[0];
  mBaseVal[1] = val[1];
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal[0] = mBaseVal[0];
    mAnimVal[1] = mBaseVal[1];
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
nsSVGIntegerPair::GetBaseValueString(nsAString &aValueAsString)
{
  aValueAsString.Truncate();
  aValueAsString.AppendInt(mBaseVal[0]);
  if (mBaseVal[0] != mBaseVal[1]) {
    aValueAsString.AppendLiteral(", ");
    aValueAsString.AppendInt(mBaseVal[1]);
  }
}

void
nsSVGIntegerPair::SetBaseValue(PRInt32 aValue, PairIndex aPairIndex,
                               nsSVGElement *aSVGElement)
{
  PRUint32 index = (aPairIndex == eFirst ? 0 : 1);
  mBaseVal[index] = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal[index] = aValue;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeIntegerPair(mAttrEnum, true);
}

void
nsSVGIntegerPair::SetBaseValues(PRInt32 aValue1, PRInt32 aValue2,
                                nsSVGElement *aSVGElement)
{
  mBaseVal[0] = aValue1;
  mBaseVal[1] = aValue2;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal[0] = aValue1;
    mAnimVal[1] = aValue2;
  }
  else {
    aSVGElement->AnimationNeedsResample();
  }
  aSVGElement->DidChangeIntegerPair(mAttrEnum, true);
}

void
nsSVGIntegerPair::SetAnimValue(const PRInt32 aValue[2], nsSVGElement *aSVGElement)
{
  mAnimVal[0] = aValue[0];
  mAnimVal[1] = aValue[1];
  mIsAnimated = true;
  aSVGElement->DidAnimateIntegerPair(mAttrEnum);
}

nsresult
nsSVGIntegerPair::ToDOMAnimatedInteger(nsIDOMSVGAnimatedInteger **aResult,
                                       PairIndex aIndex,
                                       nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedInteger(this, aIndex, aSVGElement);
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsISMILAttr*
nsSVGIntegerPair::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILIntegerPair(this, aSVGElement);
}

nsresult
nsSVGIntegerPair::SMILIntegerPair::ValueFromString(const nsAString& aStr,
                                                   const nsISMILAnimationElement* /*aSrcElement*/,
                                                   nsSMILValue& aValue,
                                                   bool& aPreventCachingOfSandwich) const
{
  PRInt32 values[2];

  nsresult rv = ParseIntegerOptionalInteger(aStr, values);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsSMILValue val(&SVGIntegerPairSMILType::sSingleton);
  val.mU.mIntPair[0] = values[0];
  val.mU.mIntPair[1] = values[1];
  aValue = val;
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

nsSMILValue
nsSVGIntegerPair::SMILIntegerPair::GetBaseValue() const
{
  nsSMILValue val(&SVGIntegerPairSMILType::sSingleton);
  val.mU.mIntPair[0] = mVal->mBaseVal[0];
  val.mU.mIntPair[1] = mVal->mBaseVal[1];
  return val;
}

void
nsSVGIntegerPair::SMILIntegerPair::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->SetAnimValue(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = false;
  }
}

nsresult
nsSVGIntegerPair::SMILIntegerPair::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGIntegerPairSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGIntegerPairSMILType::sSingleton) {
    mVal->SetAnimValue(aValue.mU.mIntPair, mSVGElement);
  }
  return NS_OK;
}
