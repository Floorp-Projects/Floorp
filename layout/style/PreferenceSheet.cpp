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
#include "mozilla/StaticPrefs_widget.h"
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

static void GetColor(const char* aPrefName, ColorScheme aColorScheme,
                     nscolor& aColor) {
  nsAutoCString darkPrefName;
  if (aColorScheme == ColorScheme::Dark) {
    darkPrefName.Append(aPrefName);
    darkPrefName.AppendLiteral(".dark");
    aPrefName = darkPrefName.get();
  }

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

void PreferenceSheet::Prefs::LoadColors(bool aIsLight) {
  auto& colors = aIsLight ? mLightColors : mDarkColors;

  if (!aIsLight) {
    // Initialize the dark-color-scheme foreground/background colors as being
    // the reverse of these members' default values, for ~reasonable fallback if
    // the user configures broken pref values.
    std::swap(colors.mDefault, colors.mDefaultBackground);
  }

  const bool useStandins = nsContentUtils::UseStandinsForNativeColors();
  // Users should be able to choose to use system colors or preferred colors
  // when HCM is disabled, and in both OS-level HCM and FF-level HCM.
  // To make this possible, we don't consider UseDocumentColors and
  // mUseAccessibilityTheme when computing the following bool.
  const bool usePrefColors = !useStandins && !mIsChrome &&
                             !StaticPrefs::browser_display_use_system_colors();
  const auto scheme = aIsLight ? ColorScheme::Light : ColorScheme::Dark;

  // Link colors might be provided by the OS, but they might not be. If they are
  // not, then fall back to the pref colors.
  //
  // In particular, we don't query active link color to the OS.
  GetColor("browser.anchor_color", scheme, colors.mLink);
  GetColor("browser.active_color", scheme, colors.mActiveLink);
  GetColor("browser.visited_color", scheme, colors.mVisitedLink);

  if (usePrefColors) {
    GetColor("browser.display.background_color", scheme,
             colors.mDefaultBackground);
    GetColor("browser.display.foreground_color", scheme, colors.mDefault);
  } else {
    using ColorID = LookAndFeel::ColorID;
    const auto standins = LookAndFeel::UseStandins(useStandins);
    colors.mDefault = LookAndFeel::Color(ColorID::Windowtext, scheme, standins,
                                         colors.mDefault);
    colors.mDefaultBackground = LookAndFeel::Color(
        ColorID::Window, scheme, standins, colors.mDefaultBackground);
    colors.mLink = LookAndFeel::Color(ColorID::MozNativehyperlinktext, scheme,
                                      standins, colors.mLink);

    if (auto color = LookAndFeel::GetColor(
            ColorID::MozNativevisitedhyperlinktext, scheme, standins)) {
      // If the system provides a visited link color, we should use it.
      colors.mVisitedLink = *color;
    } else if (mUseAccessibilityTheme) {
      // The fallback visited link color on HCM (if the system doesn't provide
      // one) is produced by preserving the foreground's green and averaging the
      // foreground and background for the red and blue.  This is how IE and
      // Edge do it too.
      colors.mVisitedLink = NS_RGB(
          AVG2(NS_GET_R(colors.mDefault), NS_GET_R(colors.mDefaultBackground)),
          NS_GET_G(colors.mDefault),
          AVG2(NS_GET_B(colors.mDefault), NS_GET_B(colors.mDefaultBackground)));
    } else {
      // Otherwise we keep the default visited link color
    }

    if (mUseAccessibilityTheme) {
      colors.mActiveLink = colors.mLink;
    }
  }

  {
    // These two are not color-scheme dependent, as we don't rebuild the
    // preference sheet based on effective color scheme.
    GetColor("browser.display.focus_text_color", ColorScheme::Light,
             colors.mFocusText);
    GetColor("browser.display.focus_background_color", ColorScheme::Light,
             colors.mFocusBackground);
  }

  // Wherever we got the default background color from, ensure it is opaque.
  colors.mDefaultBackground =
      NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF), colors.mDefaultBackground);
}

bool PreferenceSheet::Prefs::NonNativeThemeShouldBeHighContrast() const {
  // We only do that if we are overriding the document colors. Otherwise it
  // causes issues when pages only override some of the system colors,
  // specially in dark themes mode.
  return StaticPrefs::widget_non_native_theme_always_high_contrast() ||
         !mUseDocumentColors;
}

void PreferenceSheet::Prefs::Load(bool aIsChrome) {
  *this = {};

  mIsChrome = aIsChrome;
  mUseAccessibilityTheme = UseAccessibilityTheme(aIsChrome);

  LoadColors(true);
  LoadColors(false);
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
    // For printing, we always use a preferred-light color scheme.
    //
    // When overriding document colors, we ignore the `color-scheme` property,
    // but we still don't want to use the system colors (which might be dark,
    // despite having made it into mLightColors), because it both wastes ink and
    // it might interact poorly with the color adjustments we do while printing.
    //
    // So we override the light colors with our hardcoded default colors.
    sPrintPrefs.mLightColors = Prefs().mLightColors;
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
    // we should log the user's current foreground (text) color and background
    // color. Note, the document color use pref is the inverse of the HCM
    // dropdown option in preferences.
    //
    // Note that we only look at light colors because that's the color set we
    // use when forcing colors (since color-scheme is ignored when colors are
    // forced).
    //
    // The light color set is the one that potentially contains the Windows HCM
    // theme color/background (if we're using system colors and the user is
    // using a High Contrast theme), and also the colors that as of today we
    // allow setting in about:preferences.
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_HCM_FOREGROUND,
                         sContentPrefs.mLightColors.mDefault);
    Telemetry::ScalarSet(Telemetry::ScalarID::A11Y_HCM_BACKGROUND,
                         sContentPrefs.mLightColors.mDefaultBackground);
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
