/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SVGAnimatedNumber_h
#define mozilla_SVGAnimatedNumber_h

#include "DOMSVGAnimatedNumber.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsMathUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGElement.h"

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
}  // namespace dom

class SVGAnimatedNumber {
 public:
  typedef mozilla::SMILAttr SMILAttr;
  typedef mozilla::SMILValue SMILValue;
  typedef mozilla::dom::SVGElement SVGElement;

  void Init(uint8_t aAttrEnum = 0xff, float aValue = 0) {
    mAnimVal = mBaseVal = aValue;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue, SVGElement* aSVGElement);
  void GetBaseValueString(nsAString& aValue);

  void SetBaseValue(float aValue, SVGElement* aSVGElement);
  float GetBaseValue() const { return mBaseVal; }
  void SetAnimValue(float aValue, SVGElement* aSVGElement);
  float GetAnimValue() const { return mAnimVal; }

  // Returns true if the animated value of this number has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedNumber> ToDOMAnimatedNumber(
      SVGElement* aSVGElement);
  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  float mAnimVal;
  float mBaseVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

 public:
  // DOM wrapper class for the (DOM)SVGAnimatedNumber interface where the
  // wrapped class is SVGAnimatedNumber.
  struct DOMAnimatedNumber final : public mozilla::dom::DOMSVGAnimatedNumber {
    DOMAnimatedNumber(SVGAnimatedNumber* aVal, SVGElement* aSVGElement)
        : mozilla::dom::DOMSVGAnimatedNumber(aSVGElement), mVal(aVal) {}
    virtual ~DOMAnimatedNumber();

    SVGAnimatedNumber* mVal;  // kept alive because it belongs to content

    virtual float BaseVal() override { return mVal->GetBaseValue(); }
    virtual void SetBaseVal(float aValue) override {
      MOZ_ASSERT(mozilla::IsFinite(aValue));
      mVal->SetBaseValue(aValue, mSVGElement);
    }

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    virtual float AnimVal() override {
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue();
    }
  };

  struct SMILNumber : public SMILAttr {
   public:
    SMILNumber(SVGAnimatedNumber* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedNumber* mVal;
    SVGElement* mSVGElement;

    // SMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr,
        const mozilla::dom::SVGAnimationElement* aSrcElement, SMILValue& aValue,
        bool& aPreventCachingOfSandwich) const override;
    virtual SMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  //  mozilla_SVGAnimatedNumber_h
