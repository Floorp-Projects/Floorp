/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGENUM_H__
#define __NS_SVGENUM_H__

#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGElement.h"

class nsAtom;

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
}  // namespace dom

typedef uint8_t SVGEnumValue;

struct SVGEnumMapping {
  nsStaticAtom* const mKey;
  const SVGEnumValue mVal;
};

class SVGEnum {
 public:
  typedef mozilla::dom::SVGElement SVGElement;

  void Init(uint8_t aAttrEnum, uint16_t aValue) {
    mAnimVal = mBaseVal = uint8_t(aValue);
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueAtom(const nsAtom* aValue, SVGElement* aSVGElement);
  nsAtom* GetBaseValueAtom(SVGElement* aSVGElement);
  nsresult SetBaseValue(uint16_t aValue, SVGElement* aSVGElement);
  uint16_t GetBaseValue() const { return mBaseVal; }

  void SetAnimValue(uint16_t aValue, SVGElement* aSVGElement);
  uint16_t GetAnimValue() const { return mAnimVal; }
  bool IsExplicitlySet() const { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedEnumeration> ToDOMAnimatedEnum(
      SVGElement* aSVGElement);

  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  SVGEnumValue mAnimVal;
  SVGEnumValue mBaseVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

  const SVGEnumMapping* GetMapping(SVGElement* aSVGElement);

 public:
  struct DOMAnimatedEnum final : public mozilla::dom::SVGAnimatedEnumeration {
    DOMAnimatedEnum(SVGEnum* aVal, SVGElement* aSVGElement)
        : mozilla::dom::SVGAnimatedEnumeration(aSVGElement), mVal(aVal) {}
    virtual ~DOMAnimatedEnum();

    SVGEnum* mVal;  // kept alive because it belongs to content

    using mozilla::dom::SVGAnimatedEnumeration::SetBaseVal;
    virtual uint16_t BaseVal() override { return mVal->GetBaseValue(); }
    virtual void SetBaseVal(uint16_t aBaseVal,
                            mozilla::ErrorResult& aRv) override {
      aRv = mVal->SetBaseValue(aBaseVal, mSVGElement);
    }
    virtual uint16_t AnimVal() override {
      // Script may have modified animation parameters or timeline -- DOM
      // getters need to flush any resample requests to reflect these
      // modifications.
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue();
    }
  };

  struct SMILEnum : public SMILAttr {
   public:
    SMILEnum(SVGEnum* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGEnum* mVal;
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

#endif  //__NS_SVGENUM_H__
