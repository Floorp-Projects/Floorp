/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPProcessParent.h"
#include "GMPUtils.h"
#include "nsIRunnable.h"
#ifdef XP_WIN
#  include "WinUtils.h"
#endif
#include "GMPLog.h"

#include "base/string_util.h"
#include "base/process_util.h"

#include <string>

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Omnijar.h"
#  include "mozilla/Preferences.h"
#  include "mozilla/Sandbox.h"
#  include "mozilla/SandboxSettings.h"
#  include "nsMacUtilsImpl.h"
#endif

using std::string;
using std::vector;

using mozilla::gmp::GMPProcessParent;
using mozilla::ipc::GeckoChildProcessHost;

#ifdef MOZ_WIDGET_ANDROID
static const int kInvalidFd = -1;
#endif

namespace mozilla::gmp {

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool GMPProcessParent::sLaunchWithMacSandbox = true;
bool GMPProcessParent::sMacSandboxGMPLogging = false;
#  if defined(DEBUG)
bool GMPProcessParent::sIsMainThreadInitDone = false;
#  endif
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
/* static */
void GMPProcessParent::InitStaticMainThread() {
  // The GMPProcessParent constructor is called off the
  // main thread. Do main thread initialization here.
  MOZ_ASSERT(NS_IsMainThread());
  sMacSandboxGMPLogging =
      Preferences::GetBool("security.sandbox.logging.enabled") ||
      PR_GetEnv("MOZ_SANDBOX_GMP_LOGGING") || PR_GetEnv("MOZ_SANDBOX_LOGGING");
  GMP_LOG_DEBUG("GMPProcessParent::InitStaticMainThread: sandbox logging=%s",
                sMacSandboxGMPLogging ? "true" : "false");
#  if defined(DEBUG)
  sIsMainThreadInitDone = true;
#  endif
}
#endif

GMPProcessParent::GMPProcessParent(const std::string& aGMPPath)
    : GeckoChildProcessHost(GeckoProcessType_GMPlugin),
      mGMPPath(aGMPPath)
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
      ,
      mRequiresWindowServer(false)
#endif
{
  MOZ_COUNT_CTOR(GMPProcessParent);
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  MOZ_ASSERT(sIsMainThreadInitDone == true);
  mDisableOSActivityMode = sLaunchWithMacSandbox;
#endif
}

GMPProcessParent::~GMPProcessParent() { MOZ_COUNT_DTOR(GMPProcessParent); }

bool GMPProcessParent::Launch(int32_t aTimeoutMs) {
  vector<string> args;

#ifdef ALLOW_GECKO_CHILD_PROCESS_ARCH
  GMP_LOG_DEBUG("GMPProcessParent::Launch() mLaunchArch: %d", mLaunchArch);
#  if defined(XP_MACOSX)
  mLaunchOptions->arch = mLaunchArch;
  if (mLaunchArch == base::PROCESS_ARCH_X86_64) {
    mLaunchOptions->env_map["MOZ_SHMEM_PAGESIZE_16K"] = 1;
  }
#  endif
#endif

  // Resolve symlinks in the plugin path. The sandbox prevents
  // resolving symlinks in the child process if access to link
  // source file is denied.
#ifdef XP_WIN
  nsAutoString normalizedPath;
#else
  nsAutoCString normalizedPath;
#endif
  nsresult rv = NormalizePath(mGMPPath.c_str(), normalizedPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG(
        "GMPProcessParent::Launch: "
        "plugin path normaliziation failed for path: %s",
        mGMPPath.c_str());
  }

#ifdef XP_WIN
  std::wstring wGMPPath;
  if (NS_SUCCEEDED(rv)) {
    wGMPPath = normalizedPath.get();
  } else {
    wGMPPath = UTF8ToWide(mGMPPath.c_str());
  }

  // The sandbox doesn't allow file system rules where the paths contain
  // symbolic links or junction points. Sometimes the Users folder has been
  // moved to another drive using a junction point, so allow for this specific
  // case. See bug 1236680 for details.
  if (NS_WARN_IF(
          !widget::WinUtils::ResolveJunctionPointsAndSymLinks(wGMPPath))) {
    GMP_LOG_DEBUG("ResolveJunctionPointsAndSymLinks failed for GMP path=%S",
                  wGMPPath.c_str());
    return false;
  }
  GMP_LOG_DEBUG("GMPProcessParent::Launch() resolved path to %S",
                wGMPPath.c_str());

#  ifdef MOZ_SANDBOX
  // If the GMP path is a network path that is not mapped to a drive letter,
  // then we need to fix the path format for the sandbox rule.
  wchar_t volPath[MAX_PATH];
  if (::GetVolumePathNameW(wGMPPath.c_str(), volPath, MAX_PATH) &&
      ::GetDriveTypeW(volPath) == DRIVE_REMOTE &&
      wGMPPath.compare(0, 2, L"\\\\") == 0) {
    std::wstring sandboxGMPPath(wGMPPath);
    sandboxGMPPath.insert(1, L"??\\UNC");
    mAllowedFilesRead.push_back(sandboxGMPPath + L"\\*");
  } else {
    mAllowedFilesRead.push_back(wGMPPath + L"\\*");
  }
#  endif

  args.push_back(WideToUTF8(wGMPPath));
#else
  if (NS_SUCCEEDED(rv)) {
    args.push_back(normalizedPath.get());
  } else {
    args.push_back(mGMPPath);
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  // Add dummy values for pref and pref map to the file descriptors remapping
  // table. See bug 1440207 and 1481139.
  AddFdToRemap(kInvalidFd, kInvalidFd);
  AddFdToRemap(kInvalidFd, kInvalidFd);
#endif
  return SyncLaunch(args, aTimeoutMs);
}

void GMPProcessParent::Delete(nsCOMPtr<nsIRunnable> aCallback) {
  mDeletedCallback = aCallback;
  XRE_GetIOMessageLoop()->PostTask(NewNonOwningRunnableMethod(
      "gmp::GMPProcessParent::DoDelete", this, &GMPProcessParent::DoDelete));
}

void GMPProcessParent::DoDelete() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (mDeletedCallback) {
    mDeletedCallback->Run();
  }

  Destroy();
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool GMPProcessParent::IsMacSandboxLaunchEnabled() {
  return sLaunchWithMacSandbox;
}

void GMPProcessParent::SetRequiresWindowServer(bool aRequiresWindowServer) {
  mRequiresWindowServer = aRequiresWindowServer;
}

bool GMPProcessParent::FillMacSandboxInfo(MacSandboxInfo& aInfo) {
  aInfo.type = MacSandboxType_GMP;
  aInfo.hasWindowServer = mRequiresWindowServer;
  aInfo.shouldLog = (aInfo.shouldLog || sMacSandboxGMPLogging);
  nsAutoCString appPath;
  if (!nsMacUtilsImpl::GetAppPath(appPath)) {
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: failed to get app path");
    return false;
  }
  aInfo.appPath.assign(appPath.get());

  GMP_LOG_DEBUG(
      "GMPProcessParent::FillMacSandboxInfo: "
      "plugin dir path: %s",
      mGMPPath.c_str());
  nsCOMPtr<nsIFile> pluginDir;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(mGMPPath.c_str()), true,
                                getter_AddRefs(pluginDir));
  if (NS_FAILED(rv)) {
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: "
        "NS_NewLocalFile failed for plugin dir, rv=%d",
        uint32_t(rv));
    return false;
  }

