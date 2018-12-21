/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGENUM_H__
#define __NS_SVGENUM_H__

#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISMILAttr.h"
#include "SVGElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedEnumeration.h"
#include "mozilla/UniquePtr.h"

class nsAtom;
class nsSMILValue;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}  // namespace dom
}  // namespace mozilla

typedef uint8_t nsSVGEnumValue;

struct nsSVGEnumMapping {
  nsStaticAtom* const mKey;
  const nsSVGEnumValue mVal;
};

class nsSVGEnum {
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

  mozilla::UniquePtr<nsISMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

  const nsSVGEnumMapping* GetMapping(SVGElement* aSVGElement);

 public:
  struct DOMAnimatedEnum final : public mozilla::dom::SVGAnimatedEnumeration {
    DOMAnimatedEnum(nsSVGEnum* aVal, SVGElement* aSVGElement)
        : mozilla::dom::SVGAnimatedEnumeration(aSVGElement), mVal(aVal) {}
    virtual ~DOMAnimatedEnum();

    nsSVGEnum* mVal;  // kept alive because it belongs to content

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

  struct SMILEnum : public nsISMILAttr {
   public:
    SMILEnum(nsSVGEnum* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGEnum* mVal;
    SVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr,
        const mozilla::dom::SVGAnimationElement* aSrcElement,
        nsSMILValue& aValue, bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};

#endif  //__NS_SVGENUM_H__
