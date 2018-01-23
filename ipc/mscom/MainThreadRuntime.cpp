/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadRuntime.h"

#if defined(ACCESSIBILITY)
#include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#if defined(ACCESSIBILITY)
#include "nsExceptionHandler.h"
#endif // defined(ACCESSIBILITY)
#include "nsWindowsHelpers.h"
#include "nsXULAppAPI.h"

#include <accctrl.h>
#include <aclapi.h>
#include <objbase.h>
#include <objidl.h>

namespace {

struct LocalFreeDeleter
{
  void operator()(void* aPtr)
  {
    ::LocalFree(aPtr);
  }
};

} // anonymous namespace

// This API from oleaut32.dll is not declared in Windows SDK headers
extern "C" void __cdecl SetOaNoCache(void);

namespace mozilla {
namespace mscom {

MainThreadRuntime* MainThreadRuntime::sInstance = nullptr;

MainThreadRuntime::MainThreadRuntime()
  : mInitResult(E_UNEXPECTED)
#if defined(ACCESSIBILITY)
  , mActCtxRgn(a11y::Compatibility::GetActCtxResourceId())
#endif // defined(ACCESSIBILITY)
{
  // We must be the outermost COM initialization on this thread. The COM runtime
  // cannot be configured once we start manipulating objects
  MOZ_ASSERT(mStaRegion.IsValidOutermost());
  if (NS_WARN_IF(!mStaRegion.IsValidOutermost())) {
    return;
  }

  // We are required to initialize security in order to configure global options.
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

  if (XRE_IsParentProcess()) {
    MainThreadClientInfo::Create(getter_AddRefs(mClientInfo));
  }

  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

MainThreadRuntime::~MainThreadRuntime()
{
  if (mClientInfo) {
    mClientInfo->Detach();
  }

  MOZ_ASSERT(sInstance == this);
  if (sInstance == this) {
    sInstance = nullptr;
  }
}

/* static */
DWORD
MainThreadRuntime::GetClientThreadId()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess(), "Unsupported outside of parent process");
  if (!XRE_IsParentProcess()) {
    return 0;
  }

  // Don't check for a calling executable if the caller is in-process.
  // We verify this by asking COM for a call context. If none exists, then
  // we must be a local call.
  RefPtr<IServerSecurity> serverSecurity;
  if (FAILED(::CoGetCallContext(IID_IServerSecurity,
                                getter_AddRefs(serverSecurity)))) {
    return 0;
  }

  MOZ_ASSERT(sInstance);
  if (!sInstance) {
    return 0;
  }

  MOZ_ASSERT(sInstance->mClientInfo);
  if (!sInstance->mClientInfo) {
    return 0;
  }

  return sInstance->mClientInfo->GetLastRemoteCallThreadId();
}

HRESULT
MainThreadRuntime::InitializeSecurity()
{
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
  ok = ::GetTokenInformation(token, TokenPrimaryGroup, tokenPrimaryGroupBuf.get(),
                             len, &len);
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

  // Grant access to SYSTEM, Administrators, and the user.
  EXPLICIT_ACCESS entries[] = {
    {COM_RIGHTS_EXECUTE, GRANT_ACCESS, NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_USER,
       reinterpret_cast<LPWSTR>(systemSid)}},
    {COM_RIGHTS_EXECUTE, GRANT_ACCESS, NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_WELL_KNOWN_GROUP,
       reinterpret_cast<LPWSTR>(adminSid)}},
    {COM_RIGHTS_EXECUTE, GRANT_ACCESS, NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_USER,
       reinterpret_cast<LPWSTR>(tokenUser.User.Sid)}}
  };

  PACL rawDacl = nullptr;
  win32Error = ::SetEntriesInAcl(ArrayLength(entries), entries, nullptr,
                                 &rawDacl);
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

  if (!::SetSecurityDescriptorGroup(&sd, tokenPrimaryGroup.PrimaryGroup, FALSE)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return ::CoInitializeSecurity(&sd, -1, nullptr, nullptr,
                                RPC_C_AUTHN_LEVEL_DEFAULT,
                                RPC_C_IMP_LEVEL_IDENTIFY, nullptr, EOAC_NONE,
                                nullptr);
}

} // namespace mscom
} // namespace mozilla
