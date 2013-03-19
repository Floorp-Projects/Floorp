/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGENUM_H__
#define __NS_SVGENUM_H__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsISMILAttr.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"

class nsIAtom;
class nsSMILValue;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}
}

typedef uint8_t nsSVGEnumValue;

struct nsSVGEnumMapping {
  nsIAtom **mKey;
  nsSVGEnumValue mVal;
};

class nsSVGEnum
{
public:
  void Init(uint8_t aAttrEnum, uint16_t aValue) {
    mAnimVal = mBaseVal = uint8_t(aValue);
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueAtom(const nsIAtom* aValue, nsSVGElement *aSVGElement);
  nsIAtom* GetBaseValueAtom(nsSVGElement *aSVGElement);
  nsresult SetBaseValue(uint16_t aValue,
                        nsSVGElement *aSVGElement);
  uint16_t GetBaseValue() const
    { return mBaseVal; }

  void SetAnimValue(uint16_t aValue, nsSVGElement *aSVGElement);
  uint16_t GetAnimValue() const
    { return mAnimVal; }
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  nsresult ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                             nsSVGElement* aSVGElement);

  already_AddRefed<nsIDOMSVGAnimatedEnumeration>
  ToDOMAnimatedEnum(nsSVGElement* aSVGElement);

  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;
  uint8_t mAttrEnum; // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

  nsSVGEnumMapping *GetMapping(nsSVGElement *aSVGElement);

public:
  struct DOMAnimatedEnum MOZ_FINAL : public nsIDOMSVGAnimatedEnumeration
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimatedEnum)

    DOMAnimatedEnum(nsSVGEnum* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    virtual ~DOMAnimatedEnum();

    nsSVGEnum *mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(uint16_t* aResult)
      { *aResult = mVal->GetBaseValue(); return NS_OK; }
    NS_IMETHOD SetBaseVal(uint16_t aValue)
      { return mVal->SetBaseValue(aValue, mSVGElement); }

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    NS_IMETHOD GetAnimVal(uint16_t* aResult)
    {
      mSVGElement->FlushAnimations();
      *aResult = mVal->GetAnimValue();
      return NS_OK;
    }
  };

  struct SMILEnum : public nsISMILAttr
  {
  public:
    SMILEnum(nsSVGEnum* aVal, nsSVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGEnum* mVal;
    nsSVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const mozilla::dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const;
    virtual nsSMILValue GetBaseValue() const;
    virtual void ClearAnimValue();
    virtual nsresult SetAnimValue(const nsSMILValue& aValue);
  };
};

#endif //__NS_SVGENUM_H__
