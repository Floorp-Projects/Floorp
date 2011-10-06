/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFont_h___
#define nsFont_h___

#include "gfxCore.h"
#include "nsCoord.h"
#include "nsStringGlue.h"
#include "gfxFontConstants.h"

// XXX we need a method to enumerate all of the possible fonts on the
// system across family, weight, style, size, etc. But not here!

// Enumerator callback function. Return PR_FALSE to stop
typedef bool (*nsFontFamilyEnumFunc)(const nsString& aFamily, bool aGeneric, void *aData);

// IDs for generic fonts
// NOTE: 0, 1 are reserved for the special IDs of the default variable
// and fixed fonts in the presentation context, see nsPresContext.h
const PRUint8 kGenericFont_NONE         = 0x00;
// Special
const PRUint8 kGenericFont_moz_variable = 0x00; // for the default variable width font
const PRUint8 kGenericFont_moz_fixed    = 0x01; // our special "use the user's fixed font"
// CSS
const PRUint8 kGenericFont_serif        = 0x02;
const PRUint8 kGenericFont_sans_serif   = 0x04;
const PRUint8 kGenericFont_monospace    = 0x08;
const PRUint8 kGenericFont_cursive      = 0x10;
const PRUint8 kGenericFont_fantasy      = 0x20;

// Font structure.
struct NS_GFX nsFont {
  // The family name of the font
  nsString name;

  // The style of font (normal, italic, oblique; see gfxFontConstants.h)
  PRUint8 style;

  // Force this font to not be considered a 'generic' font, even if
  // the name is the same as a CSS generic font family.
  PRUint8 systemFont;

  // The variant of the font (normal, small-caps)
  PRUint8 variant;

  // The weight of the font; see gfxFontConstants.h.
  PRUint16 weight;

  // The stretch of the font (the sum of various NS_FONT_STRETCH_*
  // constants; see gfxFontConstants.h).
  PRInt16 stretch;

  // The decorations on the font (underline, overline,
  // line-through). The decorations can be binary or'd together.
  PRUint8 decorations;

  // The logical size of the font, in nscoord units
  nscoord size;

  // The aspect-value (ie., the ratio actualsize:actualxheight) that any
  // actual physical font created from this font structure must have when
  // rendering or measuring a string. A value of 0 means no adjustment
  // needs to be done.
  float sizeAdjust;

  // Font features from CSS font-feature-settings
  nsString featureSettings;

  // Language system tag, to override document language;
  // this is an OpenType "language system" tag represented as a 32-bit integer
  // (see http://www.microsoft.com/typography/otspec/languagetags.htm).
  nsString languageOverride;

  // Initialize the font struct with an ASCII name
  nsFont(const char* aName, PRUint8 aStyle, PRUint8 aVariant,
         PRUint16 aWeight, PRInt16 aStretch, PRUint8 aDecoration,
         nscoord aSize, float aSizeAdjust=0.0f,
         const nsString* aFeatureSettings = nsnull,
         const nsString* aLanguageOverride = nsnull);

  // Initialize the font struct with a (potentially) unicode name
  nsFont(const nsString& aName, PRUint8 aStyle, PRUint8 aVariant,
         PRUint16 aWeight, PRInt16 aStretch, PRUint8 aDecoration,
         nscoord aSize, float aSizeAdjust=0.0f,
         const nsString* aFeatureSettings = nsnull,
         const nsString* aLanguageOverride = nsnull);

  // Make a copy of the given font
  nsFont(const nsFont& aFont);

  nsFont();
  ~nsFont();

  bool operator==(const nsFont& aOther) const {
    return Equals(aOther);
  }

  bool Equals(const nsFont& aOther) const ;
  // Compare ignoring differences in 'variant' and 'decoration'
  bool BaseEquals(const nsFont& aOther) const;

  nsFont& operator=(const nsFont& aOther);

  // Utility method to interpret name string
  // enumerates all families specified by this font only
  // returns PR_TRUE if completed, PR_FALSE if stopped
  // enclosing quotes will be removed, and whitespace compressed (as needed)
  bool EnumerateFamilies(nsFontFamilyEnumFunc aFunc, void* aData) const;
  void GetFirstFamily(nsString& aFamily) const;

  // Utility method to return the ID of a generic font
  static void GetGenericID(const nsString& aGeneric, PRUint8* aID);
};

#define NS_FONT_VARIANT_NORMAL            0
#define NS_FONT_VARIANT_SMALL_CAPS        1

#define NS_FONT_DECORATION_NONE           0x0
#define NS_FONT_DECORATION_UNDERLINE      0x1
#define NS_FONT_DECORATION_OVERLINE       0x2
#define NS_FONT_DECORATION_LINE_THROUGH   0x4

#endif /* nsFont_h___ */
