/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGINTEGERPAIR_H__
#define __NS_SVGINTEGERPAIR_H__

#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISMILAttr.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedInteger.h"

class nsSMILValue;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
} // namespace dom
} // namespace mozilla

class nsSVGIntegerPair
{

public:
  enum PairIndex {
    eFirst,
    eSecond
  };

  void Init(uint8_t aAttrEnum = 0xff, int32_t aValue1 = 0, int32_t aValue2 = 0) {
    mAnimVal[0] = mBaseVal[0] = aValue1;
    mAnimVal[1] = mBaseVal[1] = aValue2;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement);
  void GetBaseValueString(nsAString& aValue) const;

  void SetBaseValue(int32_t aValue, PairIndex aIndex, nsSVGElement *aSVGElement);
  void SetBaseValues(int32_t aValue1, int32_t aValue2, nsSVGElement *aSVGElement);
  int32_t GetBaseValue(PairIndex aIndex) const
    { return mBaseVal[aIndex == eFirst ? 0 : 1]; }
  void SetAnimValue(const int32_t aValue[2], nsSVGElement *aSVGElement);
  int32_t GetAnimValue(PairIndex aIndex) const
    { return mAnimVal[aIndex == eFirst ? 0 : 1]; }

  // Returns true if the animated value of this integer has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // useable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedInteger>
    ToDOMAnimatedInteger(PairIndex aIndex,
                         nsSVGElement* aSVGElement);
   // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:

  int32_t mAnimVal[2];
  int32_t mBaseVal[2];
  uint8_t mAttrEnum; // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

public:
  struct DOMAnimatedInteger final : public mozilla::dom::SVGAnimatedInteger
  {
    DOMAnimatedInteger(nsSVGIntegerPair* aVal, PairIndex aIndex,
                       nsSVGElement* aSVGElement)
      : mozilla::dom::SVGAnimatedInteger(aSVGElement)
      , mVal(aVal)
      , mIndex(aIndex)
    {}
    virtual ~DOMAnimatedInteger();

    nsSVGIntegerPair* mVal; // kept alive because it belongs to content
    PairIndex mIndex; // are we the first or second integer

    virtual int32_t BaseVal() override
    {
      return mVal->GetBaseValue(mIndex);
    }
    virtual void SetBaseVal(int32_t aValue) override
    {
      mVal->SetBaseValue(aValue, mIndex, mSVGElement);
    }

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    virtual int32_t AnimVal() override
    {
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue(mIndex);
    }
  };

  struct SMILIntegerPair : public nsISMILAttr
  {
  public:
    SMILIntegerPair(nsSVGIntegerPair* aVal, nsSVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGIntegerPair* mVal;
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

#endif //__NS_SVGINTEGERPAIR_H__
