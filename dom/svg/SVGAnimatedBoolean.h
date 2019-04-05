/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGBOOLEAN_H__
#define __NS_SVGBOOLEAN_H__

#include "nsError.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

class nsAtom;

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGAnimatedBoolean;
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

class SVGAnimatedBoolean {
 public:
  typedef mozilla::dom::SVGElement SVGElement;

  void Init(uint8_t aAttrEnum = 0xff, bool aValue = false) {
    mAnimVal = mBaseVal = aValue;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
  }

  nsresult SetBaseValueAtom(const nsAtom* aValue, SVGElement* aSVGElement);
  nsAtom* GetBaseValueAtom() const;

  void SetBaseValue(bool aValue, SVGElement* aSVGElement);
  bool GetBaseValue() const { return mBaseVal; }

  void SetAnimValue(bool aValue, SVGElement* aSVGElement);
  bool GetAnimValue() const { return mAnimVal; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedBoolean> ToDOMAnimatedBoolean(
      SVGElement* aSVGElement);
  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  bool mAnimVal;
  bool mBaseVal;
  bool mIsAnimated;
  uint8_t mAttrEnum;  // element specified tracking for attribute

 public:
  struct SMILBool : public SMILAttr {
   public:
    SMILBool(SVGAnimatedBoolean* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedBoolean* mVal;
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

#endif  //__NS_SVGBOOLEAN_H__
