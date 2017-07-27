/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPChild.h"
#include "GMPContentChild.h"
#include "GMPProcessChild.h"
#include "GMPLoader.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "GMPDecryptorChild.h"
#include "GMPVideoHost.h"
#include "nsDebugImpl.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "GMPUtils.h"
#include "prio.h"
#include "base/task.h"
#include "base/command_line.h"
#include "widevine-adapter/WidevineAdapter.h"
#include "ChromiumCDMAdapter.h"
#include "GMPLog.h"

using namespace mozilla::ipc;

#ifdef XP_WIN
#include <stdlib.h> // for _exit()
#include "WinUtils.h"
#else
#include <unistd.h> // for _exit()
#endif

#if defined(MOZ_GMP_SANDBOX)
#if defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif

namespace mozilla {

#undef LOG
#undef LOGD

extern LogModule* GetGMPLog();
#define LOG(level, x, ...) MOZ_LOG(GetGMPLog(), (level), (x, ##__VA_ARGS__))
#define LOGD(x, ...) LOG(mozilla::LogLevel::Debug, "GMPChild[pid=%d] " x, (int)base::GetCurrentProcId(), ##__VA_ARGS__)

namespace gmp {

GMPChild::GMPChild()
  : mGMPMessageLoop(MessageLoop::current())
  , mGMPLoader(nullptr)
{
  LOGD("GMPChild ctor");
  nsDebugImpl::SetMultiprocessMode("GMP");
}

GMPChild::~GMPChild()
{
  LOGD("GMPChild dtor");
}

static bool
GetFileBase(const nsAString& aPluginPath,
            nsCOMPtr<nsIFile>& aLibDirectory,
            nsCOMPtr<nsIFile>& aFileBase,
            nsAutoString& aBaseName)
{
  nsresult rv = NS_NewLocalFile(aPluginPath,
                                true, getter_AddRefs(aFileBase));
  if (NS_FAILED(rv)) {
    return false;
  }

  if (NS_FAILED(aFileBase->Clone(getter_AddRefs(aLibDirectory)))) {
    return false;
  }

  nsCOMPtr<nsIFile> parent;
  rv = aFileBase->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_FAILED(rv)) {
    return false;
  }

  aBaseName = Substring(parentLeafName,
                        4,
                        parentLeafName.Length() - 1);
  return true;
}

static bool
GetPluginFile(const nsAString& aPluginPath,
              nsCOMPtr<nsIFile>& aLibDirectory,
              nsCOMPtr<nsIFile>& aLibFile)
{
  nsAutoString baseName;
  GetFileBase(aPluginPath, aLibDirectory, aLibFile, baseName);

#if defined(XP_MACOSX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".dylib");
#elif defined(OS_POSIX)
  nsAutoString binaryName = NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".so");
#elif defined(XP_WIN)
  nsAutoString binaryName =                            baseName + NS_LITERAL_STRING(".dll");
#else
#error not defined
#endif
  aLibFile->AppendRelativePath(binaryName);
  return true;
}

static bool
GetPluginFile(const nsAString& aPluginPath,
              nsCOMPtr<nsIFile>& aLibFile)
{
  nsCOMPtr<nsIFile> unusedlibDir;
  return GetPluginFile(aPluginPath, unusedlibDir, aLibFile);
}

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
static nsCString
GetNativeTarget(nsIFile* aFile)
{
  bool isLink;
  nsCString path;
  aFile->IsSymlink(&isLink);
  if (isLink) {
    aFile->GetNativeTarget(path);
  } else {
    aFile->GetNativePath(path);
  }
  return path;
}

static bool
GetPluginPaths(const nsAString& aPluginPath,
               nsCString &aPluginDirectoryPath,
               nsCString &aPluginFilePath)
{
  nsCOMPtr<nsIFile> libDirectory, libFile;
  if (!GetPluginFile(aPluginPath, libDirectory, libFile)) {
    return false;
  }

  // Mac sandbox rules expect paths to actual files and directories -- not
  // soft links.
  libDirectory->Normalize();
  aPluginDirectoryPath = GetNativeTarget(libDirectory);

  libFile->Normalize();
  aPluginFilePath = GetNativeTarget(libFile);

  return true;
}

