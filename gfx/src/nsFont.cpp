/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFont.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "gfxFont.h"

nsFont::nsFont(const char* aName, uint8_t aStyle, uint8_t aVariant,
               uint16_t aWeight, int16_t aStretch, uint8_t aDecoration,
               nscoord aSize)
{
  NS_ASSERTION(aName && IsASCII(nsDependentCString(aName)),
               "Must only pass ASCII names here");
  name.AssignASCII(aName);
  style = aStyle;
  systemFont = false;
  variant = aVariant;
  weight = aWeight;
  stretch = aStretch;
  decorations = aDecoration;
  size = aSize;
  sizeAdjust = 0.0;
  kerning = NS_FONT_KERNING_AUTO;
  synthesis = NS_FONT_SYNTHESIS_WEIGHT | NS_FONT_SYNTHESIS_STYLE;

  variantAlternates = 0;
  variantCaps = NS_FONT_VARIANT_CAPS_NORMAL;
  variantEastAsian = 0;
  variantLigatures = 0;
  variantNumeric = 0;
  variantPosition = NS_FONT_VARIANT_POSITION_NORMAL;
}

nsFont::nsFont(const nsString& aName, uint8_t aStyle, uint8_t aVariant,
               uint16_t aWeight, int16_t aStretch, uint8_t aDecoration,
               nscoord aSize)
  : name(aName)
{
  style = aStyle;
  systemFont = false;
  variant = aVariant;
  weight = aWeight;
  stretch = aStretch;
  decorations = aDecoration;
  size = aSize;
  sizeAdjust = 0.0;
  kerning = NS_FONT_KERNING_AUTO;
  synthesis = NS_FONT_SYNTHESIS_WEIGHT | NS_FONT_SYNTHESIS_STYLE;

  variantAlternates = 0;
  variantCaps = NS_FONT_VARIANT_CAPS_NORMAL;
  variantEastAsian = 0;
  variantLigatures = 0;
  variantNumeric = 0;
  variantPosition = NS_FONT_VARIANT_POSITION_NORMAL;
}

nsFont::nsFont(const nsFont& aOther)
  : name(aOther.name)
{
  style = aOther.style;
  systemFont = aOther.systemFont;
  variant = aOther.variant;
  weight = aOther.weight;
  stretch = aOther.stretch;
  decorations = aOther.decorations;
  size = aOther.size;
  sizeAdjust = aOther.sizeAdjust;
  kerning = aOther.kerning;
  synthesis = aOther.synthesis;
  fontFeatureSettings = aOther.fontFeatureSettings;
  languageOverride = aOther.languageOverride;
  variantAlternates = aOther.variantAlternates;
  variantCaps = aOther.variantCaps;
  variantEastAsian = aOther.variantEastAsian;
  variantLigatures = aOther.variantLigatures;
  variantNumeric = aOther.variantNumeric;
  variantPosition = aOther.variantPosition;
}

nsFont::nsFont()
{
}

nsFont::~nsFont()
{
}

bool nsFont::BaseEquals(const nsFont& aOther) const
{
  if ((style == aOther.style) &&
      (systemFont == aOther.systemFont) &&
      (weight == aOther.weight) &&
      (stretch == aOther.stretch) &&
      (size == aOther.size) &&
      (sizeAdjust == aOther.sizeAdjust) &&
      name.Equals(aOther.name, nsCaseInsensitiveStringComparator()) &&
      (kerning == aOther.kerning) &&
      (synthesis == aOther.synthesis) &&
      (fontFeatureSettings == aOther.fontFeatureSettings) &&
      (languageOverride == aOther.languageOverride) &&
      (variantAlternates == aOther.variantAlternates) &&
      (variantCaps == aOther.variantCaps) &&
      (variantEastAsian == aOther.variantEastAsian) &&
      (variantLigatures == aOther.variantLigatures) &&
      (variantNumeric == aOther.variantNumeric) &&
      (variantPosition == aOther.variantPosition)) {
    return true;
  }
  return false;
}

bool nsFont::Equals(const nsFont& aOther) const
{
  if (BaseEquals(aOther) &&
      (variant == aOther.variant) &&
      (decorations == aOther.decorations)) {
    return true;
  }
  return false;
}

