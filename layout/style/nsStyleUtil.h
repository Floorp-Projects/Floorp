/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsStyleUtil_h___
#define nsStyleUtil_h___

#include "nsCoord.h"
#include "nsCSSProperty.h"
#include "gfxFontFeatures.h"
#include "nsTArray.h"
#include "nsCSSValue.h"

struct nsStyleBackground;
class nsString;
class nsStringComparator;
class nsIContent;



// Style utility functions
class nsStyleUtil {
public:

 static bool DashMatchCompare(const nsAString& aAttributeValue,
                                const nsAString& aSelectorValue,
                                const nsStringComparator& aComparator);
                                
  // Append a quoted (with "") and escaped version of aString to aResult.
  static void AppendEscapedCSSString(const nsString& aString,
                                     nsAString& aResult);
  // Append the identifier given by |aIdent| to |aResult|, with
  // appropriate escaping so that it can be reparsed to the same
  // identifier.
  static void AppendEscapedCSSIdent(const nsString& aIdent,
                                    nsAString& aResult);

  // Append a bitmask-valued property's value(s) (space-separated) to aResult.
  static void AppendBitmaskCSSValue(nsCSSProperty aProperty,
                                    PRInt32 aMaskedValue,
                                    PRInt32 aFirstMask,
                                    PRInt32 aLastMask,
                                    nsAString& aResult);

  static void AppendFontFeatureSettings(const nsTArray<gfxFontFeature>& aFeatures,
                                        nsAString& aResult);

  static void AppendFontFeatureSettings(const nsCSSValue& src,
                                        nsAString& aResult);

  /*
   * Convert an author-provided floating point number to an integer (0
   * ... 255) appropriate for use in the alpha component of a color.
   */
  static PRUint8 FloatToColorComponent(float aAlpha)
  {
    NS_ASSERTION(0.0 <= aAlpha && aAlpha <= 1.0, "out of range");
    return NSToIntRound(aAlpha * 255);
  }

  /*
   * Convert the alpha component of an nscolor (0 ... 255) to the
   * floating point number with the least accurate *decimal*
   * representation that is converted to that color.
   *
   * Should be used only by serialization code.
   */
  static float ColorComponentToFloat(PRUint8 aAlpha);

  /*
   * Does this child count as significant for selector matching?
   */
  static bool IsSignificantChild(nsIContent* aChild,
                                   bool aTextIsSignificant,
                                   bool aWhitespaceIsSignificant);
};


#endif /* nsStyleUtil_h___ */
