/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPresData_h
#define mozilla_StaticPresData_h

#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsFont.h"
#include "nsAtom.h"
#include "nsLanguageAtomService.h"

namespace mozilla {

struct LangGroupFontPrefs {
  // Font sizes default to zero; they will be set in GetFontPreferences
  LangGroupFontPrefs()
      : mLangGroup(nullptr),
        mMinimumFontSize({0}),
        mDefaultVariableFont(StyleGenericFontFamily::Serif, {0}),
        mDefaultSerifFont(StyleGenericFontFamily::Serif, {0}),
        mDefaultSansSerifFont(StyleGenericFontFamily::SansSerif, {0}),
        mDefaultMonospaceFont(StyleGenericFontFamily::Monospace, {0}),
        mDefaultCursiveFont(StyleGenericFontFamily::Cursive, {0}),
        mDefaultFantasyFont(StyleGenericFontFamily::Fantasy, {0}),
        mDefaultSystemUiFont(StyleGenericFontFamily::SystemUi, {0}) {}

  StyleGenericFontFamily GetDefaultGeneric() const {
    return mDefaultVariableFont.family.families.list.AsSpan()[0].AsGeneric();
  }

  void Reset() {
    // Throw away any other LangGroupFontPrefs objects:
    mNext = nullptr;

    // Make GetFontPreferences reinitialize mLangGroupFontPrefs:
    mLangGroup = nullptr;
  }

  // Initialize this with the data for a given language
  void Initialize(nsStaticAtom* aLangGroupAtom);

  /**
   * Get the default font for the given language and generic font ID.
   * aLanguage may not be nullptr.
   *
   * This object is read-only, you must copy the font to modify it.
   *
   * For aFontID corresponding to a CSS Generic, the nsFont returned has
   * its name set to that generic font's name, and its size set to
   * the user's preference for font size for that generic and the
   * given language.
   */
  const nsFont* GetDefaultFont(StyleGenericFontFamily aFamily) const {
    switch (aFamily) {
      // Special (our default variable width font and fixed width font)
      case StyleGenericFontFamily::None:
        return &mDefaultVariableFont;
      // CSS
      case StyleGenericFontFamily::Serif:
        return &mDefaultSerifFont;
      case StyleGenericFontFamily::SansSerif:
        return &mDefaultSansSerifFont;
      case StyleGenericFontFamily::Monospace:
        return &mDefaultMonospaceFont;
      case StyleGenericFontFamily::Cursive:
        return &mDefaultCursiveFont;
      case StyleGenericFontFamily::Fantasy:
        return &mDefaultFantasyFont;
      case StyleGenericFontFamily::SystemUi:
        return &mDefaultSystemUiFont;
      case StyleGenericFontFamily::MozEmoji:
        // This shouldn't appear in font family names.
        break;
    }
    MOZ_ASSERT_UNREACHABLE("invalid font id");
    return nullptr;
  }

  nsStaticAtom* mLangGroup;
  Length mMinimumFontSize;
  nsFont mDefaultVariableFont;
  nsFont mDefaultSerifFont;
  nsFont mDefaultSansSerifFont;
  nsFont mDefaultMonospaceFont;
  nsFont mDefaultCursiveFont;
  nsFont mDefaultFantasyFont;
  nsFont mDefaultSystemUiFont;
  UniquePtr<LangGroupFontPrefs> mNext;
};

/**
 * Some functionality that has historically lived on nsPresContext does not
 * actually need to be per-document. This singleton class serves as a host
 * for that functionality. We delegate to it from nsPresContext where
 * appropriate, and use it standalone in some cases as well.
 */
class StaticPresData {
 public:
  // Initialization and shutdown of the singleton. Called exactly once.
  static void Init();
  static void Shutdown();

  // Gets an instance of the singleton. Infallible between the calls to Init
  // and Shutdown.
  static StaticPresData* Get();

  /**
   * Given a language, get the language group name, which can
   * be used as an argument to LangGroupFontPrefs::Initialize()
   *
   * aNeedsToCache is used for two things.  If null, it indicates that
   * the nsLanguageAtomService is safe to cache the result of the
   * language group lookup, either because we're on the main thread,
   * or because we're on a style worker thread but the font lock has
   * been acquired.  If non-null, it indicates that it's not safe to
   * cache the result of the language group lookup (because we're on
   * a style worker thread without the lock acquired).  In this case,
   * GetLanguageGroup will store true in *aNeedsToCache true if we
   * would have cached the result of a new lookup, and false if we
   * were able to use an existing cached result.  Thus, callers that
   * get a true *aNeedsToCache outparam value should make an effort
   * to re-call GetLanguageGroup when it is safe to cache, to avoid
   * recomputing the language group again later.
   */
  nsStaticAtom* GetLangGroup(nsAtom* aLanguage,
                             bool* aNeedsToCache = nullptr) const;

  /**
   * Same as GetLangGroup, but will not cache the result
   */
  nsStaticAtom* GetUncachedLangGroup(nsAtom* aLanguage) const;

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
   *
   * See comment on GetLangGroup for the usage of aNeedsToCache.
   */
  const LangGroupFontPrefs* GetFontPrefsForLang(nsAtom* aLanguage,
                                                bool* aNeedsToCache = nullptr);
  const nsFont* GetDefaultFont(uint8_t aFontID, nsAtom* aLanguage,
                               const LangGroupFontPrefs* aPrefs) const;

  void InvalidateFontPrefs() { mLangGroupFontPrefs.Reset(); }

 private:
  // Private constructor/destructor, to prevent other code from inadvertently
  // instantiating or deleting us. (Though we need to declare StaticAutoPtr as
  // a friend to give it permission.)
  StaticPresData();
  ~StaticPresData() = default;
  friend class StaticAutoPtr<StaticPresData>;

  nsLanguageAtomService* mLangService;
  LangGroupFontPrefs mLangGroupFontPrefs;
};

}  // namespace mozilla

#endif  // mozilla_StaticPresData_h
