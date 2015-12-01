/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFont_h___
#define nsFont_h___

#include <stdint.h>                     // for uint8_t, uint16_t
#include <sys/types.h>                  // for int16_t
#include "gfxFontFamilyList.h"
#include "gfxFontFeatures.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCoord.h"                    // for nscoord
#include "nsStringFwd.h"                // for nsSubstring
#include "nsString.h"               // for nsString
#include "nsTArray.h"                   // for nsTArray

struct gfxFontStyle;

// XXX we need a method to enumerate all of the possible fonts on the
// system across family, weight, style, size, etc. But not here!

// Enumerator callback function. Return false to stop
typedef bool (*nsFontFamilyEnumFunc)(const nsString& aFamily, bool aGeneric, void *aData);

// IDs for generic fonts
// NOTE: 0, 1 are reserved for the special IDs of the default variable
// and fixed fonts in the presentation context, see nsPresContext.h
const uint8_t kGenericFont_NONE         = 0x00;
// Special
const uint8_t kGenericFont_moz_variable = 0x00; // for the default variable width font
const uint8_t kGenericFont_moz_fixed    = 0x01; // our special "use the user's fixed font"
// CSS
const uint8_t kGenericFont_serif        = 0x02;
const uint8_t kGenericFont_sans_serif   = 0x04;
const uint8_t kGenericFont_monospace    = 0x08;
const uint8_t kGenericFont_cursive      = 0x10;
const uint8_t kGenericFont_fantasy      = 0x20;

// Font structure.
struct nsFont {

  // list of font families, either named or generic
  mozilla::FontFamilyList fontlist;

  // The style of font (normal, italic, oblique; see gfxFontConstants.h)
  uint8_t style;

  // Force this font to not be considered a 'generic' font, even if
  // the name is the same as a CSS generic font family.
  bool systemFont;

  // Variant subproperties
  uint8_t variantCaps;
  uint8_t variantNumeric;
  uint8_t variantPosition;

  uint16_t variantLigatures;
  uint16_t variantEastAsian;

  // Some font-variant-alternates property values require
  // font-specific settings defined via @font-feature-values rules.
  // These are resolved *after* font matching occurs.

  // -- bitmask for both enumerated and functional propvals
  uint16_t variantAlternates;

  // Smoothing - controls subpixel-antialiasing (currently OSX only)
  uint8_t smoothing;

  // The weight of the font; see gfxFontConstants.h.
  uint16_t weight;

  // The stretch of the font (the sum of various NS_FONT_STRETCH_*
  // constants; see gfxFontConstants.h).
  int16_t stretch;

  // Kerning
  uint8_t kerning;

  // Synthesis setting, controls use of fake bolding/italics
  uint8_t synthesis;

  // The logical size of the font, in nscoord units
  nscoord size;

  // The aspect-value (ie., the ratio actualsize:actualxheight) that any
  // actual physical font created from this font structure must have when
  // rendering or measuring a string. A value of -1.0 means no adjustment
  // needs to be done; otherwise the value must be nonnegative.
  float sizeAdjust;

  // -- list of value tags for font-specific alternate features
  nsTArray<gfxAlternateValue> alternateValues;

  // -- object used to look these up once the font is matched
  RefPtr<gfxFontFeatureValueSet> featureValueLookup;

  // Font features from CSS font-feature-settings
  nsTArray<gfxFontFeature> fontFeatureSettings;

  // Language system tag, to override document language;
  // this is an OpenType "language system" tag represented as a 32-bit integer
  // (see http://www.microsoft.com/typography/otspec/languagetags.htm).
  nsString languageOverride;

  // initialize the font with a fontlist
  nsFont(const mozilla::FontFamilyList& aFontlist, nscoord aSize);

  // initialize the font with a single generic
  nsFont(mozilla::FontFamilyType aGenericType, nscoord aSize);

  // Make a copy of the given font
  nsFont(const nsFont& aFont);

  // leave members uninitialized
  nsFont();

  ~nsFont();

  bool operator==(const nsFont& aOther) const {
    return Equals(aOther);
  }

  bool operator!=(const nsFont& aOther) const {
    return !Equals(aOther);
  }

  bool Equals(const nsFont& aOther) const;

  nsFont& operator=(const nsFont& aOther);

  void CopyAlternates(const nsFont& aOther);

  // Add featureSettings into style
  void AddFontFeaturesToStyle(gfxFontStyle *aStyle) const;

protected:
  void Init(); // helper method for initialization
};

#define NS_FONT_VARIANT_NORMAL            0
#define NS_FONT_VARIANT_SMALL_CAPS        1

#endif /* nsFont_h___ */
