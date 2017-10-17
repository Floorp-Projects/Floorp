/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#ifndef mozilla_ServoCSSParser_h
#define mozilla_ServoCSSParser_h

#include "mozilla/ServoBindings.h"

namespace mozilla {

class ServoCSSParser
{
public:
  /**
   * Returns whether the specified string can be parsed as a valid CSS
   * <color> value.
   *
   * This includes Mozilla-specific keywords such as -moz-default-color.
   */
  static bool IsValidCSSColor(const nsAString& aValue);

  /**
   * Computes an nscolor from the given CSS <color> value.
   *
   * @param aStyleSet The style set whose nsPresContext will be used to
   *   compute system colors and other special color values.
   * @param aCurrentColor The color value that currentcolor should compute to.
   * @param aValue The CSS <color> value.
   * @param aResultColor The resulting computed color value.
   * @return Whether aValue was successfully parsed and aResultColor was set.
   */
  static bool ComputeColor(ServoStyleSet* aStyleSet,
                           nscolor aCurrentColor,
                           const nsAString& aValue,
                           nscolor* aResultColor);

  /**
   * Parses a IntersectionObserver's initialization dictionary's rootMargin
   * property.
   *
   * @param aValue The rootMargin value.
   * @param aResult The nsCSSRect object to write the result into.
   * @return Whether the value was successfully parsed.
   */
  static bool ParseIntersectionObserverRootMargin(const nsAString& aValue,
                                                  nsCSSRect* aResult);
};

} // namespace mozilla

#endif // mozilla_ServoCSSParser_h
