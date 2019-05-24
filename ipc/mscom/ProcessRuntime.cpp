/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/ProcessRuntime.h"

#if defined(ACCESSIBILITY) && defined(MOZILLA_INTERNAL_API)
#  include "mozilla/a11y/Compatibility.h"
#endif  // defined(ACCESSIBILITY) && defined(MOZILLA_INTERNAL_API)
#include "mozilla/Assertions.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/mscom/ProcessRuntimeShared.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowsHelpers.h"

#if defined(MOZILLA_INTERNAL_API) && defined(MOZ_SANDBOX)
#  include "mozilla/mscom/EnsureMTA.h"
#  include "mozilla/sandboxTarget.h"
#  include "nsThreadManager.h"
#endif  // defined(MOZILLA_INTERNAL_API) && defined(MOZ_SANDBOX)

#include <accctrl.h>
#include <aclapi.h>
#include <objbase.h>
#include <objidl.h>

// This API from oleaut32.dll is not declared in Windows SDK headers
extern "C" void __cdecl SetOaNoCache(void);

namespace mozilla {
namespace mscom {

ProcessRuntime::ProcessRuntime(GeckoProcessType aProcessType)
    : mInitResult(CO_E_NOTINITIALIZED),
      mIsParentProcess(aProcessType == GeckoProcessType_Default)
#if defined(ACCESSIBILITY) && defined(MOZILLA_INTERNAL_API)
      ,
      mActCtxRgn(a11y::Compatibility::GetActCtxResourceId())
#endif  // defined(ACCESSIBILITY) && defined(MOZILLA_INTERNAL_API)
{
#if defined(MOZILLA_INTERNAL_API) && defined(MOZ_SANDBOX)
  // If our process is running under Win32k lockdown, we cannot initialize
  // COM with single-threaded apartments. This is because STAs create a hidden
  // window, which implicitly requires user32 and Win32k, which are blocked.
  // Instead we start a multi-threaded apartment and conduct our process-wide
  // COM initialization on that MTA background thread.
  if (!mIsParentProcess && IsWin32kLockedDown()) {
    // It is possible that we're running so early that we might need to start
    // the thread manager ourselves.
    nsresult rv = nsThreadManager::get().Init();
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      return;
    }

    // Use the current thread's impersonation token to initialize COM, as
    // it might fail otherwise (depending on sandbox policy).
    HANDLE rawCurThreadImpToken;
    if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_DUPLICATE | TOKEN_QUERY,
                           FALSE, &rawCurThreadImpToken)) {
      mInitResult = HRESULT_FROM_WIN32(::GetLastError());
      return;
    }
    nsAutoHandle curThreadImpToken(rawCurThreadImpToken);

#if defined(DEBUG)
    // Ensure that our current token is still an impersonation token (ie, we
    // have not yet called RevertToSelf() on this thread).
    DWORD len;
    TOKEN_TYPE tokenType;
    MOZ_ASSERT(::GetTokenInformation(rawCurThreadImpToken, TokenType,
                                     &tokenType, sizeof(tokenType), &len) &&
               len == sizeof(tokenType) && tokenType == TokenImpersonation);
#endif  // defined(DEBUG)

    // Create an impersonation token based on the current thread's token
    HANDLE rawMtaThreadImpToken = nullptr;
    if (!::DuplicateToken(rawCurThreadImpToken, SecurityImpersonation,
                          &rawMtaThreadImpToken)) {
      mInitResult = HRESULT_FROM_WIN32(::GetLastError());
      return;
    }
    nsAutoHandle mtaThreadImpToken(rawMtaThreadImpToken);

    SandboxTarget::Instance()->RegisterSandboxStartCallback([]() -> void {
      EnsureMTA([]() -> void {
        // This is a security risk if it fails, so we release assert
        MOZ_RELEASE_ASSERT(::RevertToSelf(),
                           "mscom::ProcessRuntime RevertToSelf failed");
      }, EnsureMTA::Option::ForceDispatch);
    });

    // Impersonate and initialize.
    EnsureMTA([this, rawMtaThreadImpToken]() -> void {
      if (!::SetThreadToken(nullptr, rawMtaThreadImpToken)) {
        mInitResult = HRESULT_FROM_WIN32(::GetLastError());
        return;
      }

      InitInsideApartment();
    }, EnsureMTA::Option::ForceDispatch);

    return;
  }

#endif  // defined(MOZILLA_INTERNAL_API)

  // Otherwise we initialize a single-threaded apartment on the current thread.
  mAptRegion.Init(COINIT_APARTMENTTHREADED);

  // We must be the outermost COM initialization on this thread. The COM runtime
  // cannot be configured once we start manipulating objects
  MOZ_ASSERT(mAptRegion.IsValidOutermost());
  if (!mAptRegion.IsValidOutermost()) {
    mInitResult = mAptRegion.GetHResult();
    return;
  }

  InitInsideApartment();
}

void ProcessRuntime::InitInsideApartment() {
  ProcessInitLock lock;
  if (lock.IsInitialized()) {
    // COM has already been initialized by a previous ProcessRuntime instance
    mInitResult = S_OK;
    return;
  }

  // We are required to initialize security prior to configuring global options.
  mInitResult = InitializeSecurity();
  MOZ_ASSERT(SUCCEEDED(mInitResult));
  if (FAILED(mInitResult)) {
    return;
  }

  RefPtr<IGlobalOptions> globalOpts;
  mInitResult = ::CoCreateInstance(CLSID_GlobalOptions, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_IGlobalOptions,
                                   (void**)getter_AddRefs(globalOpts));
  MOZ_ASSERT(SUCCEEDED(mInitResult));
  if (FAILED(mInitResult)) {
    return;
  }

  // Disable COM's catch-all exception handler
  mInitResult = globalOpts->Set(COMGLB_EXCEPTION_HANDLING,
                                COMGLB_EXCEPTION_DONOT_HANDLE_ANY);
  MOZ_ASSERT(SUCCEEDED(mInitResult));

  // Disable the BSTR cache (as it never invalidates, thus leaking memory)
  ::SetOaNoCache();

  if (FAILED(mInitResult)) {
    return;
  }

  lock.SetInitialized();
}

