/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable CSS property on an element */

#ifndef NS_SMILCSSPROPERTY_H_
#define NS_SMILCSSPROPERTY_H_

#include "mozilla/Attributes.h"
#include "nsISMILAttr.h"
#include "nsIAtom.h"
#include "nsCSSProperty.h"
#include "nsCSSValue.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/**
 * nsSMILCSSProperty: Implements the nsISMILAttr interface for SMIL animations
 * that target CSS properties.  Represents a particular animation-targeted CSS
 * property on a particular element.
 */
class nsSMILCSSProperty : public nsISMILAttr
{
public:
  /**
   * Constructs a new nsSMILCSSProperty.
   * @param  aPropID   The CSS property we're interested in animating.
   * @param  aElement  The element whose CSS property is being animated.
   */
  nsSMILCSSProperty(nsCSSProperty aPropID, mozilla::dom::Element* aElement);

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const mozilla::dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const override;
  virtual nsSMILValue GetBaseValue() const override;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue) override;
  virtual void        ClearAnimValue() override;

  /**
   * Utility method - returns true if the given property is supported for
   * SMIL animation.
   *
   * @param   aProperty  The property to check for animation support.
   * @return  true if the given property is supported for SMIL animation, or
   *          false otherwise
   */
  static bool IsPropertyAnimatable(nsCSSProperty aPropID);

protected:
  nsCSSProperty mPropID;
  // Using non-refcounted pointer for mElement -- we know mElement will stay
  // alive for my lifetime because a nsISMILAttr (like me) only lives as long
  // as the Compositing step, and DOM elements don't get a chance to die during
  // that time.
  mozilla::dom::Element*   mElement;
};

#endif // NS_SMILCSSPROPERTY_H_