static bool
GetAppPaths(nsCString &aAppPath, nsCString &aAppBinaryPath)
{
  nsAutoCString appPath;
  nsAutoCString appBinaryPath(
    (CommandLine::ForCurrentProcess()->argv()[0]).c_str());

  nsAutoCString::const_iterator start, end;
  appBinaryPath.BeginReading(start);
  appBinaryPath.EndReading(end);
  if (RFindInReadable(NS_LITERAL_CSTRING(".app/Contents/MacOS/"), start, end)) {
    end = start;
    ++end; ++end; ++end; ++end;
    appBinaryPath.BeginReading(start);
    appPath.Assign(Substring(start, end));
  } else {
    return false;
  }

  nsCOMPtr<nsIFile> app, appBinary;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appPath),
                                true, getter_AddRefs(app));
  if (NS_FAILED(rv)) {
    return false;
  }
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appBinaryPath),
                       true, getter_AddRefs(appBinary));
  if (NS_FAILED(rv)) {
    return false;
  }

  // Mac sandbox rules expect paths to actual files and directories -- not
  // soft links.
  aAppPath = GetNativeTarget(app);
  appBinaryPath = GetNativeTarget(appBinary);

  return true;
}

bool
GMPChild::SetMacSandboxInfo(MacSandboxPluginType aPluginType)
{
  if (!mGMPLoader) {
    return false;
  }
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    return false;
  }
  nsAutoCString appPath, appBinaryPath;
  if (!GetAppPaths(appPath, appBinaryPath)) {
    return false;
  }

  MacSandboxInfo info;
  info.type = MacSandboxType_Plugin;
  info.shouldLog = Preferences::GetBool("security.sandbox.logging.enabled") ||
                   PR_GetEnv("MOZ_SANDBOX_LOGGING");
  info.pluginInfo.type = aPluginType;
  info.pluginInfo.pluginPath.assign(pluginDirectoryPath.get());
  info.pluginInfo.pluginBinaryPath.assign(pluginFilePath.get());
  info.appPath.assign(appPath.get());
  info.appBinaryPath.assign(appBinaryPath.get());

  mGMPLoader->SetSandboxInfo(&info);
  return true;
}
#endif // XP_MACOSX && MOZ_GMP_SANDBOX

bool
GMPChild::Init(const nsAString& aPluginPath,
               base::ProcessId aParentPid,
               MessageLoop* aIOLoop,
               IPC::Channel* aChannel)
{
  LOGD("%s pluginPath=%s", __FUNCTION__, NS_ConvertUTF16toUTF8(aPluginPath).get());

  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

#ifdef MOZ_CRASHREPORTER
  CrashReporterClient::InitSingleton(this);
#endif

  mPluginPath = aPluginPath;

  return true;
}

GMPErr
GMPChild::GetAPI(const char* aAPIName,
                 void* aHostAPI,
                 void** aPluginAPI,
                 uint32_t aDecryptorId)
{
  if (!mGMPLoader) {
    return GMPGenericErr;
  }
  return mGMPLoader->GetAPI(aAPIName, aHostAPI, aPluginAPI, aDecryptorId);
}

mozilla::ipc::IPCResult
GMPChild::RecvPreloadLibs(const nsCString& aLibs)
{
#ifdef XP_WIN
  // Pre-load DLLs that need to be used by the EME plugin but that can't be
  // loaded after the sandbox has started
  // Items in this must be lowercase!
  static const char *const whitelist[] = {
    "dxva2.dll", // Get monitor information
    "evr.dll", // MFGetStrideForBitmapInfoHeader
    "mfplat.dll", // MFCreateSample, MFCreateAlignedMemoryBuffer, MFCreateMediaType
    "msmpeg2vdec.dll", // H.264 decoder
    "psapi.dll", // For GetMappedFileNameW, see bug 1383611
  };

  nsTArray<nsCString> libs;
  SplitAt(", ", aLibs, libs);
  for (nsCString lib : libs) {
    ToLowerCase(lib);
    for (const char* whiteListedLib : whitelist) {
      if (lib.EqualsASCII(whiteListedLib)) {
        LoadLibraryA(lib.get());
        break;
      }
    }
  }
#endif
  return IPC_OK();
}

