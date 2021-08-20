/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreferenceSheet.h"

#include "ServoCSSParser.h"
#include "MainThreadUtils.h"
#include "mozilla/Encoding.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_devtools.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

#define AVG2(a, b) (((a) + (b) + 1) >> 1)

namespace mozilla {

using dom::Document;

bool PreferenceSheet::sInitialized;
PreferenceSheet::Prefs PreferenceSheet::sContentPrefs;
PreferenceSheet::Prefs PreferenceSheet::sChromePrefs;
PreferenceSheet::Prefs PreferenceSheet::sPrintPrefs;

static void GetColor(const char* aPrefName, nscolor& aColor) {
  nsAutoCString value;
  Preferences::GetCString(aPrefName, value);
  if (value.IsEmpty() || Encoding::UTF8ValidUpTo(value) != value.Length()) {
    return;
  }
  nscolor result;
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), value, &result)) {
    return;
  }
  aColor = result;
}

auto PreferenceSheet::PrefsKindFor(const Document& aDoc) -> PrefsKind {
  // DevTools documents run in a content frame but should temporarily use
  // chrome preferences, in particular to avoid applying High Contrast mode
  // colors. See Bug 1575766.
  if (aDoc.IsDevToolsDocument() &&
      StaticPrefs::devtools_toolbox_force_chrome_prefs()) {
    return PrefsKind::Chrome;
  }

  if (aDoc.IsInChromeDocShell()) {
    return PrefsKind::Chrome;
  }

  if (aDoc.IsBeingUsedAsImage() && aDoc.IsDocumentURISchemeChrome()) {
    return PrefsKind::Chrome;
  }

  if (aDoc.IsStaticDocument()) {
    return PrefsKind::Print;
  }

  return PrefsKind::Content;
}

static bool UseAccessibilityTheme(bool aIsChrome) {
  return !aIsChrome &&
         !!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0);
}

static bool UseDocumentColors(bool aIsChrome, bool aUseAcccessibilityTheme) {
  switch (StaticPrefs::browser_display_document_color_use()) {
    case 1:
      return true;
    case 2:
      return aIsChrome;
    default:
      return !aUseAcccessibilityTheme;
  }
}

void PreferenceSheet::Prefs::Load(bool aIsChrome) {
  *this = {};

  mIsChrome = aIsChrome;
  mUseAccessibilityTheme = UseAccessibilityTheme(aIsChrome);

  const bool useStandins = nsContentUtils::UseStandinsForNativeColors();
  // Users should be able to choose to use system colors or preferred colors
  // when HCM is disabled, and in both OS-level HCM and FF-level HCM.
  // To make this possible, we don't consider UseDocumentColors and
  // mUseAccessibilityTheme when computing the following bool.
  const bool usePrefColors = !useStandins && !aIsChrome &&
                             !StaticPrefs::browser_display_use_system_colors();
  if (usePrefColors) {
    GetColor("browser.display.background_color", mColors.mDefaultBackground);
    GetColor("browser.display.foreground_color", mColors.mDefault);
    GetColor("browser.anchor_color", mColors.mLink);
    GetColor("browser.active_color", mColors.mActiveLink);
    GetColor("browser.visited_color", mColors.mVisitedLink);
  } else {
    using ColorID = LookAndFeel::ColorID;
    const auto standins = LookAndFeel::UseStandins(useStandins);
    // TODO(emilio): In the future we probably want to keep both sets of colors
    // around or something.
    //
    // FIXME(emilio): Why do we look at a different set of colors when using
    // standins vs. not?
    const auto scheme = LookAndFeel::ColorScheme::Light;
    mColors.mDefault = LookAndFeel::Color(
        useStandins ? ColorID::Windowtext : ColorID::WindowForeground, scheme,
        standins, mColors.mDefault);
    mColors.mDefaultBackground = LookAndFeel::Color(
        useStandins ? ColorID::Window : ColorID::WindowBackground, scheme,
        standins, mColors.mDefaultBackground);
    mColors.mLink = LookAndFeel::Color(ColorID::MozNativehyperlinktext, scheme,
                                       standins, mColors.mLink);

    if (auto color = LookAndFeel::GetColor(
            ColorID::MozNativevisitedhyperlinktext, scheme, standins)) {
      // If the system provides a visited link color, we should use it.
      mColors.mVisitedLink = *color;
    } else if (mUseAccessibilityTheme) {
      // The fallback visited link color on HCM (if the system doesn't provide
      // one) is produced by preserving the foreground's green and averaging the
      // foreground and background for the red and blue.  This is how IE and
      // Edge do it too.
      mColors.mVisitedLink = NS_RGB(AVG2(NS_GET_R(mColors.mDefault),
                                         NS_GET_R(mColors.mDefaultBackground)),
                                    NS_GET_G(mColors.mDefault),
                                    AVG2(NS_GET_B(mColors.mDefault),
                                         NS_GET_B(mColors.mDefaultBackground)));
    } else {
      // Otherwise we keep the default visited link color
    }

    if (mUseAccessibilityTheme) {
      mColors.mActiveLink = mColors.mLink;
    }
  }

  GetColor("browser.display.focus_text_color", mColors.mFocusText);
  GetColor("browser.display.focus_background_color", mColors.mFocusBackground);

  // Wherever we got the default background color from, ensure it is
  // opaque.
  mColors.mDefaultBackground =
      NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF), mColors.mDefaultBackground);
  mUseDocumentColors = UseDocumentColors(aIsChrome, mUseAccessibilityTheme);
}

void PreferenceSheet::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInitialized);

  sInitialized = true;

  sContentPrefs.Load(false);
  sChromePrefs.Load(true);
  sPrintPrefs = sContentPrefs;
  if (!sPrintPrefs.mUseDocumentColors) {
    sPrintPrefs.mColors = Prefs().mColors;
  }

  nsAutoString useDocumentColorPref;
  switch (StaticPrefs::browser_display_document_color_use()) {
    case 1:
      useDocumentColorPref.AssignLiteral("always");
      break;
    case 2:
      useDocumentColorPref.AssignLiteral("never");
      break;
    default:
      useDocumentColorPref.AssignLiteral("default");
      break;
  }

  Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_THEME, useDocumentColorPref,
                       sContentPrefs.mUseAccessibilityTheme);
  if (!sContentPrefs.mUseDocumentColors) {
    // If a user has chosen to override doc colors through OS HCM or our HCM,
    // we should log the user's current forground (text) color and background
    // color. Note, the document color use pref is the inverse of the HCM
    // dropdown option in preferences.
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_HCM_FOREGROUND,
                         sContentPrefs.mColors.mDefault);
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_HCM_BACKGROUND,
                         sContentPrefs.mColors.mDefaultBackground);
  }

  Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_BACKPLATE,
                       StaticPrefs::browser_display_permit_backplate());
}

bool PreferenceSheet::AffectedByPref(const nsACString& aPref) {
  const char* prefNames[] = {
      StaticPrefs::GetPrefName_devtools_toolbox_force_chrome_prefs(),
      StaticPrefs::GetPrefName_privacy_resistFingerprinting(),
      StaticPrefs::GetPrefName_ui_use_standins_for_native_colors(),
      "browser.anchor_color",
      "browser.active_color",
      "browser.visited_color",
  };

  if (StringBeginsWith(aPref, "browser.display."_ns)) {
    return true;
  }

  for (const char* pref : prefNames) {
    if (aPref.Equals(pref)) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla

#undef AVG2