  rv = pluginDir->Normalize();
  if (NS_FAILED(rv)) {
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: "
        "failed to normalize plugin dir path, rv=%d",
        uint32_t(rv));
    return false;
  }

  nsAutoCString resolvedPluginPath;
  pluginDir->GetNativePath(resolvedPluginPath);
  aInfo.pluginPath.assign(resolvedPluginPath.get());
  GMP_LOG_DEBUG(
      "GMPProcessParent::FillMacSandboxInfo: "
      "resolved plugin dir path: %s",
      resolvedPluginPath.get());

  if (!mozilla::IsPackagedBuild()) {
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: IsPackagedBuild()=false");

    // Repo dir
    nsCOMPtr<nsIFile> repoDir;
    rv = nsMacUtilsImpl::GetRepoDir(getter_AddRefs(repoDir));
    if (NS_FAILED(rv)) {
      GMP_LOG_DEBUG(
          "GMPProcessParent::FillMacSandboxInfo: failed to get repo dir");
      return false;
    }
    nsCString repoDirPath;
    Unused << repoDir->GetNativePath(repoDirPath);
    aInfo.testingReadPath1 = repoDirPath.get();
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: "
        "repo dir path: %s",
        repoDirPath.get());

    // Object dir
    nsCOMPtr<nsIFile> objDir;
    rv = nsMacUtilsImpl::GetObjDir(getter_AddRefs(objDir));
    if (NS_FAILED(rv)) {
      GMP_LOG_DEBUG(
          "GMPProcessParent::FillMacSandboxInfo: failed to get object dir");
      return false;
    }
    nsCString objDirPath;
    Unused << objDir->GetNativePath(objDirPath);
    aInfo.testingReadPath2 = objDirPath.get();
    GMP_LOG_DEBUG(
        "GMPProcessParent::FillMacSandboxInfo: "
        "object dir path: %s",
        objDirPath.get());
  }
  return true;
}
#endif

nsresult GMPProcessParent::NormalizePath(const char* aPath,
                                         PathString& aNormalizedPath) {
  nsCOMPtr<nsIFile> fileOrDir;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(aPath), true,
                                getter_AddRefs(fileOrDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = fileOrDir->Normalize();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef XP_WIN
  return fileOrDir->GetTarget(aNormalizedPath);
#else
  bool isLink = false;
  rv = fileOrDir->IsSymlink(&isLink);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isLink) {
    return fileOrDir->GetNativeTarget(aNormalizedPath);
  }
  return fileOrDir->GetNativePath(aNormalizedPath);
#endif
}

}  // namespace mozilla::gmp
