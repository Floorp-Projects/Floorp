/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGSTRING_H__
#define __NS_SVGSTRING_H__

#include "DOMSVGAnimatedString.h"
#include "nsAutoPtr.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class SMILValue;

namespace dom {
class SVGElement;
}

class SVGAnimatedString {
 public:
  typedef mozilla::dom::SVGElement SVGElement;

  void Init(uint8_t aAttrEnum) {
    mAnimVal = nullptr;
    mAttrEnum = aAttrEnum;
    mIsBaseSet = false;
  }

  void SetBaseValue(const nsAString& aValue, SVGElement* aSVGElement,
                    bool aDoSetAttr);
  void GetBaseValue(nsAString& aValue, const SVGElement* aSVGElement) const {
    aSVGElement->GetStringBaseValue(mAttrEnum, aValue);
  }

  void SetAnimValue(const nsAString& aValue, SVGElement* aSVGElement);
  void GetAnimValue(nsAString& aResult, const SVGElement* aSVGElement) const;

  // Returns true if the animated value of this string has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const { return !!mAnimVal || mIsBaseSet; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedString> ToDOMAnimatedString(
      SVGElement* aSVGElement);

  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  nsAutoPtr<nsString> mAnimVal;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsBaseSet;

 public:
  // DOM wrapper class for the (DOM)SVGAnimatedString interface where the
  // wrapped class is SVGAnimatedString.
  struct DOMAnimatedString final : public mozilla::dom::DOMSVGAnimatedString {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMAnimatedString)

    DOMAnimatedString(SVGAnimatedString* aVal, SVGElement* aSVGElement)
        : mozilla::dom::DOMSVGAnimatedString(aSVGElement), mVal(aVal) {}

    SVGAnimatedString* mVal;  // kept alive because it belongs to content

    void GetBaseVal(nsAString& aResult) override {
      mVal->GetBaseValue(aResult, mSVGElement);
    }

    void SetBaseVal(const nsAString& aValue) override {
      mVal->SetBaseValue(aValue, mSVGElement, true);
    }

    void GetAnimVal(nsAString& aResult) override {
      mSVGElement->FlushAnimations();
      mVal->GetAnimValue(aResult, mSVGElement);
    }

   private:
    virtual ~DOMAnimatedString();
  };
  struct SMILString : public SMILAttr {
   public:
    SMILString(SVGAnimatedString* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedString* mVal;
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

#endif  //__NS_SVGSTRING_H__
