/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDENUMERATION_H_
#define DOM_SVG_SVGANIMATEDENUMERATION_H_

#include "DOMSVGAnimatedEnumeration.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGElement.h"

class nsAtom;

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
}  // namespace dom

using SVGEnumValue = uint8_t;

struct SVGEnumMapping {
  nsStaticAtom* const mKey;
  const SVGEnumValue mVal;
};

class SVGAnimatedEnumeration {
 public:
  friend class AutoChangeEnumNotifier;
  using SVGElement = dom::SVGElement;

  void Init(uint8_t aAttrEnum, uint16_t aValue) {
    mAnimVal = mBaseVal = uint8_t(aValue);
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  // Returns whether aValue corresponded to a key in our mapping (in which case
  // we actually set the base value) or not (in which case we did not).
  bool SetBaseValueAtom(const nsAtom* aValue, SVGElement* aSVGElement);
  nsAtom* GetBaseValueAtom(SVGElement* aSVGElement);
  void SetBaseValue(uint16_t aValue, SVGElement* aSVGElement, ErrorResult& aRv);
  uint16_t GetBaseValue() const { return mBaseVal; }

  void SetAnimValue(uint16_t aValue, SVGElement* aSVGElement);
  uint16_t GetAnimValue() const { return mAnimVal; }
  bool IsExplicitlySet() const { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<dom::DOMSVGAnimatedEnumeration> ToDOMAnimatedEnum(
      SVGElement* aSVGElement);

  UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  SVGEnumValue mAnimVal;
  SVGEnumValue mBaseVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;
  bool mIsBaseSet;

  const SVGEnumMapping* GetMapping(SVGElement* aSVGElement);

 public:
  // DOM wrapper class for the (DOM)SVGAnimatedEnumeration interface where the
  // wrapped class is SVGAnimatedEnumeration.
  struct DOMAnimatedEnum final : public dom::DOMSVGAnimatedEnumeration {
    DOMAnimatedEnum(SVGAnimatedEnumeration* aVal, SVGElement* aSVGElement)
        : dom::DOMSVGAnimatedEnumeration(aSVGElement), mVal(aVal) {}
    virtual ~DOMAnimatedEnum();

    SVGAnimatedEnumeration* mVal;  // kept alive because it belongs to content

    using dom::DOMSVGAnimatedEnumeration::SetBaseVal;
    uint16_t BaseVal() override { return mVal->GetBaseValue(); }
    void SetBaseVal(uint16_t aBaseVal, ErrorResult& aRv) override {
      mVal->SetBaseValue(aBaseVal, mSVGElement, aRv);
    }
    uint16_t AnimVal() override {
      // Script may have modified animation parameters or timeline -- DOM
      // getters need to flush any resample requests to reflect these
      // modifications.
      mSVGElement->FlushAnimations();
      return mVal->GetAnimValue();
    }
  };

  struct SMILEnum : public SMILAttr {
   public:
    SMILEnum(SVGAnimatedEnumeration* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedEnumeration* mVal;
    SVGElement* mSVGElement;

    // SMILAttr methods
    nsresult ValueFromString(const nsAString& aStr,
                             const dom::SVGAnimationElement* aSrcElement,
                             SMILValue& aValue,
                             bool& aPreventCachingOfSandwich) const override;
    SMILValue GetBaseValue() const override;
    void ClearAnimValue() override;
    nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEDENUMERATION_H_
