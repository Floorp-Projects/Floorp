/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Some prefable colors for style system use */

#ifndef mozilla_ColorPreferences_h
#define mozilla_ColorPreferences_h

#include "nsColor.h"

namespace mozilla {

namespace dom {
class Document;
}

struct PreferenceSheet {
  struct Prefs {
    nscolor mLinkColor = NS_RGB(0x00, 0x00, 0xEE);
    nscolor mActiveLinkColor = NS_RGB(0xEE, 0x00, 0x00);
    nscolor mVisitedLinkColor = NS_RGB(0x55, 0x1A, 0x8B);

    nscolor mDefaultColor = NS_RGB(0, 0, 0);
    nscolor mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);

    nscolor mLinkBackgroundColor = mDefaultBackgroundColor;

    nscolor mFocusTextColor = mDefaultColor;
    nscolor mFocusBackgroundColor = mDefaultBackgroundColor;

    bool mIsChrome = false;
    bool mUseAccessibilityTheme = false;

    bool mUnderlineLinks = true;
    bool mUseFocusColors = false;
    bool mUseDocumentColors = true;
    uint8_t mFocusRingWidth = 1;
    uint8_t mFocusRingStyle = 1;
    bool mFocusRingOnAnything = false;

    // Whether the non-native theme should use system colors for widgets.
    // We only do that if we have a high-contrast theme _and_ we are overriding
    // the document colors. Otherwise it causes issues when pages only override
    // some of the system colors, specially in dark themes mode.
    bool NonNativeThemeShouldUseSystemColors() const {
      return mUseAccessibilityTheme && !mUseDocumentColors;
    }

    void Load(bool aIsChrome);
  };

  static void EnsureInitialized() {
    if (sInitialized) {
      return;
    }
    Initialize();
  }

  static void Refresh() {
    sInitialized = false;
    EnsureInitialized();
  }

  static Prefs& ContentPrefs() {
    MOZ_ASSERT(sInitialized);
    return sContentPrefs;
  }

  static Prefs& ChromePrefs() {
    MOZ_ASSERT(sInitialized);
    return sChromePrefs;
  }

  static bool ShouldUseChromePrefs(const dom::Document&);
  static const Prefs& PrefsFor(const dom::Document& aDocument) {
    return ShouldUseChromePrefs(aDocument) ? ChromePrefs() : ContentPrefs();
  }

 private:
  static bool sInitialized;
  static Prefs sContentPrefs;
  static Prefs sChromePrefs;

  static void Initialize();
};

}  // namespace mozilla

#endif
