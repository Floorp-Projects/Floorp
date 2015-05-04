/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a dummy attribute targeted by <animateMotion> element */

#ifndef MOZILLA_SVGMOTIONSMILATTR_H_
#define MOZILLA_SVGMOTIONSMILATTR_H_

#include "mozilla/Attributes.h"
#include "nsISMILAttr.h"

class nsIContent;
class nsSMILValue;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGAnimationElement;
}

/**
 * SVGMotionSMILAttr: Implements the nsISMILAttr interface for SMIL animations
 * from <animateMotion>.
 *
 * NOTE: Even though there's technically no "motion" attribute, we behave in
 * many ways as if there were, for simplicity.
 */
class SVGMotionSMILAttr : public nsISMILAttr
{
public:
  explicit SVGMotionSMILAttr(nsSVGElement* aSVGElement)
    : mSVGElement(aSVGElement) {}

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const override;
  virtual nsSMILValue GetBaseValue() const override;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue) override;
  virtual void        ClearAnimValue() override;
  virtual const nsIContent* GetTargetNode() const override;

protected:
  // Raw pointers are OK here because this SVGMotionSMILAttr is both
  // created & destroyed during a SMIL sample-step, during which time the DOM
  // isn't modified.
  nsSVGElement* mSVGElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILATTR_H_