bool
GMPChild::ResolveLinks(nsCOMPtr<nsIFile>& aPath)
{
#if defined(XP_WIN)
  return widget::WinUtils::ResolveJunctionPointsAndSymLinks(aPath);
#elif defined(XP_MACOSX)
  nsCString targetPath = GetNativeTarget(aPath);
  nsCOMPtr<nsIFile> newFile;
  if (NS_FAILED(
        NS_NewNativeLocalFile(targetPath, true, getter_AddRefs(newFile)))) {
    return false;
  }
  aPath = newFile;
  return true;
#else
  return true;
#endif
}

bool
GMPChild::GetUTF8LibPath(nsACString& aOutLibPath)
{
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    MOZ_CRASH("Error scanning plugin path");
  }
  aOutLibPath.Assign(pluginFilePath);
  return true;
#else
  nsCOMPtr<nsIFile> libFile;
  if (!GetPluginFile(mPluginPath, libFile)) {
    return false;
  }

  if (!FileExists(libFile)) {
    NS_WARNING("Can't find GMP library file!");
    return false;
  }

  nsAutoString path;
  libFile->GetPath(path);
  aOutLibPath = NS_ConvertUTF16toUTF8(path);

  return true;
#endif
}

#if defined(XP_MACOSX)
#define FIREFOX_FILE NS_LITERAL_STRING("firefox")
#define XUL_LIB_FILE NS_LITERAL_STRING("XUL")
#elif defined(XP_LINUX)
#define FIREFOX_FILE NS_LITERAL_STRING("firefox")
#define XUL_LIB_FILE NS_LITERAL_STRING("libxul.so")
#elif defined(OS_WIN)
#define FIREFOX_FILE NS_LITERAL_STRING("firefox.exe")
#define XUL_LIB_FILE NS_LITERAL_STRING("xul.dll")
#endif

#if defined(XP_MACOSX)
static bool
GetFirefoxAppPath(nsCOMPtr<nsIFile> aPluginContainerPath,
                  nsCOMPtr<nsIFile>& aOutFirefoxAppPath)
{
  // aPluginContainerPath will end with something like:
  // xxxx/NightlyDebug.app/Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container
  MOZ_ASSERT(aPluginContainerPath);
  nsCOMPtr<nsIFile> path = aPluginContainerPath;
  for (int i = 0; i < 4; i++) {
    nsCOMPtr<nsIFile> parent;
    if (NS_FAILED(path->GetParent(getter_AddRefs(parent)))) {
      return false;
    }
    path = parent;
  }
  MOZ_ASSERT(path);
  aOutFirefoxAppPath = path;
  return true;
}
#endif

