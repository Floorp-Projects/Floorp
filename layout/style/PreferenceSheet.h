/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Some prefable colors for style system use */

#ifndef mozilla_ColorPreferences_h
#define mozilla_ColorPreferences_h

#include "nsColor.h"
#include "mozilla/ColorScheme.h"

namespace mozilla {

namespace dom {
class Document;
}

struct PreferenceSheet {
  struct Prefs {
    struct Colors {
      nscolor mLink = NS_RGB(0x00, 0x00, 0xEE);
      nscolor mActiveLink = NS_RGB(0xEE, 0x00, 0x00);
      nscolor mVisitedLink = NS_RGB(0x55, 0x1A, 0x8B);

      nscolor mDefault = NS_RGB(0, 0, 0);
      nscolor mDefaultBackground = NS_RGB(0xFF, 0xFF, 0xFF);
    } mLightColors, mDarkColors;

    const Colors& ColorsFor(ColorScheme aScheme) const {
      return mMustUseLightColorSet || aScheme == ColorScheme::Light
                 ? mLightColors
                 : mDarkColors;
    }

    bool mIsChrome = false;
    bool mUseAccessibilityTheme = false;
    bool mUseDocumentColors = true;
    bool mUsePrefColors = false;
    bool mUseStandins = false;
    bool mMustUseLightColorSet = false;
    bool mMustUseLightSystemColors = false;

    ColorScheme mColorScheme = ColorScheme::Light;

    // Whether the non-native theme should use real system colors for widgets.
    bool NonNativeThemeShouldBeHighContrast() const;

    void Load(bool aIsChrome);
    void LoadColors(bool aIsLight);
  };

  static void EnsureInitialized() {
    if (sInitialized) {
      return;
    }
    Initialize();
  }

  static void Refresh() {
    sInitialized = false;
    Initialize();
  }

  static bool AffectedByPref(const nsACString&);

  enum class ChromeColorSchemeSetting { Light, Dark, System };
  static ChromeColorSchemeSetting ColorSchemeSettingForChrome();

  static ColorScheme ColorSchemeForChrome() {
    MOZ_ASSERT(sInitialized);
    return ChromePrefs().mColorScheme;
  }

  static ColorScheme PreferredColorSchemeForContent() {
    MOZ_ASSERT(sInitialized);
    return ContentPrefs().mColorScheme;
  }
  static ColorScheme ThemeDerivedColorSchemeForContent();

  static Prefs& ContentPrefs() {
    MOZ_ASSERT(sInitialized);
    return sContentPrefs;
  }

  static Prefs& ChromePrefs() {
    MOZ_ASSERT(sInitialized);
    return sChromePrefs;
  }

  static Prefs& PrintPrefs() {
    MOZ_ASSERT(sInitialized);
    return sPrintPrefs;
  }

  enum class PrefsKind {
    Chrome,
    Print,
    Content,
  };

  static PrefsKind PrefsKindFor(const dom::Document&);

  static bool ShouldUseChromePrefs(const dom::Document& aDocument) {
    return PrefsKindFor(aDocument) == PrefsKind::Chrome;
  }

  static bool MayForceColors() { return !ContentPrefs().mUseDocumentColors; }

  static const Prefs& PrefsFor(const dom::Document& aDocument) {
    switch (PrefsKindFor(aDocument)) {
      case PrefsKind::Chrome:
        return ChromePrefs();
      case PrefsKind::Print:
        return PrintPrefs();
      case PrefsKind::Content:
        break;
    }
    return ContentPrefs();
  }

 private:
  static bool sInitialized;
  static Prefs sChromePrefs;
  static Prefs sPrintPrefs;
  static Prefs sContentPrefs;

  static void Initialize();
};

}  // namespace mozilla

#endif
