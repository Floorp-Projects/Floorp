/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable CSS property on an element */

#ifndef NS_SMILCSSPROPERTY_H_
#define NS_SMILCSSPROPERTY_H_

#include "mozilla/Attributes.h"
#include "mozilla/StyleBackendType.h"
#include "nsISMILAttr.h"
#include "nsIAtom.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"

class nsStyleContext;

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
   * @param  aBaseStyleContext  The style context to use when getting the base
   *                            value. If this is nullptr and GetBaseValue is
   *                            called, an empty nsSMILValue initialized with
   *                            the nsSMILCSSValueType will be returned.
   */
  nsSMILCSSProperty(nsCSSPropertyID aPropID,
                    mozilla::dom::Element* aElement,
                    nsStyleContext* aBaseStyleContext);

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
   * @param   aBackend   The style backend to check for animation support.
   *                     This is a temporary measure until the Servo backend
   *                     supports all animatable properties (bug 1353918).
   * @return  true if the given property is supported for SMIL animation, or
   *          false otherwise
   */
  static bool IsPropertyAnimatable(nsCSSPropertyID aPropID,
                                   mozilla::StyleBackendType aBackend);

protected:
  nsCSSPropertyID mPropID;
  // Using non-refcounted pointer for mElement -- we know mElement will stay
  // alive for my lifetime because a nsISMILAttr (like me) only lives as long
  // as the Compositing step, and DOM elements don't get a chance to die during
  // that time.
  mozilla::dom::Element*   mElement;

  // The style context to use when fetching base styles.
  // As with mElement, since an nsISMILAttr only lives as long as the
  // compositing step and since ComposeAttribute holds an owning reference to
  // the base style context, we can use a non-owning reference here.
  nsStyleContext* mBaseStyleContext;
};

#endif // NS_SMILCSSPROPERTY_H_
