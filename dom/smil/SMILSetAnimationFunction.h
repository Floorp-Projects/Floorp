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
 protected:
  bool IsDisallowedAttribute(const nsAtom* aAttribute) const override;

  // Although <set> animation might look like to-animation, unlike to-animation,
  // it never interpolates values.
  // Returning false here will mean this animation function gets treated as
  // a single-valued function and no interpolation will be attempted.
  bool IsToAnimation() const override { return false; }

  // <set> applies the exact same value across the simple duration.
  bool IsValueFixedForSimpleDuration() const override { return true; }
  bool WillReplace() const override { return true; }
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILSETANIMATIONFUNCTION_H_
