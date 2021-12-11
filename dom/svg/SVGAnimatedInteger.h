/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDINTEGER_H_
#define DOM_SVG_SVGANIMATEDINTEGER_H_

#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "DOMSVGAnimatedInteger.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGElement.h"

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
}  // namespace dom

class SVGAnimatedInteger {
 public:
  friend class AutoChangeIntegerNotifier;
  using SVGElement = dom::SVGElement;

  void Init(uint8_t aAttrEnum = 0xff, int32_t aValue = 0) {
    mAnimVal = mBaseVal = aValue;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue, SVGElement* aSVGElement);
  void GetBaseValueString(nsAString& aValue);

  void SetBaseValue(int32_t aValue, SVGElement* aSVGElement);
  int32_t GetBaseValue() const { return mBaseVal; }

  void SetAnimValue(int aValue, SVGElement* aSVGElement);
  int GetAnimValue() const { return mAnimVal; }

  // Returns true if the animated value of this integer has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<dom::DOMSVGAnimatedInteger> ToDOMAnimatedInteger(
      SVGElement* aSVGElement);
  UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  int32_t mAnimVal;
  int32_t mBaseVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

 public:
  struct DOMAnimatedInteger final : public dom::DOMSVGAnimatedInteger {
    DOMAnimatedInteger(SVGAnimatedInteger* aVal, SVGElement* aSVGElement)
        : dom::DOMSVGAnimatedInteger(aSVGElement), mVal(aVal) {}
    virtual ~DOMAnimatedInteger();

    SVGAnimatedInteger* mVal;  // kept alive because it belongs to content

    virtual int32_t BaseVal() override { return mVal->GetBaseValue(); }
    virtual void SetBaseVal(int32_t aValue) override {
      mVal->SetBaseValue(aValue, mSVGElement);
    }

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    virtual int32_t AnimVal() override {
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue();
    }
  };

  struct SMILInteger : public SMILAttr {
   public:
    SMILInteger(SVGAnimatedInteger* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedInteger* mVal;
    SVGElement* mSVGElement;

    // SMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
        SMILValue& aValue, bool& aPreventCachingOfSandwich) const override;
    virtual SMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEDINTEGER_H_
