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

  /**
   * Parses a @counter-style name.
   *
   * @param aValue The name to parse.
   * @return The name as an atom, lowercased if a built-in counter style name,
   *   or nullptr if parsing failed or if the name was invalid (like "inherit").
   */
  static already_AddRefed<nsAtom> ParseCounterStyleName(const nsAString& aValue);

  /**
   * Parses a @counter-style descriptor.
   *
   * @param aDescriptor The descriptor to parse.
   * @param aValue The value of the descriptor.
   * @param aURLExtraData URL data for parsing. This would be used for
   *   image value URL resolution.
   * @param aResult The nsCSSValue to store the result in.
   * @return Whether parsing succeeded.
   */
  static bool
  ParseCounterStyleDescriptor(nsCSSCounterDesc aDescriptor,
                              const nsAString& aValue,
                              URLExtraData* aURLExtraData,
                              nsCSSValue& aResult);
};

} // namespace mozilla

#endif // mozilla_ServoCSSParser_h
