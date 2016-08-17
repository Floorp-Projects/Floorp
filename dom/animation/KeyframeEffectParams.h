/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeEffectParams_h
#define mozilla_KeyframeEffectParams_h

#include "nsCSSProps.h"
#include "nsString.h"

namespace mozilla {

enum class SpacingMode
{
  distribute,
  paced
};

struct KeyframeEffectParams
{
  void GetSpacingAsString(nsAString& aSpacing) const
  {
    if (mSpacingMode == SpacingMode::distribute) {
      aSpacing.AssignLiteral("distribute");
    } else {
      aSpacing.AssignLiteral("paced(");
      aSpacing.AppendASCII(nsCSSProps::GetStringValue(mPacedProperty).get());
      aSpacing.AppendLiteral(")");
    }
  }

  /**
   * Parse spacing string.
   *
   * @param aSpacing The input spacing string.
   * @param [out] aSpacingMode The parsed spacing mode.
   * @param [out] aPacedProperty The parsed CSS property if using paced spacing.
   * @param [out] aInvalidPacedProperty A string that, if we parsed a string of
   *                                    the form 'paced(<ident>)' where <ident>
   *                                    is not a recognized animatable property,
   *                                    will be set to <ident>.
   * @param [out] aRv The error result.
   */
  static void ParseSpacing(const nsAString& aSpacing,
                           SpacingMode& aSpacingMode,
                           nsCSSPropertyID& aPacedProperty,
                           nsAString& aInvalidPacedProperty,
                           ErrorResult& aRv);

  // FIXME: Bug 1216843: Add IterationCompositeOperations and
  //        Bug 1216844: Add CompositeOperation
  SpacingMode mSpacingMode = SpacingMode::distribute;
  nsCSSPropertyID mPacedProperty = eCSSProperty_UNKNOWN;
};

} // namespace mozilla

#endif // mozilla_KeyframeEffectParams_h
