/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS parsing utility functions */

#ifndef mozilla_ServoCSSParser_h
#define mozilla_ServoCSSParser_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/Matrix.h"
#include "nsColor.h"
#include "nsCSSPropertyID.h"
#include "nsDOMCSSDeclaration.h"
#include "nsStringFwd.h"

struct nsCSSRect;
template <class T>
class RefPtr;

namespace mozilla {

class ServoStyleSet;
struct URLExtraData;
struct StyleFontFamilyList;
struct StyleFontStretch;
struct StyleFontWeight;
struct StyleFontStyle;
struct StyleLockedDeclarationBlock;
struct StyleParsingMode;
union StyleComputedFontStyleDescriptor;

template <typename Integer, typename Number, typename LinearStops>
struct StyleTimingFunction;
struct StylePiecewiseLinearFunction;
using StyleComputedTimingFunction =
    StyleTimingFunction<int32_t, float, StylePiecewiseLinearFunction>;

namespace css {
class Loader;
}

namespace dom {
class Document;
}

class ServoCSSParser {
 public:
  using ParsingEnvironment = nsDOMCSSDeclaration::ParsingEnvironment;

  /**
   * Returns whether the specified string can be parsed as a valid CSS
   * <color> value.
   *
   * This includes Mozilla-specific keywords such as -moz-default-color.
   */
  static bool IsValidCSSColor(const nsACString& aValue);

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
  static bool ComputeColor(ServoStyleSet* aStyleSet, nscolor aCurrentColor,
                           const nsACString& aValue, nscolor* aResultColor,
                           bool* aWasCurrentColor = nullptr,
                           css::Loader* aLoader = nullptr);

  /**
   * Parse a string representing a CSS property value into a
   * StyleLockedDeclarationBlock.
   *
   * @param aProperty The property to be parsed.
   * @param aValue The specified value.
   * @param aParsingEnvironment All the parsing environment data we need.
   * @param aParsingMode The parsing mode we apply.
   * @return The parsed value as a StyleLockedDeclarationBlock. We put the value
   *   in a declaration block since that is how we represent specified values
   *   in Servo.
   */
  static already_AddRefed<StyleLockedDeclarationBlock> ParseProperty(
      nsCSSPropertyID aProperty, const nsACString& aValue,
      const ParsingEnvironment& aParsingEnvironment,
      const StyleParsingMode& aParsingMode);

  /**
   * Parse a animation timing function.
   *
   * @param aValue The specified value.
   * @param aResult The output timing function. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseEasing(const nsACString& aValue,
                          StyleComputedTimingFunction& aResult);

  /**
   * Parse a specified transform list into a gfx matrix.
   *
   * @param aValue The specified value.
   * @param aContains3DTransform The output flag indicates whether this is any
   *   3d transform function. (output)
   * @param aResult The output matrix. (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseTransformIntoMatrix(const nsACString& aValue,
                                       bool& aContains3DTransform,
                                       gfx::Matrix4x4& aResult);

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
   * @param aSize If non-null, returns the parsed font size. (output)
   * @param aSmallCaps If non-null, whether small-caps was specified (output)
   * @return Whether the value was successfully parsed.
   */
  static bool ParseFontShorthandForMatching(
      const nsACString& aValue, URLExtraData* aUrl, StyleFontFamilyList& aList,
      StyleFontStyle& aStyle, StyleFontStretch& aStretch,
      StyleFontWeight& aWeight, float* aSize = nullptr,
      bool* aSmallCaps = nullptr);

  /**
   * Get a URLExtraData from a document.
   */
  static already_AddRefed<URLExtraData> GetURLExtraData(dom::Document*);

  /**
   * Get a ParsingEnvironment from a document.
   */
  static ParsingEnvironment GetParsingEnvironment(dom::Document*);
};

}  // namespace mozilla

#endif  // mozilla_ServoCSSParser_h
