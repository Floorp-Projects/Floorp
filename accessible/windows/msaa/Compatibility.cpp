/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "nsWindowsDllInterceptor.h"
#include "nsWinUtils.h"
#include "Statistics.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::a11y;

/**
 * Return true if module version is lesser than the given version.
 */
bool
IsModuleVersionLessThan(HMODULE aModuleHandle, DWORD aMajor, DWORD aMinor)
{
  wchar_t fileName[MAX_PATH];
  ::GetModuleFileNameW(aModuleHandle, fileName, MAX_PATH);

  DWORD dummy = 0;
  DWORD length = ::GetFileVersionInfoSizeW(fileName, &dummy);

  LPBYTE versionInfo = new BYTE[length];
  ::GetFileVersionInfoW(fileName, 0, length, versionInfo);

  UINT uLen;
  VS_FIXEDFILEINFO* fixedFileInfo = nullptr;
  ::VerQueryValueW(versionInfo, L"\\", (LPVOID*)&fixedFileInfo, &uLen);
  DWORD dwFileVersionMS = fixedFileInfo->dwFileVersionMS;
  DWORD dwFileVersionLS = fixedFileInfo->dwFileVersionLS;
  delete [] versionInfo;

  DWORD dwLeftMost = HIWORD(dwFileVersionMS);
  DWORD dwSecondRight = HIWORD(dwFileVersionLS);
  return (dwLeftMost < aMajor ||
    (dwLeftMost == aMajor && dwSecondRight < aMinor));
}


////////////////////////////////////////////////////////////////////////////////
// Compatibility
////////////////////////////////////////////////////////////////////////////////

static WindowsDllInterceptor sUser32Interceptor;
static decltype(&InSendMessageEx) sInSendMessageExStub = nullptr;
static bool sInSendMessageExHackEnabled = false;
static PVOID sVectoredExceptionHandler = nullptr;

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__) || defined(__clang__)
#define RETURN_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))
#endif

static inline bool
IsCurrentThreadInBlockingMessageSend(const DWORD aStateBits)
{
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
static DWORD WINAPI
InSendMessageExHook(LPVOID lpReserved)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  DWORD result = sInSendMessageExStub(lpReserved);
  if (NS_IsMainThread() && sInSendMessageExHackEnabled &&
      IsCurrentThreadInBlockingMessageSend(result)) {
    // We want to take a strong reference to the dll so that it is never
    // unloaded/reloaded from this point forward, hence we use LoadLibrary
    // and not GetModuleHandle.
    static HMODULE comModule = LoadLibrary(L"combase.dll");
    MOZ_ASSERT(comModule);
    if (!comModule) {
      return result;
    }
    // Check if InSendMessageEx is being called from code within combase.dll
    HMODULE callingModule;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(RETURN_ADDRESS()),
                          &callingModule) && callingModule == comModule) {
      result = ISMEX_NOTIFY;
    }
  }
  return result;
}

static LONG CALLBACK
DetectInSendMessageExCompat(PEXCEPTION_POINTERS aExceptionInfo)
{
  DWORD exceptionCode = aExceptionInfo->ExceptionRecord->ExceptionCode;
  if (exceptionCode == RPC_E_CANTCALLOUT_ININPUTSYNCCALL &&
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

void
Compatibility::Init()
{
  // Note we collect some AT statistics/telemetry here for convenience.

  HMODULE jawsHandle = ::GetModuleHandleW(L"jhook");
  if (jawsHandle)
    sConsumers |= (IsModuleVersionLessThan(jawsHandle, 8, 2173)) ?
                   OLDJAWS : JAWS;

  if (::GetModuleHandleW(L"gwm32inc"))
    sConsumers |= WE;

  if (::GetModuleHandleW(L"dolwinhk"))
    sConsumers |= DOLPHIN;

  if (::GetModuleHandleW(L"STSA32"))
    sConsumers |= SEROTEK;

  if (::GetModuleHandleW(L"nvdaHelperRemote"))
    sConsumers |= NVDA;

  if (::GetModuleHandleW(L"OsmHooks"))
    sConsumers |= COBRA;

  if (::GetModuleHandleW(L"WebFinderRemote"))
    sConsumers |= ZOOMTEXT;

  if (::GetModuleHandleW(L"Kazahook"))
    sConsumers |= KAZAGURU;

  if (::GetModuleHandleW(L"TextExtractorImpl32") ||
      ::GetModuleHandleW(L"TextExtractorImpl64"))
    sConsumers |= YOUDAO;

  if (::GetModuleHandleW(L"uiautomation") ||
      ::GetModuleHandleW(L"uiautomationcore"))
    sConsumers |= UIAUTOMATION;

  // If we have a known consumer remove the unknown bit.
  if (sConsumers != Compatibility::UNKNOWN)
    sConsumers ^= Compatibility::UNKNOWN;

  // Gather telemetry
  uint32_t temp = sConsumers;
  for (int i = 0; temp; i++) {
    if (temp & 0x1)
      statistics::A11yConsumers(i);

    temp >>= 1;
  }

  // Turn off new tab switching for Jaws and WE.
  if (sConsumers & (JAWS | OLDJAWS | WE)) {
    // Check to see if the pref for disallowing CtrlTab is already set. If so,
    // bail out (respect the user settings). If not, set it.
    if (!Preferences::HasUserValue("browser.ctrlTab.disallowForScreenReaders"))
      Preferences::SetBool("browser.ctrlTab.disallowForScreenReaders", true);
  }

  // If we have a known consumer who is not NVDA, we enable detection for the
  // InSendMessageEx compatibility hack. NVDA does not require this.
  if ((sConsumers & ~(Compatibility::UNKNOWN | NVDA)) &&
      BrowserTabsRemoteAutostart()) {
    sUser32Interceptor.Init("user32.dll");
    if (!sInSendMessageExStub) {
      sUser32Interceptor.AddHook("InSendMessageEx",
                                 reinterpret_cast<intptr_t>(&InSendMessageExHook),
                                 (void**)&sInSendMessageExStub);
    }
    // The vectored exception handler allows us to catch exceptions ahead of any
    // SEH handlers.
    if (!sVectoredExceptionHandler) {
      sVectoredExceptionHandler =
        AddVectoredExceptionHandler(TRUE, &DetectInSendMessageExCompat);
    }
  }
}

