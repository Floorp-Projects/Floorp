/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  SVGMotionSMILAttr(nsSVGElement* aSVGElement)
    : mSVGElement(aSVGElement) {}

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const MOZ_OVERRIDE;
  virtual nsSMILValue GetBaseValue() const MOZ_OVERRIDE;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue) MOZ_OVERRIDE;
  virtual void        ClearAnimValue() MOZ_OVERRIDE;
  virtual const nsIContent* GetTargetNode() const MOZ_OVERRIDE;

protected:
  // Raw pointers are OK here because this SVGMotionSMILAttr is both
  // created & destroyed during a SMIL sample-step, during which time the DOM
  // isn't modified.
  nsSVGElement* mSVGElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILATTR_H_
