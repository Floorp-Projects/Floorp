/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadRuntime.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsDebug.h"
#include "nsWindowsHelpers.h"

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

namespace mozilla {
namespace mscom {

MainThreadRuntime::MainThreadRuntime()
  : mInitResult(E_UNEXPECTED)
{
  // We must be the outermost COM initialization on this thread. The COM runtime
  // cannot be configured once we start manipulating objects
  MOZ_ASSERT(mStaRegion.IsValidOutermost());
  if (NS_WARN_IF(!mStaRegion.IsValidOutermost())) {
    return;
  }

  // Windows XP doesn't support setting of the COM exception policy, so we'll
  // just stop here in that case.
  if (!IsVistaOrLater()) {
    mInitResult = S_OK;
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

  // Windows 7 has a policy that is even more strict. We should use that one
  // whenever possible.
  ULONG_PTR exceptionSetting = IsWin7OrLater() ?
                               COMGLB_EXCEPTION_DONOT_HANDLE_ANY :
                               COMGLB_EXCEPTION_DONOT_HANDLE;
  mInitResult = globalOpts->Set(COMGLB_EXCEPTION_HANDLING, exceptionSetting);
  MOZ_ASSERT(SUCCEEDED(mInitResult));
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

  // Grant access to SYSTEM, Administrators, and the user.
  EXPLICIT_ACCESS entries[] = {
    {COM_RIGHTS_EXECUTE, GRANT_ACCESS, NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_NAME, TRUSTEE_IS_USER,
       L"SYSTEM"}},
    {COM_RIGHTS_EXECUTE, GRANT_ACCESS, NO_INHERITANCE,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_NAME, TRUSTEE_IS_WELL_KNOWN_GROUP,
       L"ADMINISTRATORS"}},
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