nsTArray<nsCString>
GMPChild::MakeCDMHostVerificationPaths()
{
  nsTArray<nsCString> paths;

  // Plugin binary path.
  nsCOMPtr<nsIFile> path;
  nsString str;
  if (GetPluginFile(mPluginPath, path) && FileExists(path) &&
      ResolveLinks(path) && NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(NS_ConvertUTF16toUTF8(str));
  }

  // Plugin-container binary path.
  // Note: clang won't let us initialize an nsString from a wstring, so we
  // need to go through UTF8 to get to an nsString.
  const std::string pluginContainer =
    WideToUTF8(CommandLine::ForCurrentProcess()->program());
  path = nullptr;
  str = NS_ConvertUTF8toUTF16(nsDependentCString(pluginContainer.c_str()));
  if (NS_SUCCEEDED(NS_NewLocalFile(str,
                                   true, /* aFollowLinks */
                                   getter_AddRefs(path))) &&
      FileExists(path) && ResolveLinks(path) &&
      NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(nsCString(NS_ConvertUTF16toUTF8(str)));
  } else {
    // Without successfully determining plugin-container's path, we can't
    // determine libxul's or Firefox's. So give up.
    return paths;
  }

  // Firefox application binary path.
  nsCOMPtr<nsIFile> appDir;
#if defined(XP_WIN) || defined(XP_LINUX)
  // Note: re-using 'path' var here, as on Windows/Linux we assume Firefox
  // executable is in the same directory as plugin-container.
  if (NS_SUCCEEDED(path->GetParent(getter_AddRefs(appDir))) &&
      NS_SUCCEEDED(appDir->Clone(getter_AddRefs(path))) &&
      NS_SUCCEEDED(path->Append(FIREFOX_FILE)) && FileExists(path) &&
      ResolveLinks(path) && NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(NS_ConvertUTF16toUTF8(str));
  }
#elif defined(XP_MACOSX)
  // On MacOS the firefox binary is a few parent directories up from
  // plugin-container.
  if (GetFirefoxAppPath(path, appDir) &&
      NS_SUCCEEDED(appDir->Clone(getter_AddRefs(path))) &&
      NS_SUCCEEDED(path->Append(FIREFOX_FILE)) && FileExists(path) &&
      ResolveLinks(path) && NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(NS_ConvertUTF16toUTF8(str));
  }
#endif
  // Libxul path. Note: re-using 'path' var here, as we assume libxul is in
  // the same directory as Firefox executable.
  appDir->GetPath(str);
  if (NS_SUCCEEDED(appDir->Clone(getter_AddRefs(path))) &&
      NS_SUCCEEDED(path->Append(XUL_LIB_FILE)) && FileExists(path) &&
      ResolveLinks(path) && NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(NS_ConvertUTF16toUTF8(str));
  }

  return paths;
}

static nsCString
ToCString(const nsTArray<nsCString>& aStrings)
{
  nsCString result;
  for (const nsCString& s : aStrings) {
    if (!result.IsEmpty()) {
      result.AppendLiteral(",");
    }
    result.Append(s);
  }
  return result;
}

mozilla::ipc::IPCResult
GMPChild::AnswerStartPlugin(const nsString& aAdapter)
{
  LOGD("%s", __FUNCTION__);

  nsCString libPath;
  if (!GetUTF8LibPath(libPath)) {
    return IPC_FAIL_NO_REASON(this);
  }

  auto platformAPI = new GMPPlatformAPI();
  InitPlatformAPI(*platformAPI, this);

  mGMPLoader = MakeUnique<GMPLoader>();
#if defined(MOZ_GMP_SANDBOX)
  if (!mGMPLoader->CanSandbox()) {
    LOGD("%s Can't sandbox GMP, failing", __FUNCTION__);
    delete platformAPI;
    return IPC_FAIL_NO_REASON(this);
  }
#endif

  bool isWidevine = aAdapter.EqualsLiteral("widevine");
  bool isChromium = aAdapter.EqualsLiteral("chromium");
#if defined(MOZ_GMP_SANDBOX) && defined(XP_MACOSX)
  MacSandboxPluginType pluginType = MacSandboxPluginType_GMPlugin_Default;
  if (isWidevine || isChromium) {
    pluginType = MacSandboxPluginType_GMPlugin_EME_Widevine;
  }
  if (!SetMacSandboxInfo(pluginType)) {
    NS_WARNING("Failed to set Mac GMP sandbox info");
    delete platformAPI;
    return IPC_FAIL_NO_REASON(this);
  }
#endif

  GMPAdapter* adapter = nullptr;
  if (isWidevine) {
    adapter = new WidevineAdapter();
  } else if (isChromium) {
    nsTArray<nsCString> paths(MakeCDMHostVerificationPaths());
    GMP_LOG("%s CDM host paths=%s", __func__, ToCString(paths).get());
    adapter = new ChromiumCDMAdapter(Move(paths));
  }

  if (!mGMPLoader->Load(libPath.get(),
                        libPath.Length(),
                        platformAPI,
                        adapter)) {
    NS_WARNING("Failed to load GMP");
    delete platformAPI;
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

MessageLoop*
GMPChild::GMPMessageLoop()
{
  return mGMPMessageLoop;
}

void
GMPChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD("%s reason=%d", __FUNCTION__, aWhy);

  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    MOZ_ASSERT_IF(aWhy == NormalShutdown, !mGMPContentChildren[i - 1]->IsUsed());
    mGMPContentChildren[i - 1]->Close();
  }

  if (mGMPLoader) {
    mGMPLoader->Shutdown();
  }
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    ProcessChild::QuickExit();
  }

