/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Compatibility.h"

#include "mozilla/WindowsVersion.h"
#include "nsExceptionHandler.h"
#include "nsUnicharUtils.h"
#include "nsWindowsDllInterceptor.h"
#include "nsWinUtils.h"
#include "Statistics.h"

#include "mozilla/Preferences.h"

#include <shlobj.h>

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

#if !defined(HAVE_64BIT_BUILD)

static bool
ReadCOMRegDefaultString(const nsString& aRegPath, nsAString& aOutBuf)
{
  aOutBuf.Truncate();

  // Get the required size and type of the registry value.
  // We expect either REG_SZ or REG_EXPAND_SZ.
  DWORD type;
  DWORD bufLen = 0;
  LONG result = ::RegGetValue(HKEY_CLASSES_ROOT, aRegPath.get(),
                              nullptr, RRF_RT_ANY, &type, nullptr, &bufLen);
  if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return false;
  }

  // Now obtain the value
  DWORD flags = type == REG_SZ ? RRF_RT_REG_SZ : RRF_RT_REG_EXPAND_SZ;

  aOutBuf.SetLength((bufLen + 1) / sizeof(char16_t));

  result = ::RegGetValue(HKEY_CLASSES_ROOT, aRegPath.get(), nullptr,
                         flags, nullptr, aOutBuf.BeginWriting(), &bufLen);
  if (result != ERROR_SUCCESS) {
    aOutBuf.Truncate();
    return false;
  }

  // Truncate terminator
  aOutBuf.Truncate((bufLen + 1) / sizeof(char16_t) - 1);
  return true;
}

static bool
IsSystemOleAcc(nsCOMPtr<nsIFile>& aFile)
{
  // Use FOLDERID_SystemX86 so that Windows doesn't give us a redirected
  // system32 if we're a 32-bit process running on a 64-bit OS. This is
  // necessary because the values that we are reading from the registry
  // are not redirected; they reference SysWOW64 directly.
  PWSTR systemPath = nullptr;
  HRESULT hr = ::SHGetKnownFolderPath(FOLDERID_SystemX86, 0, nullptr,
                                      &systemPath);
  if (FAILED(hr)) {
    return false;
  }

  nsCOMPtr<nsIFile> oleAcc;
  nsresult rv = NS_NewLocalFile(nsDependentString(systemPath), false,
                                getter_AddRefs(oleAcc));

  ::CoTaskMemFree(systemPath);
  systemPath = nullptr;

  if (NS_FAILED(rv)) {
    return false;
  }

  rv = oleAcc->Append(NS_LITERAL_STRING("oleacc.dll"));
  if (NS_FAILED(rv)) {
    return false;
  }

  bool isEqual;
  rv = oleAcc->Equals(aFile, &isEqual);
  return NS_SUCCEEDED(rv) && isEqual;
}

static bool
IsTypelibPreferred()
{
  // If IAccessible's Proxy/Stub CLSID is kUniversalMarshalerClsid, then any
  // external a11y clients are expecting to use a typelib.
  NS_NAMED_LITERAL_STRING(kUniversalMarshalerClsid,
      "{00020424-0000-0000-C000-000000000046}");

  NS_NAMED_LITERAL_STRING(kIAccessiblePSClsidPath,
      "Interface\\{618736E0-3C3D-11CF-810C-00AA00389B71}\\ProxyStubClsid32");

  nsAutoString psClsid;
  if (!ReadCOMRegDefaultString(kIAccessiblePSClsidPath, psClsid)) {
    return false;
  }

  return psClsid.Equals(kUniversalMarshalerClsid,
                        nsCaseInsensitiveStringComparator());
}

static bool
IsIAccessibleTypelibRegistered()
{
  // The system default IAccessible typelib is always registered with version
  // 1.1, under the neutral locale (LCID 0).
  NS_NAMED_LITERAL_STRING(kIAccessibleTypelibRegPath,
    "TypeLib\\{1EA4DBF0-3C3B-11CF-810C-00AA00389B71}\\1.1\\0\\win32");

  nsAutoString typelibPath;
  if (!ReadCOMRegDefaultString(kIAccessibleTypelibRegPath, typelibPath)) {
    return false;
  }

  nsCOMPtr<nsIFile> libTestFile;
  nsresult rv = NS_NewLocalFile(typelibPath, false, getter_AddRefs(libTestFile));
  if (NS_FAILED(rv)) {
    return false;
  }

  return IsSystemOleAcc(libTestFile);
}

static bool
IsIAccessiblePSRegistered()
{
  NS_NAMED_LITERAL_STRING(kIAccessiblePSRegPath,
    "CLSID\\{03022430-ABC4-11D0-BDE2-00AA001A1953}\\InProcServer32");

  nsAutoString proxyStubPath;
  if (!ReadCOMRegDefaultString(kIAccessiblePSRegPath, proxyStubPath)) {
    return false;
  }

  nsCOMPtr<nsIFile> libTestFile;
  nsresult rv = NS_NewLocalFile(proxyStubPath, false, getter_AddRefs(libTestFile));
  if (NS_FAILED(rv)) {
    return false;
  }

  return IsSystemOleAcc(libTestFile);
}

static bool
UseIAccessibleProxyStub()
{
  // If a typelib is preferred then external clients are expecting to use
  // typelib marshaling, so we should use that whenever available.
  if (IsTypelibPreferred() && IsIAccessibleTypelibRegistered()) {
    return false;
  }

  // Otherwise we try the proxy/stub
  if (IsIAccessiblePSRegistered()) {
    return true;
  }

  // If we reach this point then something is seriously wrong with the
  // IAccessible configuration in the computer's registry. Let's annotate this
  // so that we can easily determine this condition during crash analysis.
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("IAccessibleConfig"),
                                     NS_LITERAL_CSTRING("NoSystemTypeLibOrPS"));
  return false;
}

#endif // !defined(HAVE_64BIT_BUILD)

uint16_t
Compatibility::GetActCtxResourceId()
{
#if defined(HAVE_64BIT_BUILD)
  // The manifest for 64-bit Windows is embedded with resource ID 64.
  return 64;
#else
  // The manifest for 32-bit Windows is embedded with resource ID 32.
  // Beginning with Windows 10 Creators Update, 32-bit builds always use the
  // 64-bit manifest. Older builds of Windows may or may not require the 64-bit
  // manifest: UseIAccessibleProxyStub() determines the course of action.
  if (mozilla::IsWin10CreatorsUpdateOrLater() ||
      UseIAccessibleProxyStub()) {
    return 64;
  }

  return 32;
#endif // defined(HAVE_64BIT_BUILD)
}

