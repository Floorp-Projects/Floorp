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

void StaticPresData::Init() {
  MOZ_ASSERT(!sSingleton);
  sSingleton = new StaticPresData();
}

void StaticPresData::Shutdown() {
  MOZ_ASSERT(sSingleton);
  delete sSingleton;
  sSingleton = nullptr;
}

StaticPresData* StaticPresData::Get() {
  MOZ_ASSERT(sSingleton);
  return sSingleton;
}

StaticPresData::StaticPresData() {
  mLangService = nsLanguageAtomService::GetService();
}

#define MAKE_FONT_PREF_KEY(_pref, _s0, _s1) \
  _pref.Assign(_s0);                        \
  _pref.Append(_s1);

// clang-format off
static const char* const kGenericFont[] = {
  ".variable.",
  ".serif.",
  ".sans-serif.",
  ".monospace.",
  ".cursive.",
  ".fantasy."
};
// clang-format on

// These are private, use the list in nsFont.h if you want a public list.
enum {
  eDefaultFont_Variable,
  eDefaultFont_Serif,
  eDefaultFont_SansSerif,
  eDefaultFont_Monospace,
  eDefaultFont_Cursive,
  eDefaultFont_Fantasy,
  eDefaultFont_COUNT
};

void LangGroupFontPrefs::Initialize(nsStaticAtom* aLangGroupAtom) {
  mLangGroup = aLangGroupAtom;

  /* Fetch the font prefs to be used -- see bug 61883 for details.
     Not all prefs are needed upfront. Some are fallback prefs intended
     for the GFX font sub-system...

  -- attributes for generic fonts --------------------------------------

  font.default.[langGroup] = serif | sans-serif
    fallback generic font

  font.name.[generic].[langGroup]
    current user' selected font on the pref dialog

  font.name-list.[generic].[langGroup] = fontname1, fontname2, ...
    [factory pre-built list]

  font.size.[generic].[langGroup] = integer
    settable by the user

  font.size-adjust.[generic].[langGroup] = "float"
    settable by the user

  font.minimum-size.[langGroup] = integer
    settable by the user
  */

  nsAutoCString langGroup;
  aLangGroupAtom->ToUTF8String(langGroup);

  mDefaultVariableFont.size = nsPresContext::CSSPixelsToAppUnits(16);
  mDefaultMonospaceFont.size = nsPresContext::CSSPixelsToAppUnits(13);

  nsAutoCString pref;

  // get font.minimum-size.[langGroup]

  MAKE_FONT_PREF_KEY(pref, "font.minimum-size.", langGroup);

  int32_t size = Preferences::GetInt(pref.get());
  mMinimumFontSize = nsPresContext::CSSPixelsToAppUnits(size);

  // clang-format off
  nsFont* fontTypes[] = {
    &mDefaultVariableFont,
    &mDefaultSerifFont,
    &mDefaultSansSerifFont,
    &mDefaultMonospaceFont,
    &mDefaultCursiveFont,
    &mDefaultFantasyFont
  };
  // clang-format on
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

    // set the default variable font (the other fonts are seen as 'generic'
    // fonts in GFX and will be queried there when hunting for alternative
    // fonts)
    if (eType == eDefaultFont_Variable) {
      // XXX "font.name.variable."?  There is no such pref...
      MAKE_FONT_PREF_KEY(pref, "font.name.variable.", langGroup);

      nsAutoCString value;
      Preferences::GetCString(pref.get(), value);
      if (!value.IsEmpty()) {
        FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
        StyleGenericFontFamily defaultType = defaultVariableName.mGeneric;
        NS_ASSERTION(defaultType == StyleGenericFontFamily::Serif ||
                         defaultType == StyleGenericFontFamily::SansSerif,
                     "default type must be serif or sans-serif");
        mDefaultVariableFont.fontlist = FontFamilyList();
        mDefaultVariableFont.fontlist.SetDefaultFontType(defaultType);
        // We create mDefaultVariableFont.fontlist with defaultType as the
        // fallback font, and not as part of the font list proper. This way,
        // it can be overwritten should there be a language change.
      } else {
        MAKE_FONT_PREF_KEY(pref, "font.default.", langGroup);
        Preferences::GetCString(pref.get(), value);
        if (!value.IsEmpty()) {
          FontFamilyName defaultVariableName = FontFamilyName::Convert(value);
          StyleGenericFontFamily defaultType = defaultVariableName.mGeneric;
          NS_ASSERTION(defaultType == StyleGenericFontFamily::Serif ||
                           defaultType == StyleGenericFontFamily::SansSerif,
                       "default type must be serif or sans-serif");
          mDefaultVariableFont.fontlist = FontFamilyList();
          mDefaultVariableFont.fontlist.SetDefaultFontType(defaultType);
          // We create mDefaultVariableFont.fontlist with defaultType as the
          // (fallback) font, and not as part of the font list proper. This way,
          // it can be overwritten should there be a language change.
        }
      }
    } else {
      if (eType != eDefaultFont_Monospace) {
        // all the other generic fonts are initialized with the size of the
        // variable font, but their specific size can supersede later -- see
        // below
        font->size = mDefaultVariableFont.size;
      }
    }

    // Bug 84398: for spec purists, a different font-size only applies to the
    // .variable. and .fixed. fonts and the other fonts should get
    // |font-size-adjust|. The problem is that only GfxWin has the support for
    // |font-size-adjust|. So for parity, we enable the ability to set a
    // different font-size on all platforms.

    // get font.size.[generic].[langGroup]
    // size=0 means 'Auto', i.e., generic fonts retain the size of the variable
    // font
    MAKE_FONT_PREF_KEY(pref, "font.size", generic_dot_langGroup);
    size = Preferences::GetInt(pref.get());
    if (size > 0) {
      font->size = nsPresContext::CSSPixelsToAppUnits(size);
    }

    // get font.size-adjust.[generic].[langGroup]
    // XXX only applicable on GFX ports that handle |font-size-adjust|
    MAKE_FONT_PREF_KEY(pref, "font.size-adjust", generic_dot_langGroup);
    nsAutoCString cvalue;
    Preferences::GetCString(pref.get(), cvalue);
    if (!cvalue.IsEmpty()) {
      font->sizeAdjust = (float)atof(cvalue.get());
    }

#ifdef DEBUG_rbs
    printf("%s Family-list:%s size:%d sizeAdjust:%.2f\n",
           generic_dot_langGroup.get(), NS_ConvertUTF16toUTF8(font->name).get(),
           font->size, font->sizeAdjust);
#endif
  }
}

