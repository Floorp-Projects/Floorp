/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPresData.h"

#include "mozilla/Preferences.h"
#include "mozilla/ServoBindings.h"
#include "nsPresContext.h"
namespace mozilla {

static StaticPresData* sSingleton = nullptr;

void
StaticPresData::Init()
{
  MOZ_ASSERT(!sSingleton);
  sSingleton = new StaticPresData();
}

void
StaticPresData::Shutdown()
{
  MOZ_ASSERT(sSingleton);
  delete sSingleton;
  sSingleton = nullptr;
}

StaticPresData*
StaticPresData::Get()
{
  MOZ_ASSERT(sSingleton);
  return sSingleton;
}

StaticPresData::StaticPresData()
{
  mLangService = nsLanguageAtomService::GetService();

  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THIN] = nsPresContext::CSSPixelsToAppUnits(1);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_MEDIUM] = nsPresContext::CSSPixelsToAppUnits(3);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THICK] = nsPresContext::CSSPixelsToAppUnits(5);
}

#define MAKE_FONT_PREF_KEY(_pref, _s0, _s1) \
 _pref.Assign(_s0); \
 _pref.Append(_s1);

static const char* const kGenericFont[] = {
  ".variable.",
  ".fixed.",
  ".serif.",
  ".sans-serif.",
  ".monospace.",
  ".cursive.",
  ".fantasy."
};

// These are private, use the list in nsFont.h if you want a public list.
enum {
  eDefaultFont_Variable,
  eDefaultFont_Fixed,
  eDefaultFont_Serif,
  eDefaultFont_SansSerif,
  eDefaultFont_Monospace,
  eDefaultFont_Cursive,
  eDefaultFont_Fantasy,
  eDefaultFont_COUNT
};