/* static */
DWORD
ProcessRuntime::GetClientThreadId() {
  DWORD callerTid;
  HRESULT hr = ::CoGetCallerTID(&callerTid);
  // Don't return callerTid unless the call succeeded and returned S_FALSE,
  // indicating that the caller originates from a different process.
  if (hr != S_FALSE) {
    return 0;
  }

  return callerTid;
}

HRESULT
ProcessRuntime::InitializeSecurity() {
  HANDLE rawToken = nullptr;
  BOOL ok = ::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &rawToken);
  if (!ok) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  nsAutoHandle token(rawToken);

  DWORD len = 0;
  ok = ::GetTokenInformation(token, TokenUser, nullptr, len, &len);
  DWORD win32Error = ::GetLastError();
  if (!ok && win32Error != ERROR_INSUFFICIENT_BUFFER) {
    return HRESULT_FROM_WIN32(win32Error);
  }

  auto tokenUserBuf = MakeUnique<BYTE[]>(len);
  TOKEN_USER& tokenUser = *reinterpret_cast<TOKEN_USER*>(tokenUserBuf.get());
  ok = ::GetTokenInformation(token, TokenUser, tokenUserBuf.get(), len, &len);
  if (!ok) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  len = 0;
  ok = ::GetTokenInformation(token, TokenPrimaryGroup, nullptr, len, &len);
  win32Error = ::GetLastError();
  if (!ok && win32Error != ERROR_INSUFFICIENT_BUFFER) {
    return HRESULT_FROM_WIN32(win32Error);
  }

  auto tokenPrimaryGroupBuf = MakeUnique<BYTE[]>(len);
  TOKEN_PRIMARY_GROUP& tokenPrimaryGroup =
      *reinterpret_cast<TOKEN_PRIMARY_GROUP*>(tokenPrimaryGroupBuf.get());
  ok = ::GetTokenInformation(token, TokenPrimaryGroup,
                             tokenPrimaryGroupBuf.get(), len, &len);
  if (!ok) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  SECURITY_DESCRIPTOR sd;
  if (!::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  BYTE systemSid[SECURITY_MAX_SID_SIZE];
  DWORD systemSidSize = sizeof(systemSid);
  if (!::CreateWellKnownSid(WinLocalSystemSid, nullptr, systemSid,
                            &systemSidSize)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  BYTE adminSid[SECURITY_MAX_SID_SIZE];
  DWORD adminSidSize = sizeof(adminSid);
  if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSid,
                            &adminSidSize)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  BYTE appContainersSid[SECURITY_MAX_SID_SIZE];
  DWORD appContainersSidSize = sizeof(appContainersSid);
  if (mIsParentProcess && IsWin8OrLater()) {
    if (!::CreateWellKnownSid(WinBuiltinAnyPackageSid, nullptr,
                              appContainersSid, &appContainersSidSize)) {
      return HRESULT_FROM_WIN32(::GetLastError());
    }
  }

  // Grant access to SYSTEM, Administrators, the user, and when running as the
  // browser process on Windows 8+, all app containers.
  const size_t kMaxInlineEntries = 4;
  mozilla::Vector<EXPLICIT_ACCESS_W, kMaxInlineEntries> entries;

  Unused << entries.append(EXPLICIT_ACCESS_W{
      COM_RIGHTS_EXECUTE,
      GRANT_ACCESS,
      NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_USER,
       reinterpret_cast<LPWSTR>(systemSid)}});

  Unused << entries.append(EXPLICIT_ACCESS_W{
      COM_RIGHTS_EXECUTE,
      GRANT_ACCESS,
      NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID,
       TRUSTEE_IS_WELL_KNOWN_GROUP, reinterpret_cast<LPWSTR>(adminSid)}});

  Unused << entries.append(EXPLICIT_ACCESS_W{
      COM_RIGHTS_EXECUTE,
      GRANT_ACCESS,
      NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_USER,
       reinterpret_cast<LPWSTR>(tokenUser.User.Sid)}});

  if (mIsParentProcess && IsWin8OrLater()) {
    Unused << entries.append(
        EXPLICIT_ACCESS_W{COM_RIGHTS_EXECUTE,
                          GRANT_ACCESS,
                          NO_INHERITANCE,
                          {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID,
                           TRUSTEE_IS_WELL_KNOWN_GROUP,
                           reinterpret_cast<LPWSTR>(appContainersSid)}});
  }

  PACL rawDacl = nullptr;
  win32Error =
      ::SetEntriesInAclW(entries.length(), entries.begin(), nullptr, &rawDacl);
  if (win32Error != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(win32Error);
  }

  UniquePtr<ACL, LocalFreeDeleter> dacl(rawDacl);

  if (!::SetSecurityDescriptorDacl(&sd, TRUE, dacl.get(), FALSE)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (!::SetSecurityDescriptorOwner(&sd, tokenUser.User.Sid, FALSE)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (!::SetSecurityDescriptorGroup(&sd, tokenPrimaryGroup.PrimaryGroup,
                                    FALSE)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return ::CoInitializeSecurity(
      &sd, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
      RPC_C_IMP_LEVEL_IDENTIFY, nullptr, EOAC_NONE, nullptr);
}

}  // namespace mscom
}  // namespace mozilla