nsFont& nsFont::operator=(const nsFont& aOther)
{
  name = aOther.name;
  style = aOther.style;
  systemFont = aOther.systemFont;
  variant = aOther.variant;
  weight = aOther.weight;
  stretch = aOther.stretch;
  decorations = aOther.decorations;
  size = aOther.size;
  sizeAdjust = aOther.sizeAdjust;
  kerning = aOther.kerning;
  synthesis = aOther.synthesis;
  fontFeatureSettings = aOther.fontFeatureSettings;
  languageOverride = aOther.languageOverride;
  variantAlternates = aOther.variantAlternates;
  variantCaps = aOther.variantCaps;
  variantEastAsian = aOther.variantEastAsian;
  variantLigatures = aOther.variantLigatures;
  variantNumeric = aOther.variantNumeric;
  variantPosition = aOther.variantPosition;
  return *this;
}

void
nsFont::AddFontFeaturesToStyle(gfxFontStyle *aStyle) const
{
  // simple copy for now, font-variant implementation will expand
  aStyle->featureSettings.AppendElements(fontFeatureSettings);
}

static bool IsGenericFontFamily(const nsString& aFamily)
{
  uint8_t generic;
  nsFont::GetGenericID(aFamily, &generic);
  return generic != kGenericFont_NONE;
}

const PRUnichar kNullCh       = PRUnichar('\0');
const PRUnichar kSingleQuote  = PRUnichar('\'');
const PRUnichar kDoubleQuote  = PRUnichar('\"');
const PRUnichar kComma        = PRUnichar(',');

bool nsFont::EnumerateFamilies(nsFontFamilyEnumFunc aFunc, void* aData) const
{
  const PRUnichar *p, *p_end;
  name.BeginReading(p);
  name.EndReading(p_end);
  nsAutoString family;

  while (p < p_end) {
    while (nsCRT::IsAsciiSpace(*p))
      if (++p == p_end)
        return true;

    bool generic;
    if (*p == kSingleQuote || *p == kDoubleQuote) {
      // quoted font family
      PRUnichar quoteMark = *p;
      if (++p == p_end)
        return true;
      const PRUnichar *nameStart = p;

      // XXX What about CSS character escapes?
      while (*p != quoteMark)
        if (++p == p_end)
          return true;

      family = Substring(nameStart, p);
      generic = false;

      while (++p != p_end && *p != kComma)
        /* nothing */ ;

    } else {
      // unquoted font family
      const PRUnichar *nameStart = p;
      while (++p != p_end && *p != kComma)
        /* nothing */ ;

      family = Substring(nameStart, p);
      family.CompressWhitespace(false, true);
      generic = IsGenericFontFamily(family);
    }

    if (!family.IsEmpty() && !(*aFunc)(family, generic, aData))
      return false;

    ++p; // may advance past p_end
  }

  return true;
}

static bool FontEnumCallback(const nsString& aFamily, bool aGeneric, void *aData)
{
  *((nsString*)aData) = aFamily;
  return false;
}

void nsFont::GetFirstFamily(nsString& aFamily) const
{
  EnumerateFamilies(FontEnumCallback, &aFamily);
}

/*static*/
void nsFont::GetGenericID(const nsString& aGeneric, uint8_t* aID)
{
  *aID = kGenericFont_NONE;
  if (aGeneric.LowerCaseEqualsLiteral("-moz-fixed"))      *aID = kGenericFont_moz_fixed;
  else if (aGeneric.LowerCaseEqualsLiteral("serif"))      *aID = kGenericFont_serif;
  else if (aGeneric.LowerCaseEqualsLiteral("sans-serif")) *aID = kGenericFont_sans_serif;
  else if (aGeneric.LowerCaseEqualsLiteral("cursive"))    *aID = kGenericFont_cursive;
  else if (aGeneric.LowerCaseEqualsLiteral("fantasy"))    *aID = kGenericFont_fantasy;
  else if (aGeneric.LowerCaseEqualsLiteral("monospace"))  *aID = kGenericFont_monospace;
}
