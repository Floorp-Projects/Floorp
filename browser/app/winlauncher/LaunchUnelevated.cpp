/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#define MOZ_USE_LAUNCHER_ERROR

#include "LaunchUnelevated.h"

#include "mozilla/Assertions.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/mscom/ProcessRuntime.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ShellHeaderOnlyUtils.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "nsWindowsHelpers.h"

#include <windows.h>

static mozilla::LauncherResult<bool> IsHighIntegrity(
    const nsAutoHandle& aToken) {
  DWORD reqdLen;
  if (!::GetTokenInformation(aToken.get(), TokenIntegrityLevel, nullptr, 0,
                             &reqdLen)) {
    DWORD err = ::GetLastError();
    if (err != ERROR_INSUFFICIENT_BUFFER) {
      return LAUNCHER_ERROR_FROM_WIN32(err);
    }
  }

  auto buf = mozilla::MakeUnique<char[]>(reqdLen);

  if (!::GetTokenInformation(aToken.get(), TokenIntegrityLevel, buf.get(),
                             reqdLen, &reqdLen)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  auto tokenLabel = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buf.get());

  DWORD subAuthCount = *::GetSidSubAuthorityCount(tokenLabel->Label.Sid);
  DWORD integrityLevel =
      *::GetSidSubAuthority(tokenLabel->Label.Sid, subAuthCount - 1);
  return integrityLevel > SECURITY_MANDATORY_MEDIUM_RID;
}

