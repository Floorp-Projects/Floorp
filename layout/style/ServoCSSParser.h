/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#ifndef mozilla_ServoCSSParser_h
#define mozilla_ServoCSSParser_h

#include "mozilla/gfx/Types.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ServoTypes.h"
#include "nsColor.h"
#include "nsCSSPropertyID.h"
#include "nsDOMCSSDeclaration.h"
#include "nsString.h"

class nsCSSValue;
class nsIDocument;
struct nsCSSRect;

using RawGeckoGfxMatrix4x4 = mozilla::gfx::Float[16];

namespace mozilla {
namespace css {
class Loader;
} // namespace css
} // namespace mozilla

namespace mozilla {

class ServoStyleSet;
class SharedFontList;
struct URLExtraData;

class ServoCSSParser
{
public:
  using ParsingEnvironment = nsDOMCSSDeclaration::ServoCSSParsingEnvironment;

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
   * @param aWasCurrentColor Whether aValue was currentcolor. Can be nullptr
   *   if the caller doesn't care.
   * @param aLoader The CSS loader for document we're parsing a color for,
   *   so that parse errors can be reported to the console. If nullptr, errors
   *   won't be reported to the console.
   * @return Whether aValue was successfully parsed and aResultColor was set.
   */
  static bool ComputeColor(ServoStyleSet* aStyleSet,
                           nscolor aCurrentColor,
                           const nsAString& aValue,
                           nscolor* aResultColor,
                           bool* aWasCurrentColor = nullptr,
                           css::Loader* aLoader = nullptr);

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

  /**
   * Parse a string representing a CSS property value into a
   * RawServoDeclarationBlock.
   *
   * @param aProperty The property to be parsed.
   * @param aValue The specified value.
   * @param aParsingEnvironment All the parsing environment data we need.
   * @param aParsingMode The paring mode we apply.
   * @return The parsed value as a RawServoDeclarationBlock. We put the value
   *   in a declaration block since that is how we represent specified values
   *   in Servo.
   */
  static already_AddRefed<RawServoDeclarationBlock> ParseProperty(
    nsCSSPropertyID aProperty,
    const nsAString& aValue,
    const ParsingEnvironment& aParsingEnvironment,
    ParsingMode aParsingMode = ParsingMode::Default);

  /**
   * Parse a animation timing function.
   *
   * @param aValue The specified value.
   * @param aUrl The parser url extra data.
   * @param aResult The output timing function. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseEasing(const nsAString& aValue,
                          URLExtraData* aUrl,
                          nsTimingFunction& aResult);

  /**
   * Parse a specified transform list into a gfx matrix.
   *
   * @param aValue The specified value.
   * @param aContains3DTransform The output flag indicates whether this is any
   *   3d transform function. (output)
   * @param aResult The output matrix. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseTransformIntoMatrix(const nsAString& aValue,
                                       bool& aContains3DTransform,
                                       RawGeckoGfxMatrix4x4& aResult);

  /**
   * Parse a font descriptor.
   *
   * @param aDescID The font descriptor id.
   * @param aValue The specified value.
   * @param aUrl The parser url extra data.
   * @param aResult The parsed result. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseFontDescriptor(nsCSSFontDesc aDescID,
                                  const nsAString& aValue,
                                  URLExtraData* aUrl,
                                  nsCSSValue& aResult);

  /**
   * Parse a font shorthand for FontFaceSet matching, so we only care about
   * FontFamily, FontStyle, FontStretch, and FontWeight.
   *
   * @param aValue The specified value.
   * @param aUrl The parser url extra data.
   * @param aList The parsed FontFamily list. (output)
   * @param aStyle The parsed FontStyle. (output)
   * @param aStretch The parsed FontStretch. (output)
   * @param aWeight The parsed FontWeight. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseFontShorthandForMatching(const nsAString& aValue,
                                            URLExtraData* aUrl,
                                            RefPtr<SharedFontList>& aList,
                                            nsCSSValue& aStyle,
                                            nsCSSValue& aStretch,
                                            nsCSSValue& aWeight);

  /**
   * Get a URLExtraData from |nsIDocument|.
   *
   * @param aDocument The current document.
   * @return The URLExtraData object.
   */
  static already_AddRefed<URLExtraData> GetURLExtraData(nsIDocument* aDocument);

  /**
   * Get a ParsingEnvironment from |nsIDocument|.
   *
   * @param aDocument The current document.
   * @return The ParsingEnvironment object.
   */
  static ParsingEnvironment GetParsingEnvironment(nsIDocument* aDocument);
};

} // namespace mozilla

#endif // mozilla_ServoCSSParser_h
