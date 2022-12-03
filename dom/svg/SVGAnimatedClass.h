/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDCLASS_H_
#define DOM_SVG_SVGANIMATEDCLASS_H_

#include "nsCycleCollectionParticipant.h"
#include "mozilla/SVGAnimatedClassOrString.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGAnimatedString;
class SVGElement;
}  // namespace dom

class SVGAnimatedClass final : public SVGAnimatedClassOrString {
 public:
  using SVGElement = dom::SVGElement;

  void Init() { mAnimVal = nullptr; }

  void SetBaseValue(const nsAString& aValue, SVGElement* aSVGElement,
                    bool aDoSetAttr) override;
  void GetBaseValue(nsAString& aValue,
                    const SVGElement* aSVGElement) const override;

  void SetAnimValue(const nsAString& aValue, SVGElement* aSVGElement);
  void GetAnimValue(nsAString& aResult,
                    const SVGElement* aSVGElement) const override;
  bool IsAnimated() const { return !!mAnimVal; }

  UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  UniquePtr<nsString> mAnimVal;

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

#endif  // DOM_SVG_SVGANIMATEDCLASS_H_
