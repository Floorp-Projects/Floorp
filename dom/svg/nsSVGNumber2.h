/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGNUMBER2_H__
#define __NS_SVGNUMBER2_H__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISMILAttr.h"
#include "nsMathUtils.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/SVGAnimatedNumber.h"

class nsSMILValue;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}
}

class nsSVGNumber2
{

public:
  void Init(uint8_t aAttrEnum = 0xff, float aValue = 0) {
    mAnimVal = mBaseVal = aValue;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement);
  void GetBaseValueString(nsAString& aValue);

  void SetBaseValue(float aValue, nsSVGElement *aSVGElement);
  float GetBaseValue() const
    { return mBaseVal; }
  void SetAnimValue(float aValue, nsSVGElement *aSVGElement);
  float GetAnimValue() const
    { return mAnimVal; }

  // Returns true if the animated value of this number has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // useable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedNumber>
  ToDOMAnimatedNumber(nsSVGElement* aSVGElement);
  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:

  float mAnimVal;
  float mBaseVal;
  uint8_t mAttrEnum; // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

public:
  struct DOMAnimatedNumber final : public mozilla::dom::SVGAnimatedNumber
  {
    DOMAnimatedNumber(nsSVGNumber2* aVal, nsSVGElement* aSVGElement)
      : mozilla::dom::SVGAnimatedNumber(aSVGElement)
      , mVal(aVal)
    {}
    virtual ~DOMAnimatedNumber();

    nsSVGNumber2* mVal; // kept alive because it belongs to content

    virtual float BaseVal() override
    {
      return mVal->GetBaseValue();
    }
    virtual void SetBaseVal(float aValue) override
    {
      MOZ_ASSERT(mozilla::IsFinite(aValue));
      mVal->SetBaseValue(aValue, mSVGElement);
    }

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    virtual float AnimVal() override
    {
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue();
    }
  };

  struct SMILNumber : public nsISMILAttr
  {
  public:
    SMILNumber(nsSVGNumber2* aVal, nsSVGElement* aSVGElement)
    : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGNumber2* mVal;
    nsSVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const mozilla::dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};

#endif //__NS_SVGNUMBER2_H__