void
LangGroupFontPrefs::Initialize(nsIAtom* aLangGroupAtom)
{
  mLangGroup = aLangGroupAtom;

  /* Fetch the font prefs to be used -- see bug 61883 for details.
     Not all prefs are needed upfront. Some are fallback prefs intended
     for the GFX font sub-system...

  1) unit : assumed to be the same for all language groups -------------
  font.size.unit = px | pt    XXX could be folded in the size... bug 90440

  2) attributes for generic fonts --------------------------------------
  font.default.[langGroup] = serif | sans-serif - fallback generic font
  font.name.[generic].[langGroup] = current user' selected font on the pref dialog
  font.name-list.[generic].[langGroup] = fontname1, fontname2, ... [factory pre-built list]
  font.size.[generic].[langGroup] = integer - settable by the user
  font.size-adjust.[generic].[langGroup] = "float" - settable by the user
  font.minimum-size.[langGroup] = integer - settable by the user
  */

  nsAutoCString langGroup;
  aLangGroupAtom->ToUTF8String(langGroup);

  mDefaultVariableFont.size = nsPresContext::CSSPixelsToAppUnits(16);
  mDefaultFixedFont.size = nsPresContext::CSSPixelsToAppUnits(13);

  nsAutoCString pref;

  // get the current applicable font-size unit
  enum {eUnit_unknown = -1, eUnit_px, eUnit_pt};
  int32_t unit = eUnit_px;

  nsAutoCString cvalue;
  Preferences::GetCString("font.size.unit", cvalue);

  if (!cvalue.IsEmpty()) {
    if (cvalue.EqualsLiteral("px")) {
      unit = eUnit_px;
    }
    else if (cvalue.EqualsLiteral("pt")) {
      unit = eUnit_pt;
    }
    else {
      // XXX should really send this warning to the user (Error Console?).
      // And just default to unit = eUnit_px?
      NS_WARNING("unexpected font-size unit -- expected: 'px' or 'pt'");
      unit = eUnit_unknown;
    }
  }

  // get font.minimum-size.[langGroup]

  MAKE_FONT_PREF_KEY(pref, "font.minimum-size.", langGroup);

  int32_t size = Preferences::GetInt(pref.get());
  if (unit == eUnit_px) {
    mMinimumFontSize = nsPresContext::CSSPixelsToAppUnits(size);
  }
  else if (unit == eUnit_pt) {
    mMinimumFontSize = nsPresContext::CSSPointsToAppUnits(size);
  }

  nsFont* fontTypes[] = {
    &mDefaultVariableFont,
    &mDefaultFixedFont,
    &mDefaultSerifFont,
    &mDefaultSansSerifFont,
    &mDefaultMonospaceFont,
    &mDefaultCursiveFont,
    &mDefaultFantasyFont
  };
  static_assert(MOZ_ARRAY_LENGTH(fontTypes) == eDefaultFont_COUNT,
                "FontTypes array count is not correct");

  // Get attributes specific to each generic font. We do not get the user's
  // generic-font-name-to-specific-family-name preferences because its the
  // generic name that should be fed into the cascade. It is up to the GFX
  // code to look up the font prefs to convert generic names to specific
  // family names as necessary.
  nsAutoCString generic_dot_langGroup;
  for (uint32_t eType = 0; eType < ArrayLength(fontTypes); ++eType) {
    generic_dot_langGroup.Assign(kGenericFont[eType]);
    generic_dot_langGroup.Append(langGroup);

    nsFont* font = fontTypes[eType];

    // set the default variable font (the other fonts are seen as 'generic' fonts
    // in GFX and will be queried there when hunting for alternative fonts)
    if (eType == eDefaultFont_Variable) {
      // XXX "font.name.variable."?  There is no such pref...
      MAKE_FONT_PREF_KEY(pref, "font.name.variable.", langGroup);

      nsAutoString value;
      Preferences::GetString(pref.get(), value);
      if (!value.IsEmpty()) {
        FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
        FontFamilyType defaultType = defaultVariableName.mType;
        NS_ASSERTION(defaultType == eFamily_serif ||
                     defaultType == eFamily_sans_serif,
                     "default type must be serif or sans-serif");
        mDefaultVariableFont.fontlist = FontFamilyList(defaultType);
      }
      else {
        MAKE_FONT_PREF_KEY(pref, "font.default.", langGroup);
        Preferences::GetString(pref.get(), value);
        if (!value.IsEmpty()) {
          FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
          FontFamilyType defaultType = defaultVariableName.mType;
          NS_ASSERTION(defaultType == eFamily_serif ||
                       defaultType == eFamily_sans_serif,
                       "default type must be serif or sans-serif");
          mDefaultVariableFont.fontlist = FontFamilyList(defaultType);
        }
      }
    }
    else {
      if (eType == eDefaultFont_Monospace) {
        // This takes care of the confusion whereby people often expect "monospace"
        // to have the same default font-size as "-moz-fixed" (this tentative
        // size may be overwritten with the specific value for "monospace" when
        // "font.size.monospace.[langGroup]" is read -- see below)
        mDefaultMonospaceFont.size = mDefaultFixedFont.size;
      }
      else if (eType != eDefaultFont_Fixed) {
        // all the other generic fonts are initialized with the size of the
        // variable font, but their specific size can supersede later -- see below
        font->size = mDefaultVariableFont.size;
      }
    }

    // Bug 84398: for spec purists, a different font-size only applies to the
    // .variable. and .fixed. fonts and the other fonts should get |font-size-adjust|.
    // The problem is that only GfxWin has the support for |font-size-adjust|. So for
    // parity, we enable the ability to set a different font-size on all platforms.

    // get font.size.[generic].[langGroup]
    // size=0 means 'Auto', i.e., generic fonts retain the size of the variable font
    MAKE_FONT_PREF_KEY(pref, "font.size", generic_dot_langGroup);
    size = Preferences::GetInt(pref.get());
    if (size > 0) {
      if (unit == eUnit_px) {
        font->size = nsPresContext::CSSPixelsToAppUnits(size);
      }
      else if (unit == eUnit_pt) {
        font->size = nsPresContext::CSSPointsToAppUnits(size);
      }
    }

    // get font.size-adjust.[generic].[langGroup]
    // XXX only applicable on GFX ports that handle |font-size-adjust|
    MAKE_FONT_PREF_KEY(pref, "font.size-adjust", generic_dot_langGroup);
    cvalue.Truncate();
    Preferences::GetCString(pref.get(), cvalue);
    if (!cvalue.IsEmpty()) {
      font->sizeAdjust = (float)atof(cvalue.get());
    }

#ifdef DEBUG_rbs
    printf("%s Family-list:%s size:%d sizeAdjust:%.2f\n",
           generic_dot_langGroup.get(),
           NS_ConvertUTF16toUTF8(font->name).get(), font->size,
           font->sizeAdjust);
#endif
  }
}

