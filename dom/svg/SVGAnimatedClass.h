/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGCLASS_H__
#define __NS_SVGCLASS_H__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGAnimatedString;
class SVGElement;

class SVGAnimatedClass {
 public:
  typedef mozilla::dom::SVGElement SVGElement;

  void Init() { mAnimVal = nullptr; }

  void SetBaseValue(const nsAString& aValue, SVGElement* aSVGElement,
                    bool aDoSetAttr);
  void GetBaseValue(nsAString& aValue, const SVGElement* aSVGElement) const;

  void SetAnimValue(const nsAString& aValue, SVGElement* aSVGElement);
  void GetAnimValue(nsAString& aResult, const SVGElement* aSVGElement) const;
  bool IsAnimated() const { return !!mAnimVal; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedString> ToDOMAnimatedString(
      SVGElement* aSVGElement);

  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  nsAutoPtr<nsString> mAnimVal;

 public:
  struct SMILString : public SMILAttr {
   public:
    SMILString(SVGAnimatedClass* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedClass* mVal;
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

}  // namespace dom
}  // namespace mozilla

#endif  //__NS_SVGCLASS_H__
