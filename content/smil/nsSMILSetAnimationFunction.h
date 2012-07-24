/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILSETANIMATIONFUNCTION_H_
#define NS_SMILSETANIMATIONFUNCTION_H_

#include "nsSMILAnimationFunction.h"

//----------------------------------------------------------------------
// nsSMILSetAnimationFunction
//
// Subclass of nsSMILAnimationFunction that limits the behaviour to that offered
// by a <set> element.
//
class nsSMILSetAnimationFunction : public nsSMILAnimationFunction
{
public:
  /*
   * Sets animation-specific attributes (or marks them dirty, in the case
   * of from/to/by/values).
   *
   * @param aAttribute The attribute being set
   * @param aValue     The updated value of the attribute.
   * @param aResult    The nsAttrValue object that may be used for storing the
   *                   parsed result.
   * @param aParseResult  Outparam used for reporting parse errors. Will be set
   *                      to NS_OK if everything succeeds.
   * @returns true if aAttribute is a recognized animation-related
   *          attribute; false otherwise.
   */
  virtual bool SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                         nsAttrValue& aResult, nsresult* aParseResult = nsnull);

  /*
   * Unsets the given attribute.
   *
   * @returns true if aAttribute is a recognized animation-related
   *          attribute; false otherwise.
   */
  virtual bool UnsetAttr(nsIAtom* aAttribute) MOZ_OVERRIDE;

protected:
  // Although <set> animation might look like to-animation, unlike to-animation,
  // it never interpolates values.
  // Returning false here will mean this animation function gets treated as
  // a single-valued function and no interpolation will be attempted.
  virtual bool IsToAnimation() const MOZ_OVERRIDE {
    return false;
  }

  // <set> applies the exact same value across the simple duration.
  virtual bool IsValueFixedForSimpleDuration() const MOZ_OVERRIDE {
    return true;
  }
  virtual bool               HasAttr(nsIAtom* aAttName) const MOZ_OVERRIDE;
  virtual const nsAttrValue* GetAttr(nsIAtom* aAttName) const MOZ_OVERRIDE;
  virtual bool               GetAttr(nsIAtom* aAttName,
                                     nsAString& aResult) const MOZ_OVERRIDE;
  virtual bool WillReplace() const MOZ_OVERRIDE;

  bool IsDisallowedAttribute(const nsIAtom* aAttribute) const;
};

#endif // NS_SMILSETANIMATIONFUNCTION_H_