static mozilla::LauncherResult<HANDLE> GetMediumIntegrityToken(
    const nsAutoHandle& aProcessToken) {
  HANDLE rawResult;
  if (!::DuplicateTokenEx(aProcessToken.get(), 0, nullptr,
                          SecurityImpersonation, TokenPrimary, &rawResult)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  nsAutoHandle result(rawResult);

  BYTE mediumIlSid[SECURITY_MAX_SID_SIZE];
  DWORD mediumIlSidSize = sizeof(mediumIlSid);
  if (!::CreateWellKnownSid(WinMediumLabelSid, nullptr, mediumIlSid,
                            &mediumIlSidSize)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  TOKEN_MANDATORY_LABEL integrityLevel = {};
  integrityLevel.Label.Attributes = SE_GROUP_INTEGRITY;
  integrityLevel.Label.Sid = reinterpret_cast<PSID>(mediumIlSid);

  if (!::SetTokenInformation(rawResult, TokenIntegrityLevel, &integrityLevel,
                             sizeof(integrityLevel))) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return result.disown();
}

static mozilla::LauncherResult<bool> IsAdminByAppCompat(
    HKEY aRootKey, const wchar_t* aExecutablePath) {
  static const wchar_t kPathToLayers[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\"
      L"AppCompatFlags\\Layers";

  DWORD dataLength = 0;
  LSTATUS status = ::RegGetValueW(aRootKey, kPathToLayers, aExecutablePath,
                                  RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
                                  nullptr, nullptr, &dataLength);
  if (status == ERROR_FILE_NOT_FOUND) {
    return false;
  } else if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  auto valueData = mozilla::MakeUnique<wchar_t[]>(dataLength);
  if (!valueData) {
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_OUTOFMEMORY);
  }

  status = ::RegGetValueW(aRootKey, kPathToLayers, aExecutablePath,
                          RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr,
                          valueData.get(), &dataLength);
  if (status != ERROR_SUCCESS) {
    return LAUNCHER_ERROR_FROM_WIN32(status);
  }

  const wchar_t kRunAsAdmin[] = L"RUNASADMIN";
  const wchar_t kDelimiters[] = L" ";
  wchar_t* tokenContext = nullptr;
  const wchar_t* token = wcstok_s(valueData.get(), kDelimiters, &tokenContext);
  while (token) {
    if (!_wcsnicmp(token, kRunAsAdmin, mozilla::ArrayLength(kRunAsAdmin))) {
      return true;
    }
    token = wcstok_s(nullptr, kDelimiters, &tokenContext);
  }

  return false;
}

namespace mozilla {

// If we're running at an elevated integrity level, re-run ourselves at the
// user's normal integrity level. We do this by locating the active explorer
// shell, and then asking it to do a ShellExecute on our behalf. We do it this
// way to ensure that the child process runs as the original user in the active
// session; an elevated process could be running with different credentials than
// those of the session.
// See https://devblogs.microsoft.com/oldnewthing/20131118-00/?p=2643

LauncherVoidResult LaunchUnelevated(int aArgc, wchar_t* aArgv[]) {
  // We need COM to talk to Explorer. Using ProcessRuntime so that
  // process-global COM configuration is done correctly
  mozilla::mscom::ProcessRuntime mscom(
      mozilla::mscom::ProcessRuntime::ProcessCategory::Launcher);
  if (!mscom) {
    return LAUNCHER_ERROR_FROM_HRESULT(mscom.GetHResult());
  }

  constexpr wchar_t const* kTagArg[1] = {L"--" ATTEMPTING_DEELEVATION_FLAG};
  // Omit argv[0] because ShellExecute doesn't need it in params
  UniquePtr<wchar_t[]> cmdLine(
      MakeCommandLine(aArgc - 1, aArgv + 1, 1, kTagArg));
  if (!cmdLine) {
    return LAUNCHER_ERROR_GENERIC();
  }

  _bstr_t cmd;

  UniquePtr<wchar_t[]> packageFamilyName = mozilla::GetPackageFamilyName();
  if (packageFamilyName) {
    int cmdLen =
        // 22 for the prefix + suffix + null terminator below
        22 + wcslen(packageFamilyName.get());
    wchar_t appCmd[cmdLen];
    swprintf(appCmd, cmdLen, L"shell:appsFolder\\%s!App",
             packageFamilyName.get());
    cmd = appCmd;
  } else {
    cmd = aArgv[0];
  }

  _variant_t args(cmdLine.get());
  _variant_t operation(L"open");
  _variant_t directory;
  _variant_t showCmd(SW_SHOWNORMAL);
  return ShellExecuteByExplorer(cmd, args, operation, directory, showCmd);
}

LauncherResult<ElevationState> GetElevationState(
    const wchar_t* aExecutablePath, mozilla::LauncherFlags aFlags,
    nsAutoHandle& aOutMediumIlToken) {
  aOutMediumIlToken.reset();

  const DWORD tokenFlags = TOKEN_QUERY | TOKEN_DUPLICATE |
                           TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY;
  HANDLE rawToken;
  if (!::OpenProcessToken(::GetCurrentProcess(), tokenFlags, &rawToken)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  nsAutoHandle token(rawToken);

  LauncherResult<TOKEN_ELEVATION_TYPE> elevationType = GetElevationType(token);
  if (elevationType.isErr()) {
    return elevationType.propagateErr();
  }

  Maybe<ElevationState> elevationState;
  switch (elevationType.unwrap()) {
    case TokenElevationTypeLimited:
      return ElevationState::eNormalUser;
    case TokenElevationTypeFull:
      elevationState = Some(ElevationState::eElevated);
      break;
    case TokenElevationTypeDefault: {
      // In this case, UAC is disabled. We do not yet know whether or not we
      // are running at high integrity. If we are at high integrity, we can't
      // relaunch ourselves in a non-elevated state via Explorer, as we would
      // just end up in an infinite loop of launcher processes re-launching
      // themselves.
      LauncherResult<bool> isHighIntegrity = IsHighIntegrity(token);
      if (isHighIntegrity.isErr()) {
        return isHighIntegrity.propagateErr();
      }

      if (!isHighIntegrity.unwrap()) {
        return ElevationState::eNormalUser;
      }

      elevationState = Some(ElevationState::eHighIntegrityNoUAC);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Was a new value added to the enumeration?");
      return LAUNCHER_ERROR_GENERIC();
  }

  MOZ_ASSERT(elevationState.isSome() &&
                 elevationState.value() != ElevationState::eNormalUser,
             "Should have returned earlier for the eNormalUser case.");

  LauncherResult<bool> isAdminByAppCompat =
      IsAdminByAppCompat(HKEY_CURRENT_USER, aExecutablePath);
  if (isAdminByAppCompat.isErr()) {
    return isAdminByAppCompat.propagateErr();
  }

  if (isAdminByAppCompat.unwrap()) {
    elevationState = Some(ElevationState::eHighIntegrityByAppCompat);
  } else {
    isAdminByAppCompat =
        IsAdminByAppCompat(HKEY_LOCAL_MACHINE, aExecutablePath);
    if (isAdminByAppCompat.isErr()) {
      return isAdminByAppCompat.propagateErr();
    }

    if (isAdminByAppCompat.unwrap()) {
      elevationState = Some(ElevationState::eHighIntegrityByAppCompat);
    }
  }

  // A medium IL token is not needed in the following cases.
  // 1) We keep the process elevated (= LauncherFlags::eNoDeelevate)
  // 2) The process was elevated by UAC (= ElevationState::eElevated)
  //    AND the launcher process doesn't wait for the browser process
  if ((aFlags & mozilla::LauncherFlags::eNoDeelevate) ||
      (elevationState.value() == ElevationState::eElevated &&
       !(aFlags & mozilla::LauncherFlags::eWaitForBrowser))) {
    return elevationState.value();
  }

  LauncherResult<HANDLE> tokenResult = GetMediumIntegrityToken(token);
  if (tokenResult.isOk()) {
    aOutMediumIlToken.own(tokenResult.unwrap());
  } else {
    return tokenResult.propagateErr();
  }

  return elevationState.value();
}

}  // namespace mozilla
