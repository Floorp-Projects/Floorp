/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable CSS property on an element */

#ifndef mozilla_SMILCSSProperty_h
#define mozilla_SMILCSSProperty_h

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "nsAtom.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"

namespace mozilla {
class ComputedStyle;
namespace dom {
class Element;
}  // namespace dom

/**
 * SMILCSSProperty: Implements the SMILAttr interface for SMIL animations
 * that target CSS properties.  Represents a particular animation-targeted CSS
 * property on a particular element.
 */
class SMILCSSProperty : public SMILAttr {
 public:
  /**
   * Constructs a new SMILCSSProperty.
   * @param  aPropID   The CSS property we're interested in animating.
   * @param  aElement  The element whose CSS property is being animated.
   * @param  aBaseComputedStyle  The ComputedStyle to use when getting the base
   *                             value. If this is nullptr and GetBaseValue is
   *                             called, an empty SMILValue initialized with
   *                             the SMILCSSValueType will be returned.
   */
  SMILCSSProperty(nsCSSPropertyID aPropID, dom::Element* aElement,
                  ComputedStyle* aBaseComputedStyle);

  // SMILAttr methods
  virtual nsresult ValueFromString(
      const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
      SMILValue& aValue, bool& aPreventCachingOfSandwich) const override;
  virtual SMILValue GetBaseValue() const override;
  virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  virtual void ClearAnimValue() override;

  /**
   * Utility method - returns true if the given property is supported for
   * SMIL animation.
   *
   * @param   aProperty  The property to check for animation support.
   * @return  true if the given property is supported for SMIL animation, or
   *          false otherwise
   */
  static bool IsPropertyAnimatable(nsCSSPropertyID aPropID);

 protected:
  nsCSSPropertyID mPropID;
  // Using non-refcounted pointer for mElement -- we know mElement will stay
  // alive for my lifetime because a SMILAttr (like me) only lives as long
  // as the Compositing step, and DOM elements don't get a chance to die during
  // that time.
  dom::Element* mElement;

  // The style to use when fetching base styles.
  //
  // As with mElement, since a SMILAttr only lives as long as the
  // compositing step and since ComposeAttribute holds an owning reference to
  // the base ComputedStyle, we can use a non-owning reference here.
  ComputedStyle* mBaseComputedStyle;
};

}  // namespace mozilla

#endif  // mozilla_SMILCSSProperty_h
