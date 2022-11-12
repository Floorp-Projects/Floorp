/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/WindowsVersion.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsExceptionHandler.h"
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWinUtils.h"
#include "Statistics.h"

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

static WindowsDllInterceptor sUser32Interceptor;
static WindowsDllInterceptor::FuncHookType<decltype(&InSendMessageEx)>
    sInSendMessageExStub;
static bool sInSendMessageExHackEnabled = false;
static PVOID sVectoredExceptionHandler = nullptr;

#if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(_ReturnAddress)
#  define RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__) || defined(__clang__)
#  define RETURN_ADDRESS() \
    __builtin_extract_return_addr(__builtin_return_address(0))
#endif

static inline bool IsCurrentThreadInBlockingMessageSend(
    const DWORD aStateBits) {
  // From the MSDN docs for InSendMessageEx
  return (aStateBits & (ISMEX_REPLIED | ISMEX_SEND)) == ISMEX_SEND;
}

/**
 * COM assumes that if you're invoking a proxy from an STA thread while
 * InSendMessageEx reports that the calling thread is blocked, that you'll
 * deadlock your own process. It returns the RPC_E_CANTCALLOUT_ININPUTSYNCCALL
 * error code. This is not actually true in our case: we are calling into
 * the multithreaded apartment via ALPC. In this hook, we check to see if the
 * caller is COM, and if so, we lie to it.
 *
 * This hack is necessary for ATs who invoke COM proxies from within
 * WH_CALLWNDPROC hooks, WinEvent hooks, or a WndProc handling a sent
 * (as opposed to posted) message.
 */
static DWORD WINAPI InSendMessageExHook(LPVOID lpReserved) {
  MOZ_ASSERT(XRE_IsParentProcess());
  DWORD result = sInSendMessageExStub(lpReserved);
  if (NS_IsMainThread() && sInSendMessageExHackEnabled &&
      IsCurrentThreadInBlockingMessageSend(result)) {
    // We want to take a strong reference to the dll so that it is never
    // unloaded/reloaded from this point forward, hence we use LoadLibrary
    // and not GetModuleHandle.
    static const HMODULE comModule = []() -> HMODULE {
      HMODULE module = LoadLibraryW(L"combase.dll");
      if (!module) {
        // combase is not present on Windows 7, so we fall back to ole32 there
        module = LoadLibraryW(L"ole32.dll");
      }

      return module;
    }();

    MOZ_ASSERT(comModule);
    if (!comModule) {
      return result;
    }

    // Check if InSendMessageEx is being called from code within comModule
    HMODULE callingModule;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(RETURN_ADDRESS()),
                          &callingModule) &&
        callingModule == comModule) {
      result = ISMEX_NOTIFY;
    }
  }
  return result;
}

static LONG CALLBACK
DetectInSendMessageExCompat(PEXCEPTION_POINTERS aExceptionInfo) {
  DWORD exceptionCode = aExceptionInfo->ExceptionRecord->ExceptionCode;
  if (exceptionCode == static_cast<DWORD>(RPC_E_CANTCALLOUT_ININPUTSYNCCALL) &&
      NS_IsMainThread()) {
    sInSendMessageExHackEnabled = true;
    // We don't need this exception handler anymore, so remove it
    if (RemoveVectoredExceptionHandler(sVectoredExceptionHandler)) {
      sVectoredExceptionHandler = nullptr;
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

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

  // If we have a consumer who is not NVDA, we enable detection for the
  // InSendMessageEx compatibility hack. NVDA does not require this.
  // We also skip UIA, as we see crashes there.
  if ((sConsumers & (~(UIAUTOMATION | NVDA))) && BrowserTabsRemoteAutostart()) {
    sUser32Interceptor.Init("user32.dll");
    sInSendMessageExStub.Set(sUser32Interceptor, "InSendMessageEx",
                             &InSendMessageExHook);

    // The vectored exception handler allows us to catch exceptions ahead of any
    // SEH handlers.
    if (!sVectoredExceptionHandler) {
      // We need to let ASan's ShadowExceptionHandler remain in the firstHandler
      // position, otherwise we'll get infinite recursion when our handler
      // faults on shadow memory.
      const ULONG firstHandler = FALSE;
      sVectoredExceptionHandler = AddVectoredExceptionHandler(
          firstHandler, &DetectInSendMessageExCompat);
    }
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
        return NeedsWindows11SuggestedActionsWorkaround();
    }
  }();

  if (doSuppress) {
    sA11yClipboardCopySuppressionStartTime = ::GetTickCount();
  }
}

/* static */
bool Compatibility::IsA11ySuppressedForClipboardCopy() {
  constexpr DWORD kSuppressTimeout = 1000;  // ms
  if (!sA11yClipboardCopySuppressionStartTime) {
    return false;
  }
  return ::GetTickCount() - sA11yClipboardCopySuppressionStartTime <
         kSuppressTimeout;
}