nsIAtom*
StaticPresData::GetLangGroup(nsIAtom* aLanguage,
                             bool* aNeedsToCache) const
{
  nsIAtom* langGroupAtom = nullptr;
  langGroupAtom = mLangService->GetLanguageGroup(aLanguage, aNeedsToCache);
  if (!langGroupAtom) {
    langGroupAtom = nsGkAtoms::x_western; // Assume x-western is safe...
  }
  return langGroupAtom;
}

already_AddRefed<nsIAtom>
StaticPresData::GetUncachedLangGroup(nsIAtom* aLanguage) const
{
  nsCOMPtr<nsIAtom> langGroupAtom = mLangService->GetUncachedLanguageGroup(aLanguage);
  if (!langGroupAtom) {
    langGroupAtom = nsGkAtoms::x_western; // Assume x-western is safe...
  }
  return langGroupAtom.forget();
}

const LangGroupFontPrefs*
StaticPresData::GetFontPrefsForLangHelper(nsIAtom* aLanguage,
                                          const LangGroupFontPrefs* aPrefs,
                                          bool* aNeedsToCache) const
{
  // Get language group for aLanguage:
  MOZ_ASSERT(aLanguage);
  MOZ_ASSERT(mLangService);
  MOZ_ASSERT(aPrefs);

  nsIAtom* langGroupAtom = GetLangGroup(aLanguage, aNeedsToCache);

  if (aNeedsToCache && *aNeedsToCache) {
    return nullptr;
  }

  LangGroupFontPrefs* prefs = const_cast<LangGroupFontPrefs*>(aPrefs);
  if (prefs->mLangGroup) { // if initialized
    DebugOnly<uint32_t> count = 0;
    for (;;) {
      NS_ASSERTION(++count < 35, "Lang group count exceeded!!!");
      if (prefs->mLangGroup == langGroupAtom) {
        return prefs;
      }
      if (!prefs->mNext) {
        break;
      }
      prefs = prefs->mNext;
    }
    if (aNeedsToCache) {
      *aNeedsToCache = true;
      return nullptr;
    }
    AssertIsMainThreadOrServoLangFontPrefsCacheLocked();
    // nothing cached, so go on and fetch the prefs for this lang group:
    prefs = prefs->mNext = new LangGroupFontPrefs;
  }

  if (aNeedsToCache) {
    *aNeedsToCache = true;
    return nullptr;
  }

  AssertIsMainThreadOrServoLangFontPrefsCacheLocked();
  prefs->Initialize(langGroupAtom);

  return prefs;
}

const nsFont*
StaticPresData::GetDefaultFontHelper(uint8_t aFontID, nsIAtom *aLanguage,
                                     const LangGroupFontPrefs* aPrefs) const
{
  MOZ_ASSERT(aLanguage);
  MOZ_ASSERT(aPrefs);

  const nsFont *font;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID:
      font = &aPrefs->mDefaultVariableFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      font = &aPrefs->mDefaultFixedFont;
      break;
    // CSS
    case kGenericFont_serif:
      font = &aPrefs->mDefaultSerifFont;
      break;
    case kGenericFont_sans_serif:
      font = &aPrefs->mDefaultSansSerifFont;
      break;
    case kGenericFont_monospace:
      font = &aPrefs->mDefaultMonospaceFont;
      break;
    case kGenericFont_cursive:
      font = &aPrefs->mDefaultCursiveFont;
      break;
    case kGenericFont_fantasy:
      font = &aPrefs->mDefaultFantasyFont;
      break;
    default:
      font = nullptr;
      NS_ERROR("invalid arg");
      break;
  }
  return font;
}


} // namespace mozilla
