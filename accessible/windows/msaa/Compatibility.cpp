/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsExceptionHandler.h"
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsWinUtils.h"
#include "Statistics.h"
#include "AccessibleWrap.h"

#include "mozilla/Preferences.h"

#include <shlobj.h>

using namespace mozilla;
using namespace mozilla::a11y;

/**
 * String versions of consumer flags. See GetHumanReadableConsumersStr.
 */
static const wchar_t* ConsumerStringMap[CONSUMERS_ENUM_LEN + 1] = {
    L"NVDA",    L"JAWS",         L"OLDJAWS",       L"WE",       L"DOLPHIN",
    L"SEROTEK", L"COBRA",        L"ZOOMTEXT",      L"KAZAGURU", L"YOUDAO",
    L"UNKNOWN", L"UIAUTOMATION", L"VISPEROSHARED", L"\0"};

bool Compatibility::IsModuleVersionLessThan(HMODULE aModuleHandle,
                                            unsigned long long aVersion) {
  LauncherResult<ModuleVersion> version = GetModuleVersion(aModuleHandle);
  if (version.isErr()) {
    return true;
  }

  return version.unwrap() < aVersion;
}

////////////////////////////////////////////////////////////////////////////////
// Compatibility
////////////////////////////////////////////////////////////////////////////////

uint32_t Compatibility::sConsumers = Compatibility::UNKNOWN;

/**
 * This function is safe to call multiple times.
 */
/* static */
void Compatibility::InitConsumers() {
  HMODULE jawsHandle = ::GetModuleHandleW(L"jhook");
  if (jawsHandle) {
    sConsumers |=
        IsModuleVersionLessThan(jawsHandle, MAKE_FILE_VERSION(19, 0, 0, 0))
            ? OLDJAWS
            : JAWS;
  }

  if (::GetModuleHandleW(L"gwm32inc")) sConsumers |= WE;

  if (::GetModuleHandleW(L"dolwinhk")) sConsumers |= DOLPHIN;

  if (::GetModuleHandleW(L"STSA32")) sConsumers |= SEROTEK;

  if (::GetModuleHandleW(L"nvdaHelperRemote")) sConsumers |= NVDA;

  if (::GetModuleHandleW(L"OsmHooks") || ::GetModuleHandleW(L"OsmHks64"))
    sConsumers |= COBRA;

  if (::GetModuleHandleW(L"WebFinderRemote")) sConsumers |= ZOOMTEXT;

  if (::GetModuleHandleW(L"Kazahook")) sConsumers |= KAZAGURU;

  if (::GetModuleHandleW(L"TextExtractorImpl32") ||
      ::GetModuleHandleW(L"TextExtractorImpl64"))
    sConsumers |= YOUDAO;

  if (::GetModuleHandleW(L"uiautomation") ||
      ::GetModuleHandleW(L"uiautomationcore"))
    sConsumers |= UIAUTOMATION;

  if (::GetModuleHandleW(L"AccEventCache")) {
    sConsumers |= VISPEROSHARED;
  }

  // If we have a known consumer remove the unknown bit.
  if (sConsumers != Compatibility::UNKNOWN)
    sConsumers &= ~Compatibility::UNKNOWN;
}

/* static */
bool Compatibility::HasKnownNonUiaConsumer() {
  InitConsumers();
  return sConsumers & ~(Compatibility::UNKNOWN | UIAUTOMATION);
}

void Compatibility::Init() {
  // Note we collect some AT statistics/telemetry here for convenience.
  InitConsumers();

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::AccessibilityInProcClient,
      nsPrintfCString("0x%X", sConsumers));

  // Gather telemetry
  uint32_t temp = sConsumers;
  for (int i = 0; temp; i++) {
    if (temp & 0x1) statistics::A11yConsumers(i);

    temp >>= 1;
  }

  // Turn off new tab switching for Jaws and WE.
  if (sConsumers & (JAWS | OLDJAWS | WE)) {
    // Check to see if the pref for disallowing CtrlTab is already set. If so,
    // bail out (respect the user settings). If not, set it.
    if (!Preferences::HasUserValue("browser.ctrlTab.disallowForScreenReaders"))
      Preferences::SetBool("browser.ctrlTab.disallowForScreenReaders", true);
  }
}

// static
void Compatibility::GetHumanReadableConsumersStr(nsAString& aResult) {
  bool appened = false;
  uint32_t index = 0;
  for (uint32_t consumers = sConsumers; consumers; consumers = consumers >> 1) {
    if (consumers & 0x1) {
      if (appened) {
        aResult.AppendLiteral(",");
      }
      aResult.Append(ConsumerStringMap[index]);
      appened = true;
    }
    if (++index > CONSUMERS_ENUM_LEN) {
      break;
    }
  }
}

// Time when SuppressA11yForClipboardCopy() was called, as returned by
// ::GetTickCount().
static DWORD sA11yClipboardCopySuppressionStartTime = 0;

/* static */
void Compatibility::SuppressA11yForClipboardCopy() {
  // Bug 1774285: Windows Suggested Actions (introduced in Windows 11 22H2)
  // might walk the a11y tree using UIA whenever anything is copied to the
  // clipboard. This causes an unacceptable hang, particularly when the cache
  // is disabled.
  bool doSuppress = [&] {
    switch (
        StaticPrefs::accessibility_windows_suppress_after_clipboard_copy()) {
      case 0:
        return false;
      case 1:
        return true;
      default:
        // Our workaround for Suggested Actions is needed from Windows 11 22H2
        return IsWin1122H2OrLater();
    }
  }();

  if (doSuppress) {
    sA11yClipboardCopySuppressionStartTime = ::GetTickCount();
  }
}

/* static */
bool Compatibility::IsA11ySuppressedForClipboardCopy() {
  constexpr DWORD kSuppressTimeout = 1500;  // ms
  if (!sA11yClipboardCopySuppressionStartTime) {
    return false;
  }
  return ::GetTickCount() - sA11yClipboardCopySuppressionStartTime <
         kSuppressTimeout;
}
