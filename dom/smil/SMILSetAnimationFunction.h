/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILSETANIMATIONFUNCTION_H_
#define DOM_SMIL_SMILSETANIMATIONFUNCTION_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAnimationFunction.h"

namespace mozilla {

//----------------------------------------------------------------------
// SMILSetAnimationFunction
//
// Subclass of SMILAnimationFunction that limits the behaviour to that offered
// by a <set> element.
//
class SMILSetAnimationFunction : public SMILAnimationFunction {
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
  virtual bool SetAttr(nsAtom* aAttribute, const nsAString& aValue,
                       nsAttrValue& aResult,
                       nsresult* aParseResult = nullptr) override;

  /*
   * Unsets the given attribute.
   *
   * @returns true if aAttribute is a recognized animation-related
   *          attribute; false otherwise.
   */
  virtual bool UnsetAttr(nsAtom* aAttribute) override;

 protected:
  // Although <set> animation might look like to-animation, unlike to-animation,
  // it never interpolates values.
  // Returning false here will mean this animation function gets treated as
  // a single-valued function and no interpolation will be attempted.
  virtual bool IsToAnimation() const override { return false; }

  // <set> applies the exact same value across the simple duration.
  virtual bool IsValueFixedForSimpleDuration() const override { return true; }
  virtual bool HasAttr(nsAtom* aAttName) const override;
  virtual const nsAttrValue* GetAttr(nsAtom* aAttName) const override;
  virtual bool GetAttr(nsAtom* aAttName, nsAString& aResult) const override;
  virtual bool WillReplace() const override;

  bool IsDisallowedAttribute(const nsAtom* aAttribute) const;
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILSETANIMATIONFUNCTION_H_
