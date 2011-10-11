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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGEnum.h"
#include "nsIAtom.h"
#include "nsSVGElement.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "SMILEnumType.h"
#endif // MOZ_SMIL

using namespace mozilla;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(nsSVGEnum::DOMAnimatedEnum, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGEnum::DOMAnimatedEnum)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGEnum::DOMAnimatedEnum)

DOMCI_DATA(SVGAnimatedEnumeration, nsSVGEnum::DOMAnimatedEnum)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGEnum::DOMAnimatedEnum)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedEnumeration)
NS_INTERFACE_MAP_END

nsSVGEnumMapping *
nsSVGEnum::GetMapping(nsSVGElement *aSVGElement)
{
  nsSVGElement::EnumAttributesInfo info = aSVGElement->GetEnumInfo();

  NS_ASSERTION(info.mEnumCount > 0 && mAttrEnum < info.mEnumCount,
               "mapping request for a non-attrib enum");

  return info.mEnumInfo[mAttrEnum].mMapping;
}

nsresult
nsSVGEnum::SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement)
{
  nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aValue);

  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (valAtom == *(mapping->mKey)) {
      mIsBaseSet = PR_TRUE;
      if (mBaseVal != mapping->mVal) {
        mBaseVal = mapping->mVal;
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
      }
      return NS_OK;
    }
    mapping++;
  }

  // only a warning since authors may mistype attribute values
  NS_WARNING("unknown enumeration key");
  return NS_ERROR_DOM_SYNTAX_ERR;
}

void
nsSVGEnum::GetBaseValueString(nsAString& aValue, nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (mBaseVal == mapping->mVal) {
      (*mapping->mKey)->ToString(aValue);
      return;
    }
    mapping++;
  }
  NS_ERROR("unknown enumeration value");
}

nsresult
nsSVGEnum::SetBaseValue(PRUint16 aValue,
                        nsSVGElement *aSVGElement)
{
  nsSVGEnumMapping *mapping = GetMapping(aSVGElement);

  while (mapping && mapping->mKey) {
    if (mapping->mVal == aValue) {
      mIsBaseSet = PR_TRUE;
      if (mBaseVal != PRUint8(aValue)) {
        mBaseVal = PRUint8(aValue);
        if (!mIsAnimated) {
          mAnimVal = mBaseVal;
        }
#ifdef MOZ_SMIL
        else {
          aSVGElement->AnimationNeedsResample();
        }
#endif
        aSVGElement->DidChangeEnum(mAttrEnum, true);
      }
      return NS_OK;
    }
    mapping++;
  }
  return NS_ERROR_DOM_SYNTAX_ERR;
}

void
nsSVGEnum::SetAnimValue(PRUint16 aValue, nsSVGElement *aSVGElement)
{
  mAnimVal = aValue;
  mIsAnimated = PR_TRUE;
  aSVGElement->DidAnimateEnum(mAttrEnum);
}

nsresult
nsSVGEnum::ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                             nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimatedEnum(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

#ifdef MOZ_SMIL
nsISMILAttr*
nsSVGEnum::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILEnum(this, aSVGElement);
}

nsresult
nsSVGEnum::SMILEnum::ValueFromString(const nsAString& aStr,
                                     const nsISMILAnimationElement* /*aSrcElement*/,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const
{
  nsCOMPtr<nsIAtom> valAtom = do_GetAtom(aStr);
  nsSVGEnumMapping *mapping = mVal->GetMapping(mSVGElement);

  while (mapping && mapping->mKey) {
    if (valAtom == *(mapping->mKey)) {
      nsSMILValue val(&SMILEnumType::sSingleton);
      val.mU.mUint = mapping->mVal;
      aValue = val;
      aPreventCachingOfSandwich = PR_FALSE;
      return NS_OK;
    }
    mapping++;
  }
  
  // only a warning since authors may mistype attribute values
  NS_WARNING("unknown enumeration key");
  return NS_ERROR_FAILURE;
}

nsSMILValue
nsSVGEnum::SMILEnum::GetBaseValue() const
{
  nsSMILValue val(&SMILEnumType::sSingleton);
  val.mU.mUint = mVal->mBaseVal;
  return val;
}

void
nsSVGEnum::SMILEnum::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->SetAnimValue(mVal->mBaseVal, mSVGElement);
    mVal->mIsAnimated = PR_FALSE;
  }
}

nsresult
nsSVGEnum::SMILEnum::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILEnumType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILEnumType::sSingleton) {
    NS_ABORT_IF_FALSE(aValue.mU.mUint <= USHRT_MAX,
                      "Very large enumerated value - too big for PRUint16");
    mVal->SetAnimValue(PRUint16(aValue.mU.mUint), mSVGElement);
  }
  return NS_OK;
}
#endif // MOZ_SMIL
