/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsStyleUtil_h___
#define nsStyleUtil_h___

#include "nsCoord.h"
#include "nsCSSPropertyID.h"
#include "nsTArrayForwardDeclare.h"
#include "gfxFontFamilyList.h"
#include "nsStringFwd.h"
#include "nsStyleStruct.h"
#include "nsCRT.h"

class nsCSSValue;
class nsStyleCoord;
class nsIContent;
class nsIPrincipal;
class nsIURI;
struct gfxFontFeature;
struct gfxAlternateValue;
struct nsCSSKTableEntry;
struct nsCSSValueList;

namespace mozilla {
class FontSlantStyle;
namespace dom {
class Element;
}
}  // namespace mozilla

// Style utility functions
class nsStyleUtil {
 public:
  static bool DashMatchCompare(const nsAString& aAttributeValue,
                               const nsAString& aSelectorValue,
                               const nsStringComparator& aComparator);

  static bool ValueIncludes(const nsAString& aValueList,
                            const nsAString& aValue,
                            const nsStringComparator& aComparator);

  // Append a quoted (with 'quoteChar') and escaped version of aString
  // to aResult.  'quoteChar' must be ' or ".
  static void AppendEscapedCSSString(const nsAString& aString,
                                     nsAString& aResult,
                                     char16_t quoteChar = '"');

  // Append the identifier given by |aIdent| to |aResult|, with
  // appropriate escaping so that it can be reparsed to the same
  // identifier.  An exception is if aIdent contains U+0000, which
  // will be escaped as U+FFFD and then reparsed back to U+FFFD.
  static void AppendEscapedCSSIdent(const nsAString& aIdent,
                                    nsAString& aResult);

  static void AppendFontSlantStyle(const mozilla::FontSlantStyle&,
                                   nsAString& aResult);

 public:
  // Append a bitmask-valued property's value(s) (space-separated) to aResult.
  static void AppendBitmaskCSSValue(const nsCSSKTableEntry aTable[],
                                    int32_t aMaskedValue, int32_t aFirstMask,
                                    int32_t aLastMask, nsAString& aResult);

  static void AppendAngleValue(const nsStyleCoord& aValue, nsAString& aResult);

  static void AppendPaintOrderValue(uint8_t aValue, nsAString& aResult);

  static void AppendCSSNumber(float aNumber, nsAString& aResult) {
    aResult.AppendFloat(aNumber);
  }

  /*
   * Convert an author-provided floating point number to an integer (0
   * ... 255) appropriate for use in the alpha component of a color.
   */
  static uint8_t FloatToColorComponent(float aAlpha) {
    NS_ASSERTION(0.0 <= aAlpha && aAlpha <= 1.0, "out of range");
    return static_cast<uint8_t>(NSToIntRound(aAlpha * 255));
  }

  /*
   * Convert the alpha component of an nscolor (0 ... 255) to the
   * floating point number with the least accurate *decimal*
   * representation that is converted to that color.
   *
   * Should be used only by serialization code.
   */
  static float ColorComponentToFloat(uint8_t aAlpha);

  /**
   * GetSerializedColorValue() computes serialized color value of aColor and
   * returns it with aSerializedColor.
   * https://drafts.csswg.org/cssom/#serialize-a-css-component-value
   */
  static void GetSerializedColorValue(nscolor aColor,
                                      nsAString& aSerializedColor);

  /*
   * Does this child count as significant for selector matching?
   */
  static bool IsSignificantChild(nsIContent* aChild,
                                 bool aWhitespaceIsSignificant);

  /*
   * Thread-safe version of IsSignificantChild()
   */
  static bool ThreadSafeIsSignificantChild(const nsIContent* aChild,
                                           bool aWhitespaceIsSignificant);
  /**
   * Returns true if our object-fit & object-position properties might cause
   * a replaced element's contents to overflow its content-box (requiring
   * clipping), or false if we can be sure that this won't happen.
   *
   * This lets us optimize by skipping clipping when we can tell it's
   * unnecessary (particularly with the default values of these properties).
   *
   * @param aStylePos The nsStylePosition whose object-fit & object-position
   *                  properties should be checked for potential overflow.
   * @return false if we can be sure that the object-fit & object-position
   *         properties on 'aStylePos' cannot cause a replaced element's
   *         contents to overflow its content-box. Otherwise (if overflow is
   *         is possible), returns true.
   */
  static bool ObjectPropsMightCauseOverflow(const nsStylePosition* aStylePos);

  /*
   *  Does this principal have a CSP that blocks the application of
   *  inline styles? Returns false if application of the style should
   *  be blocked.
   *
   *  @param aContent
   *      The <style> element that the caller wants to know whether to honor.
   *      Included to check the nonce attribute if one is provided. Allowed to
   *      be null, if this is for something other than a <style> element (in
   *      which case nonces won't be checked).
   *  @param aPrincipal
   *      The principal of the of the document (*not* of the style sheet).
   *      The document's principal is where any Content Security Policy that
   *      should be used to block or allow inline styles will be located.
   *  @param aTriggeringPrincipal
   *      The principal of the scripted caller which added the inline
   *      stylesheet, or null if no scripted caller can be identified.
   *  @param aSourceURI
   *      URI of document containing inline style (for reporting violations)
   *  @param aLineNumber
   *      Line number of inline style element in the containing document (for
   *      reporting violations)
   *  @param aColumnNumber
   *      Column number of inline style element in the containing document (for
   *      reporting violations)
   *  @param aStyleText
   *      Contents of the inline style element (for reporting violations)
   *  @param aRv
   *      Return error code in case of failure
   *  @return
   *      Does CSP allow application of the specified inline style?
   */
  static bool CSPAllowsInlineStyle(mozilla::dom::Element* aContent,
                                   nsIPrincipal* aPrincipal,
                                   nsIPrincipal* aTriggeringPrincipal,
                                   nsIURI* aSourceURI, uint32_t aLineNumber,
                                   uint32_t aColumnNumber,
                                   const nsAString& aStyleText, nsresult* aRv);

  template <size_t N>
  static bool MatchesLanguagePrefix(const char16_t* aLang, size_t aLen,
                                    const char16_t (&aPrefix)[N]) {
    return !NS_strncmp(aLang, aPrefix, N - 1) &&
           (aLen == N - 1 || aLang[N - 1] == '-');
  }

  template <size_t N>
  static bool MatchesLanguagePrefix(const nsAtom* aLang,
                                    const char16_t (&aPrefix)[N]) {
    MOZ_ASSERT(aLang);
    return MatchesLanguagePrefix(aLang->GetUTF16String(), aLang->GetLength(),
                                 aPrefix);
  }

  template <size_t N>
  static bool MatchesLanguagePrefix(const nsAString& aLang,
                                    const char16_t (&aPrefix)[N]) {
    return MatchesLanguagePrefix(aLang.Data(), aLang.Length(), aPrefix);
  }
};

#endif /* nsStyleUtil_h___ */
