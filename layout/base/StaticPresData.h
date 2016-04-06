/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPresData_h
#define mozilla_StaticPresData_h

#include "nsAutoPtr.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsFont.h"
#include "nsIAtom.h"
#include "nsILanguageAtomService.h"

namespace mozilla {

struct LangGroupFontPrefs {
  // Font sizes default to zero; they will be set in GetFontPreferences
  LangGroupFontPrefs()
    : mLangGroup(nullptr)
    , mMinimumFontSize(0)
    , mDefaultVariableFont(mozilla::eFamily_serif, 0)
    , mDefaultFixedFont(mozilla::eFamily_monospace, 0)
    , mDefaultSerifFont(mozilla::eFamily_serif, 0)
    , mDefaultSansSerifFont(mozilla::eFamily_sans_serif, 0)
    , mDefaultMonospaceFont(mozilla::eFamily_monospace, 0)
    , mDefaultCursiveFont(mozilla::eFamily_cursive, 0)
    , mDefaultFantasyFont(mozilla::eFamily_fantasy, 0)
  {}

  void Reset()
  {
    // Throw away any other LangGroupFontPrefs objects:
    mNext = nullptr;

    // Make GetFontPreferences reinitialize mLangGroupFontPrefs:
    mLangGroup = nullptr;
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;
    LangGroupFontPrefs* curr = mNext;
    while (curr) {
      n += aMallocSizeOf(curr);

      // Measurement of the following members may be added later if DMD finds
      // it is worthwhile:
      // - mLangGroup
      // - mDefault*Font

      curr = curr->mNext;
    }
    return n;
  }

  nsCOMPtr<nsIAtom> mLangGroup;
  nscoord mMinimumFontSize;
  nsFont mDefaultVariableFont;
  nsFont mDefaultFixedFont;
  nsFont mDefaultSerifFont;
  nsFont mDefaultSansSerifFont;
  nsFont mDefaultMonospaceFont;
  nsFont mDefaultCursiveFont;
  nsFont mDefaultFantasyFont;
  nsAutoPtr<LangGroupFontPrefs> mNext;
};

/**
 * Some functionality that has historically lived on nsPresContext does not
 * actually need to be per-document. This singleton class serves as a host
 * for that functionality. We delegate to it from nsPresContext where
 * appropriate, and use it standalone in some cases as well.
 */
class StaticPresData
{
public:
  // Initialization and shutdown of the singleton. Called exactly once.
  static void Init();
  static void Shutdown();

  // Gets an instance of the singleton. Infallible between the calls to Init
  // and Shutdown.
  static StaticPresData* Get();

  /**
   * This table maps border-width enums 'thin', 'medium', 'thick'
   * to actual nscoord values.
   */
  const nscoord* GetBorderWidthTable() { return mBorderWidthTable; }

  /**
   * Fetch the user's font preferences for the given aLanguage's
   * langugage group.
   *
   * The original code here is pretty old, and includes an optimization
   * whereby language-specific prefs are read per-document, and the
   * results are stored in a linked list, which is assumed to be very short
   * since most documents only ever use one language.
   *
   * Storing this per-session rather than per-document would almost certainly
   * be fine. But just to be on the safe side, we leave the old mechanism as-is,
   * with an additional per-session cache that new callers can use if they don't
   * have a PresContext.
   */
  const LangGroupFontPrefs* GetFontPrefsForLangHelper(nsIAtom* aLanguage,
                                                      const LangGroupFontPrefs* aPrefs) const;
  /**
   * Get the default font for the given language and generic font ID.
   * aLanguage may not be nullptr.
   *
   * This object is read-only, you must copy the font to modify it.
   *
   * When aFontID is kPresContext_DefaultVariableFontID or
   * kPresContext_DefaultFixedFontID (which equals
   * kGenericFont_moz_fixed, which is used for the -moz-fixed generic),
   * the nsFont returned has its name as a CSS generic family (serif or
   * sans-serif for the former, monospace for the latter), and its size
   * as the default font size for variable or fixed fonts for the
   * language group.
   *
   * For aFontID corresponding to a CSS Generic, the nsFont returned has
   * its name set to that generic font's name, and its size set to
   * the user's preference for font size for that generic and the
   * given language.
   */
  const nsFont* GetDefaultFontHelper(uint8_t aFontID,
                                     nsIAtom* aLanguage,
                                     const LangGroupFontPrefs* aPrefs) const;

  /*
   * These versions operate on the font pref cache on StaticPresData.
   */

  const nsFont* GetDefaultFont(uint8_t aFontID, nsIAtom* aLanguage) const
  {
    MOZ_ASSERT(aLanguage);
    return GetDefaultFontHelper(aFontID, aLanguage, GetFontPrefsForLang(aLanguage));
  }
  const LangGroupFontPrefs* GetFontPrefsForLang(nsIAtom* aLanguage) const
  {
    MOZ_ASSERT(aLanguage);
    return GetFontPrefsForLangHelper(aLanguage, &mStaticLangGroupFontPrefs);
  }

  void ResetCachedFontPrefs() { mStaticLangGroupFontPrefs.Reset(); }

private:
  StaticPresData();
  ~StaticPresData() {}

  nsCOMPtr<nsILanguageAtomService> mLangService;
  nscoord mBorderWidthTable[3];
  LangGroupFontPrefs mStaticLangGroupFontPrefs;
};

} // namespace mozilla

#endif // mozilla_StaticPresData_h
