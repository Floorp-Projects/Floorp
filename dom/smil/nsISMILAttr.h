/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_ISMILATTR_H_
#define NS_ISMILATTR_H_

#include "nscore.h"

class nsSMILValue;
class nsIContent;
class nsAString;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}
}

////////////////////////////////////////////////////////////////////////
// nsISMILAttr: A variable targeted by SMIL for animation and can therefore have
// an underlying (base) value and an animated value For example, an attribute of
// a particular SVG element.
//
// These objects only exist during the compositing phase of SMIL animation
// calculations. They have a single owner who is responsible for deleting the
// object.

class nsISMILAttr
{
public:
  /**
   * Creates a new nsSMILValue for this attribute from a string. The string is
   * parsed in the context of this attribute so that context-dependent values
   * such as em-based units can be resolved into a canonical form suitable for
   * animation (including interpolation etc.).
   *
   * @param aStr        A string defining the new value to be created.
   * @param aSrcElement The source animation element. This may be needed to
   *                    provided additional context data such as for
   *                    animateTransform where the 'type' attribute is needed to
   *                    parse the value.
   * @param[out] aValue Outparam for storing the parsed value.
   * @param[out] aPreventCachingOfSandwich
   *                    Outparam to indicate whether the attribute contains
   *                    dependencies on its context that should prevent the
   *                    result of the animation sandwich from being cached and
   *                    reused in future samples.
   * @return NS_OK on success or an error code if creation failed.
   */
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const mozilla::dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const = 0;

  /**
   * Gets the underlying value of this attribute.
   *
   * @return an nsSMILValue object. returned_object.IsNull() will be true if an
   * error occurred.
   */
  virtual nsSMILValue GetBaseValue() const = 0;

  /**
   * Clears the animated value of this attribute.
   *
   * NOTE: The animation target is not guaranteed to be in a document when this
   * method is called. (See bug 523188)
   */
  virtual void ClearAnimValue() = 0;

  /**
   * Sets the presentation value of this attribute.
   *
   * @param aValue  The value to set.
   * @return NS_OK on success or an error code if setting failed.
   */
  virtual nsresult SetAnimValue(const nsSMILValue& aValue) = 0;

  /**
   * Returns the targeted content node, for any nsISMILAttr implementations
   * that want to expose that to the animation logic.  Otherwise, returns
   * null.
   *
   * @return the targeted content node, if this nsISMILAttr implementation
   * wishes to make it avaiable.  Otherwise, nullptr.
   */
  virtual const nsIContent* GetTargetNode() const { return nullptr; }

  /**
   * Virtual destructor, to make sure subclasses can clean themselves up.
   */
  virtual ~nsISMILAttr() {}
};

#endif // NS_ISMILATTR_H_