#ifdef MOZ_CRASHREPORTER
  CrashReporterClient::DestroySingleton();
#endif
  XRE_ShutdownChildProcess();
}

void
GMPChild::ProcessingError(Result aCode, const char* aReason)
{
  switch (aCode) {
    case MsgDropped:
      _exit(0); // Don't trigger a crash report.
    case MsgNotKnown:
      MOZ_CRASH("aborting because of MsgNotKnown");
    case MsgNotAllowed:
      MOZ_CRASH("aborting because of MsgNotAllowed");
    case MsgPayloadError:
      MOZ_CRASH("aborting because of MsgPayloadError");
    case MsgProcessingError:
      MOZ_CRASH("aborting because of MsgProcessingError");
    case MsgRouteError:
      MOZ_CRASH("aborting because of MsgRouteError");
    case MsgValueError:
      MOZ_CRASH("aborting because of MsgValueError");
    default:
      MOZ_CRASH("not reached");
  }
}

PGMPTimerChild*
GMPChild::AllocPGMPTimerChild()
{
  return new GMPTimerChild(this);
}

bool
GMPChild::DeallocPGMPTimerChild(PGMPTimerChild* aActor)
{
  MOZ_ASSERT(mTimerChild == static_cast<GMPTimerChild*>(aActor));
  mTimerChild = nullptr;
  return true;
}

GMPTimerChild*
GMPChild::GetGMPTimers()
{
  if (!mTimerChild) {
    PGMPTimerChild* sc = SendPGMPTimerConstructor();
    if (!sc) {
      return nullptr;
    }
    mTimerChild = static_cast<GMPTimerChild*>(sc);
  }
  return mTimerChild;
}

PGMPStorageChild*
GMPChild::AllocPGMPStorageChild()
{
  return new GMPStorageChild(this);
}

bool
GMPChild::DeallocPGMPStorageChild(PGMPStorageChild* aActor)
{
  mStorage = nullptr;
  return true;
}

GMPStorageChild*
GMPChild::GetGMPStorage()
{
  if (!mStorage) {
    PGMPStorageChild* sc = SendPGMPStorageConstructor();
    if (!sc) {
      return nullptr;
    }
    mStorage = static_cast<GMPStorageChild*>(sc);
  }
  return mStorage;
}

mozilla::ipc::IPCResult
GMPChild::RecvCrashPluginNow()
{
  MOZ_CRASH();
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPChild::RecvCloseActive()
{
  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    mGMPContentChildren[i - 1]->CloseActive();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPChild::RecvInitGMPContentChild(Endpoint<PGMPContentChild>&& aEndpoint)
{
  GMPContentChild* child =
    mGMPContentChildren.AppendElement(new GMPContentChild(this))->get();
  aEndpoint.Bind(child);
  return IPC_OK();
}

void
GMPChild::GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild)
{
  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    UniquePtr<GMPContentChild>& toDestroy = mGMPContentChildren[i - 1];
    if (toDestroy.get() == aGMPContentChild) {
      SendPGMPContentChildDestroyed();
      RefPtr<DeleteTask<GMPContentChild>> task =
        new DeleteTask<GMPContentChild>(toDestroy.release());
      MessageLoop::current()->PostTask(task.forget());
      mGMPContentChildren.RemoveElementAt(i - 1);
      break;
    }
  }
}

} // namespace gmp
} // namespace mozilla

#undef LOG
#undef LOGD
