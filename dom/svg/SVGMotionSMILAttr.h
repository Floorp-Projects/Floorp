/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a dummy attribute targeted by <animateMotion> element */

#ifndef DOM_SVG_SVGMOTIONSMILATTR_H_
#define DOM_SVG_SVGMOTIONSMILATTR_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"

class nsIContent;

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

/**
 * SVGMotionSMILAttr: Implements the SMILAttr interface for SMIL animations
 * from <animateMotion>.
 *
 * NOTE: Even though there's technically no "motion" attribute, we behave in
 * many ways as if there were, for simplicity.
 */
class SVGMotionSMILAttr : public SMILAttr {
 public:
  explicit SVGMotionSMILAttr(dom::SVGElement* aSVGElement)
      : mSVGElement(aSVGElement) {}

  // SMILAttr methods
  nsresult ValueFromString(const nsAString& aStr,
                           const dom::SVGAnimationElement* aSrcElement,
                           SMILValue& aValue,
                           bool& aPreventCachingOfSandwich) const override;
  SMILValue GetBaseValue() const override;
  nsresult SetAnimValue(const SMILValue& aValue) override;
  void ClearAnimValue() override;
  const nsIContent* GetTargetNode() const override;

 protected:
  // Raw pointers are OK here because this SVGMotionSMILAttr is both
  // created & destroyed during a SMIL sample-step, during which time the DOM
  // isn't modified.
  dom::SVGElement* mSVGElement;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGMOTIONSMILATTR_H_