nsStaticAtom* StaticPresData::GetLangGroup(nsAtom* aLanguage,
                                           bool* aNeedsToCache) const {
  nsStaticAtom* langGroupAtom =
      mLangService->GetLanguageGroup(aLanguage, aNeedsToCache);
  // Assume x-western is safe...
  return langGroupAtom ? langGroupAtom : nsGkAtoms::x_western;
}

nsStaticAtom* StaticPresData::GetUncachedLangGroup(nsAtom* aLanguage) const {
  nsStaticAtom* langGroupAtom =
      mLangService->GetUncachedLanguageGroup(aLanguage);
  return langGroupAtom ? langGroupAtom : nsGkAtoms::x_western;
}

const LangGroupFontPrefs* StaticPresData::GetFontPrefsForLang(
    nsAtom* aLanguage, bool* aNeedsToCache) {
  // Get language group for aLanguage:
  MOZ_ASSERT(aLanguage);
  MOZ_ASSERT(mLangService);

  nsStaticAtom* langGroupAtom = GetLangGroup(aLanguage, aNeedsToCache);
  if (aNeedsToCache && *aNeedsToCache) {
    return nullptr;
  }

  if (!aNeedsToCache) {
    AssertIsMainThreadOrServoFontMetricsLocked();
  }

  LangGroupFontPrefs* prefs = &mLangGroupFontPrefs;
  if (prefs->mLangGroup) {  // if initialized
    DebugOnly<uint32_t> count = 0;
    for (;;) {
      if (prefs->mLangGroup == langGroupAtom) {
        return prefs;
      }
      if (!prefs->mNext) {
        break;
      }
      prefs = prefs->mNext.get();
    }
    if (aNeedsToCache) {
      *aNeedsToCache = true;
      return nullptr;
    }
    // nothing cached, so go on and fetch the prefs for this lang group:
    prefs->mNext = MakeUnique<LangGroupFontPrefs>();
    prefs = prefs->mNext.get();
  }

  if (aNeedsToCache) {
    *aNeedsToCache = true;
    return nullptr;
  }

  AssertIsMainThreadOrServoFontMetricsLocked();
  prefs->Initialize(langGroupAtom);

  return prefs;
}

}  // namespace mozilla
