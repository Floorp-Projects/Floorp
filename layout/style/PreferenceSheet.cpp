/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreferenceSheet.h"

#include "ServoCSSParser.h"
#include "MainThreadUtils.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_devtools.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

#define AVG2(a, b) (((a) + (b) + 1) >> 1)

using mozilla::dom::Document;

namespace mozilla {

bool PreferenceSheet::sInitialized;
PreferenceSheet::Prefs PreferenceSheet::sContentPrefs;
PreferenceSheet::Prefs PreferenceSheet::sChromePrefs;

static void GetColor(const char* aPrefName, nscolor& aColor) {
  nsAutoString value;
  Preferences::GetString(aPrefName, value);
  if (value.IsEmpty()) {
    return;
  }
  nscolor result;
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), value, &result)) {
    return;
  }
  aColor = result;
}

bool PreferenceSheet::ShouldUseChromePrefs(const Document& aDoc) {
  // DevTools documents run in a content frame but should temporarily use
  // chrome preferences, in particular to avoid applying High Contrast mode
  // colors. See Bug 1575766.
  if (aDoc.IsDevToolsDocument() &&
      StaticPrefs::devtools_toolbox_force_chrome_prefs()) {
    return true;
  }

  if (aDoc.IsInChromeDocShell()) {
    return true;
  }

  if (aDoc.IsBeingUsedAsImage() && aDoc.IsDocumentURISchemeChrome()) {
    return true;
  }

  return false;
}

static bool UseAccessibilityTheme(bool aIsChrome) {
  return !aIsChrome &&
         !!LookAndFeel::GetInt(LookAndFeel::eIntID_UseAccessibilityTheme, 0);
}

void PreferenceSheet::Prefs::Load(bool aIsChrome) {
  *this = {};

  mIsChrome = aIsChrome;
  mUseAccessibilityTheme = UseAccessibilityTheme(aIsChrome);
  mUnderlineLinks = StaticPrefs::browser_underline_anchors();

  mUseFocusColors = StaticPrefs::browser_display_use_focus_colors();
  mFocusRingWidth = StaticPrefs::browser_display_focus_ring_width();
  mFocusRingStyle = StaticPrefs::browser_display_focus_ring_style();
  mFocusRingOnAnything = StaticPrefs::browser_display_focus_ring_on_anything();

  const bool usePrefColors = !aIsChrome && !mUseAccessibilityTheme &&
                             !StaticPrefs::browser_display_use_system_colors();

  if (nsContentUtils::UseStandinsForNativeColors()) {
    mDefaultColor = LookAndFeel::GetColorUsingStandins(
        LookAndFeel::ColorID::Windowtext, mDefaultColor);
    mDefaultBackgroundColor = LookAndFeel::GetColorUsingStandins(
        LookAndFeel::ColorID::Window, mDefaultBackgroundColor);
    mLinkColor = LookAndFeel::GetColorUsingStandins(
        LookAndFeel::ColorID::MozNativehyperlinktext, mLinkColor);
  } else if (usePrefColors) {
    GetColor("browser.display.background_color", mDefaultBackgroundColor);
    GetColor("browser.display.foreground_color", mDefaultColor);
    GetColor("browser.anchor_color", mLinkColor);
  } else {
    mDefaultColor = LookAndFeel::GetColor(
        LookAndFeel::ColorID::WindowForeground, mDefaultColor);
    mDefaultBackgroundColor = LookAndFeel::GetColor(
        LookAndFeel::ColorID::WindowBackground, mDefaultBackgroundColor);
    mLinkColor = LookAndFeel::GetColor(
        LookAndFeel::ColorID::MozNativehyperlinktext, mLinkColor);
  }

  if (mUseAccessibilityTheme && !nsContentUtils::UseStandinsForNativeColors()) {
    mActiveLinkColor = mLinkColor;
    // Visited link color is produced by preserving the foreground's green
    // and averaging the foreground and background for the red and blue.
    // This is how IE and Edge do it too.
    mVisitedLinkColor = NS_RGB(
        AVG2(NS_GET_R(mDefaultColor), NS_GET_R(mDefaultBackgroundColor)),
        NS_GET_G(mDefaultColor),
        AVG2(NS_GET_B(mDefaultColor), NS_GET_B(mDefaultBackgroundColor)));
  } else {
    GetColor("browser.active_color", mActiveLinkColor);
    GetColor("browser.visited_color", mVisitedLinkColor);
  }

  GetColor("browser.display.focus_text_color", mFocusTextColor);
  GetColor("browser.display.focus_background_color", mFocusBackgroundColor);

  // Wherever we got the default background color from, ensure it is
  // opaque.
  mDefaultBackgroundColor =
      NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF), mDefaultBackgroundColor);
}

void PreferenceSheet::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sInitialized);

  sInitialized = true;

  sContentPrefs.Load(false);
  sChromePrefs.Load(true);

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
                       UseAccessibilityTheme(false));
}

}  // namespace mozilla

#undef AVG2
