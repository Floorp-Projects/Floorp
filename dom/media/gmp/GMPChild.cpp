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
#include "GMPVideoHost.h"
#include "nsDebugImpl.h"
#include "nsExceptionHandler.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"
#include "mozilla/Algorithm.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/TextUtils.h"
#include "GMPUtils.h"
#include "prio.h"
#include "base/task.h"
#include "base/command_line.h"
#include "ChromiumCDMAdapter.h"
#include "GMPLog.h"

using namespace mozilla::ipc;

#ifdef XP_WIN
#  include <stdlib.h>  // for _exit()
#  include "WinUtils.h"
#else
#  include <unistd.h>  // for _exit()
#endif

#if defined(MOZ_SANDBOX)
#  if defined(XP_MACOSX)
#    include "mozilla/Sandbox.h"
#  endif
#endif

namespace mozilla {

#undef LOG
#undef LOGD

extern LogModule* GetGMPLog();
#define LOG(level, x, ...) MOZ_LOG(GetGMPLog(), (level), (x, ##__VA_ARGS__))
#define LOGD(x, ...)                                   \
  LOG(mozilla::LogLevel::Debug, "GMPChild[pid=%d] " x, \
      (int)base::GetCurrentProcId(), ##__VA_ARGS__)

namespace gmp {

GMPChild::GMPChild()
    : mGMPMessageLoop(MessageLoop::current()), mGMPLoader(nullptr) {
  LOGD("GMPChild ctor");
  nsDebugImpl::SetMultiprocessMode("GMP");
}

GMPChild::~GMPChild() { LOGD("GMPChild dtor"); }

static bool GetFileBase(const nsAString& aPluginPath,
                        nsCOMPtr<nsIFile>& aLibDirectory,
                        nsCOMPtr<nsIFile>& aFileBase, nsAutoString& aBaseName) {
  nsresult rv = NS_NewLocalFile(aPluginPath, true, getter_AddRefs(aFileBase));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (NS_WARN_IF(NS_FAILED(aFileBase->Clone(getter_AddRefs(aLibDirectory))))) {
    return false;
  }

  nsCOMPtr<nsIFile> parent;
  rv = aFileBase->GetParent(getter_AddRefs(parent));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  aBaseName = Substring(parentLeafName, 4, parentLeafName.Length() - 1);
  return true;
}

static bool GetPluginFile(const nsAString& aPluginPath,
                          nsCOMPtr<nsIFile>& aLibDirectory,
                          nsCOMPtr<nsIFile>& aLibFile) {
  nsAutoString baseName;
  GetFileBase(aPluginPath, aLibDirectory, aLibFile, baseName);

#if defined(XP_MACOSX)
  nsAutoString binaryName =
      NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".dylib");
#elif defined(OS_POSIX)
  nsAutoString binaryName =
      NS_LITERAL_STRING("lib") + baseName + NS_LITERAL_STRING(".so");
#elif defined(XP_WIN)
  nsAutoString binaryName = baseName + NS_LITERAL_STRING(".dll");
#else
#  error not defined
#endif
  aLibFile->AppendRelativePath(binaryName);
  return true;
}

static bool GetPluginFile(const nsAString& aPluginPath,
                          nsCOMPtr<nsIFile>& aLibFile) {
  nsCOMPtr<nsIFile> unusedlibDir;
  return GetPluginFile(aPluginPath, unusedlibDir, aLibFile);
}

#if defined(XP_MACOSX)
static nsCString GetNativeTarget(nsIFile* aFile) {
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

#  if defined(MOZ_SANDBOX)
static bool GetPluginPaths(const nsAString& aPluginPath,
                           nsCString& aPluginDirectoryPath,
                           nsCString& aPluginFilePath) {
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

static bool GetAppPaths(nsCString& aAppPath, nsCString& aAppBinaryPath) {
  nsAutoCString appPath;
  nsAutoCString appBinaryPath(
      (CommandLine::ForCurrentProcess()->argv()[0]).c_str());

  nsAutoCString::const_iterator start, end;
  appBinaryPath.BeginReading(start);
  appBinaryPath.EndReading(end);
  if (RFindInReadable(NS_LITERAL_CSTRING(".app/Contents/MacOS/"), start, end)) {
    end = start;
    ++end;
    ++end;
    ++end;
    ++end;
    appBinaryPath.BeginReading(start);
    appPath.Assign(Substring(start, end));
  } else {
    return false;
  }

  nsCOMPtr<nsIFile> app, appBinary;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appPath), true,
                                getter_AddRefs(app));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(appBinaryPath), true,
                       getter_AddRefs(appBinary));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Mac sandbox rules expect paths to actual files and directories -- not
  // soft links.
  aAppPath = GetNativeTarget(app);
  appBinaryPath = GetNativeTarget(appBinary);

  return true;
}

bool GMPChild::SetMacSandboxInfo(bool aAllowWindowServer) {
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
  info.type = MacSandboxType_GMP;
  info.shouldLog = Preferences::GetBool("security.sandbox.logging.enabled") ||
                   PR_GetEnv("MOZ_SANDBOX_LOGGING");
  info.hasWindowServer = aAllowWindowServer;
  info.pluginPath.assign(pluginDirectoryPath.get());
  info.pluginBinaryPath.assign(pluginFilePath.get());
  info.appPath.assign(appPath.get());
  info.appBinaryPath.assign(appBinaryPath.get());

  mGMPLoader->SetSandboxInfo(&info);
  return true;
}
#  endif  // MOZ_SANDBOX
#endif    // XP_MACOSX

bool GMPChild::Init(const nsAString& aPluginPath, base::ProcessId aParentPid,
                    MessageLoop* aIOLoop, IPC::Channel* aChannel) {
  LOGD("%s pluginPath=%s", __FUNCTION__,
       NS_ConvertUTF16toUTF8(aPluginPath).get());

  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

  CrashReporterClient::InitSingleton(this);

  mPluginPath = aPluginPath;

  return true;
}

mozilla::ipc::IPCResult GMPChild::RecvProvideStorageId(
    const nsCString& aStorageId) {
  LOGD("%s", __FUNCTION__);
  mStorageId = aStorageId;
  return IPC_OK();
}

GMPErr GMPChild::GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI,
                        uint32_t aDecryptorId) {
  if (!mGMPLoader) {
    return GMPGenericErr;
  }
  return mGMPLoader->GetAPI(aAPIName, aHostAPI, aPluginAPI, aDecryptorId);
}

mozilla::ipc::IPCResult GMPChild::RecvPreloadLibs(const nsCString& aLibs) {
#ifdef XP_WIN
  // Pre-load DLLs that need to be used by the EME plugin but that can't be
  // loaded after the sandbox has started
  // Items in this must be lowercase!
  constexpr static const char16_t* whitelist[] = {
      u"dxva2.dll",        // Get monitor information
      u"evr.dll",          // MFGetStrideForBitmapInfoHeader
      u"mfplat.dll",       // MFCreateSample, MFCreateAlignedMemoryBuffer,
                           // MFCreateMediaType
      u"msmpeg2vdec.dll",  // H.264 decoder
      u"psapi.dll",        // For GetMappedFileNameW, see bug 1383611
  };
  constexpr static bool (*IsASCII)(const char16_t*) =
      IsAsciiNullTerminated<char16_t>;
  static_assert(AllOf(std::begin(whitelist), std::end(whitelist), IsASCII),
                "Items in the whitelist must not contain non-ASCII "
                "characters!");

  nsTArray<nsCString> libs;
  SplitAt(", ", aLibs, libs);
  for (nsCString lib : libs) {
    ToLowerCase(lib);
    for (const char16_t* whiteListedLib : whitelist) {
      if (nsDependentString(whiteListedLib)
              .EqualsASCII(lib.Data(), lib.Length())) {
        LoadLibraryW(char16ptr_t(whiteListedLib));
        break;
      }
    }
  }
#endif
  return IPC_OK();
}

static bool ResolveLinks(nsCOMPtr<nsIFile>& aPath) {
#if defined(XP_WIN)
  return widget::WinUtils::ResolveJunctionPointsAndSymLinks(aPath);
#elif defined(XP_MACOSX)
  nsCString targetPath = GetNativeTarget(aPath);
  nsCOMPtr<nsIFile> newFile;
  if (NS_WARN_IF(NS_FAILED(
          NS_NewNativeLocalFile(targetPath, true, getter_AddRefs(newFile))))) {
    return false;
  }
  aPath = newFile;
  return true;
#else
  return true;
#endif
}

bool GMPChild::GetUTF8LibPath(nsACString& aOutLibPath) {
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
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

static nsCOMPtr<nsIFile> AppendFile(nsCOMPtr<nsIFile>&& aFile,
                                    const nsString& aStr) {
  return (aFile && NS_SUCCEEDED(aFile->Append(aStr))) ? aFile : nullptr;
}

static nsCOMPtr<nsIFile> CloneFile(const nsCOMPtr<nsIFile>& aFile) {
  nsCOMPtr<nsIFile> clone;
  return (aFile && NS_SUCCEEDED(aFile->Clone(getter_AddRefs(clone)))) ? clone
                                                                      : nullptr;
}

static nsCOMPtr<nsIFile> GetParentFile(const nsCOMPtr<nsIFile>& aFile) {
  nsCOMPtr<nsIFile> parent;
  return (aFile && NS_SUCCEEDED(aFile->GetParent(getter_AddRefs(parent))))
             ? parent
             : nullptr;
}

#if defined(XP_WIN)
static bool IsFileLeafEqualToASCII(const nsCOMPtr<nsIFile>& aFile,
                                   const char* aStr) {
  nsAutoString leafName;
  return aFile && NS_SUCCEEDED(aFile->GetLeafName(leafName)) &&
         leafName.EqualsASCII(aStr);
}
#endif

#if defined(XP_WIN)
#  define FIREFOX_FILE NS_LITERAL_STRING("firefox.exe")
#  define XUL_LIB_FILE NS_LITERAL_STRING("xul.dll")
#elif defined(XP_MACOSX)
#  define FIREFOX_FILE NS_LITERAL_STRING("firefox")
#  define XUL_LIB_FILE NS_LITERAL_STRING("XUL")
#else
#  define FIREFOX_FILE NS_LITERAL_STRING("firefox")
#  define XUL_LIB_FILE NS_LITERAL_STRING("libxul.so")
#endif

static nsCOMPtr<nsIFile> GetFirefoxAppPath(
    nsCOMPtr<nsIFile> aPluginContainerPath) {
  MOZ_ASSERT(aPluginContainerPath);
#if defined(XP_MACOSX)
  // On MacOS the firefox binary is a few parent directories up from
  // plugin-container.
  // aPluginContainerPath will end with something like:
  // xxxx/NightlyDebug.app/Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container
  nsCOMPtr<nsIFile> path = aPluginContainerPath;
  for (int i = 0; i < 4; i++) {
    path = GetParentFile(path);
  }
  return path;
#else
  nsCOMPtr<nsIFile> parent = GetParentFile(aPluginContainerPath);
#  if XP_WIN
  if (IsFileLeafEqualToASCII(parent, "i686")) {
    // We must be on Windows on ARM64, where the plugin-container path will
    // be in the 'i686' subdir. The firefox.exe is in the parent directory.
    parent = GetParentFile(parent);
  }
#  endif
  return parent;
#endif
}

#if defined(XP_MACOSX)
static bool GetSigPath(const int aRelativeLayers,
                       const nsString& aTargetSigFileName,
                       nsCOMPtr<nsIFile> aExecutablePath,
                       nsCOMPtr<nsIFile>& aOutSigPath) {
  // The sig file will be located in
  // xxxx/NightlyDebug.app/Contents/Resources/XUL.sig
  // xxxx/NightlyDebug.app/Contents/Resources/firefox.sig
  // xxxx/NightlyDebug.app/Contents/MacOS/plugin-container.app/Contents/Resources/plugin-container.sig
  // On MacOS the sig file is a few parent directories up from
  // its executable file.
  // Start to search the path from the path of the executable file we provided.
  MOZ_ASSERT(aExecutablePath);
  nsCOMPtr<nsIFile> path = aExecutablePath;
  for (int i = 0; i < aRelativeLayers; i++) {
    nsCOMPtr<nsIFile> parent;
    if (NS_WARN_IF(NS_FAILED(path->GetParent(getter_AddRefs(parent))))) {
      return false;
    }
    path = parent;
  }
  MOZ_ASSERT(path);
  aOutSigPath = path;
  return NS_SUCCEEDED(path->Append(NS_LITERAL_STRING("Resources"))) &&
         NS_SUCCEEDED(path->Append(aTargetSigFileName));
}
#endif

static bool AppendHostPath(nsCOMPtr<nsIFile>& aFile,
                           nsTArray<Pair<nsCString, nsCString>>& aPaths) {
  nsString str;
  if (!FileExists(aFile) || !ResolveLinks(aFile) ||
      NS_FAILED(aFile->GetPath(str))) {
    return false;
  }

  nsCString filePath = NS_ConvertUTF16toUTF8(str);
  nsCString sigFilePath;
#if defined(XP_MACOSX)
  nsAutoString binary;
  if (NS_FAILED(aFile->GetLeafName(binary))) {
    return false;
  }
  binary.Append(NS_LITERAL_STRING(".sig"));
  nsCOMPtr<nsIFile> sigFile;
  if (GetSigPath(2, binary, aFile, sigFile) &&
      NS_SUCCEEDED(sigFile->GetPath(str))) {
    sigFilePath = NS_ConvertUTF16toUTF8(str);
  } else {
    // Cannot successfully get the sig file path.
    // Assume it is located at the same place as plugin-container
    // alternatively.
    sigFilePath =
        nsCString(NS_ConvertUTF16toUTF8(str) + NS_LITERAL_CSTRING(".sig"));
  }
#else
  sigFilePath =
      nsCString(NS_ConvertUTF16toUTF8(str) + NS_LITERAL_CSTRING(".sig"));
#endif
  aPaths.AppendElement(MakePair(std::move(filePath), std::move(sigFilePath)));
  return true;
}

nsTArray<Pair<nsCString, nsCString>> GMPChild::MakeCDMHostVerificationPaths() {
  // Record the file path and its sig file path.
  nsTArray<Pair<nsCString, nsCString>> paths;
  // Plugin binary path.
  nsCOMPtr<nsIFile> path;
  nsString str;
  if (GetPluginFile(mPluginPath, path) && FileExists(path) &&
      ResolveLinks(path) && NS_SUCCEEDED(path->GetPath(str))) {
    paths.AppendElement(MakePair(
        nsCString(NS_ConvertUTF16toUTF8(str)),
        nsCString(NS_ConvertUTF16toUTF8(str) + NS_LITERAL_CSTRING(".sig"))));
  }

  // Plugin-container binary path.
  // Note: clang won't let us initialize an nsString from a wstring, so we
  // need to go through UTF8 to get to an nsString.
  const std::string pluginContainer =
      WideToUTF8(CommandLine::ForCurrentProcess()->program());
  path = nullptr;
  str = NS_ConvertUTF8toUTF16(nsDependentCString(pluginContainer.c_str()));
  if (NS_FAILED(NS_NewLocalFile(str, true, /* aFollowLinks */
                                getter_AddRefs(path))) ||
      !AppendHostPath(path, paths)) {
    // Without successfully determining plugin-container's path, we can't
    // determine libxul's or Firefox's. So give up.
    return paths;
  }

#if defined(XP_WIN)
  // On Windows on ARM64, we should also append the x86 plugin-container's
  // xul.dll.
  const bool isWindowsOnARM64 =
      IsFileLeafEqualToASCII(GetParentFile(path), "i686");
  if (isWindowsOnARM64) {
    nsCOMPtr<nsIFile> x86XulPath =
        AppendFile(GetParentFile(path), XUL_LIB_FILE);
    if (!AppendHostPath(x86XulPath, paths)) {
      return paths;
    }
  }
#endif

  // Firefox application binary path.
  nsCOMPtr<nsIFile> appDir = GetFirefoxAppPath(path);
  path = AppendFile(CloneFile(appDir), FIREFOX_FILE);
  if (!AppendHostPath(path, paths)) {
    return paths;
  }

  // Libxul path. Note: re-using 'appDir' var here, as we assume libxul is in
  // the same directory as Firefox executable.
  appDir->GetPath(str);
  path = AppendFile(CloneFile(appDir), XUL_LIB_FILE);
  if (!AppendHostPath(path, paths)) {
    return paths;
  }

  return paths;
}

static nsCString ToCString(const nsTArray<Pair<nsCString, nsCString>>& aPairs) {
  nsCString result;
  for (const auto& p : aPairs) {
    if (!result.IsEmpty()) {
      result.AppendLiteral(",");
    }
    result.Append(
        nsPrintfCString("(%s,%s)", p.first().get(), p.second().get()));
  }
  return result;
}

mozilla::ipc::IPCResult GMPChild::AnswerStartPlugin(const nsString& aAdapter) {
  LOGD("%s", __FUNCTION__);

  nsCString libPath;
  if (!GetUTF8LibPath(libPath)) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::GMPLibraryPath,
        NS_ConvertUTF16toUTF8(mPluginPath));

#ifdef XP_WIN
    return IPC_FAIL(this,
                    nsPrintfCString("Failed to get lib path with error(%d).",
                                    GetLastError())
                        .get());
#else
    return IPC_FAIL(this, "Failed to get lib path.");
#endif
  }

  auto platformAPI = new GMPPlatformAPI();
  InitPlatformAPI(*platformAPI, this);

  mGMPLoader = MakeUnique<GMPLoader>();
#if defined(MOZ_SANDBOX)
  if (!mGMPLoader->CanSandbox()) {
    LOGD("%s Can't sandbox GMP, failing", __FUNCTION__);
    delete platformAPI;
    return IPC_FAIL(this, "Can't sandbox GMP.");
  }
#endif
  bool isChromium = aAdapter.EqualsLiteral("chromium");
#if defined(MOZ_SANDBOX) && defined(XP_MACOSX)
  // Use of the chromium adapter indicates we are going to be
  // running the Widevine plugin which requires access to the
  // WindowServer in the Mac GMP sandbox policy.
  if (!SetMacSandboxInfo(isChromium /* allow-window-server */)) {
    NS_WARNING("Failed to set Mac GMP sandbox info");
    delete platformAPI;
    return IPC_FAIL(
        this, nsPrintfCString("Failed to set Mac GMP sandbox info.").get());
  }
#endif

  GMPAdapter* adapter = nullptr;
  if (isChromium) {
    auto&& paths = MakeCDMHostVerificationPaths();
    GMP_LOG("%s CDM host paths=%s", __func__, ToCString(paths).get());
    adapter = new ChromiumCDMAdapter(std::move(paths));
  }

  if (!mGMPLoader->Load(libPath.get(), libPath.Length(), platformAPI,
                        adapter)) {
    NS_WARNING("Failed to load GMP");
    delete platformAPI;
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::GMPLibraryPath,
        NS_ConvertUTF16toUTF8(mPluginPath));

#ifdef XP_WIN
    return IPC_FAIL(this, nsPrintfCString("Failed to load GMP with error(%d).",
                                          GetLastError())
                              .get());
#else
    return IPC_FAIL(this, "Failed to load GMP.");
#endif
  }

  return IPC_OK();
}

MessageLoop* GMPChild::GMPMessageLoop() { return mGMPMessageLoop; }

void GMPChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOGD("%s reason=%d", __FUNCTION__, aWhy);

  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    MOZ_ASSERT_IF(aWhy == NormalShutdown,
                  !mGMPContentChildren[i - 1]->IsUsed());
    mGMPContentChildren[i - 1]->Close();
  }

  if (mGMPLoader) {
    mGMPLoader->Shutdown();
  }
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    ProcessChild::QuickExit();
  }

  CrashReporterClient::DestroySingleton();

  XRE_ShutdownChildProcess();
}

void GMPChild::ProcessingError(Result aCode, const char* aReason) {
  switch (aCode) {
    case MsgDropped:
      _exit(0);  // Don't trigger a crash report.
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

PGMPTimerChild* GMPChild::AllocPGMPTimerChild() {
  return new GMPTimerChild(this);
}

bool GMPChild::DeallocPGMPTimerChild(PGMPTimerChild* aActor) {
  MOZ_ASSERT(mTimerChild == static_cast<GMPTimerChild*>(aActor));
  mTimerChild = nullptr;
  return true;
}

GMPTimerChild* GMPChild::GetGMPTimers() {
  if (!mTimerChild) {
    PGMPTimerChild* sc = SendPGMPTimerConstructor();
    if (!sc) {
      return nullptr;
    }
    mTimerChild = static_cast<GMPTimerChild*>(sc);
  }
  return mTimerChild;
}

PGMPStorageChild* GMPChild::AllocPGMPStorageChild() {
  return new GMPStorageChild(this);
}

bool GMPChild::DeallocPGMPStorageChild(PGMPStorageChild* aActor) {
  mStorage = nullptr;
  return true;
}

GMPStorageChild* GMPChild::GetGMPStorage() {
  if (!mStorage) {
    PGMPStorageChild* sc = SendPGMPStorageConstructor();
    if (!sc) {
      return nullptr;
    }
    mStorage = static_cast<GMPStorageChild*>(sc);
  }
  return mStorage;
}

mozilla::ipc::IPCResult GMPChild::RecvCrashPluginNow() {
  MOZ_CRASH();
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPChild::RecvCloseActive() {
  for (uint32_t i = mGMPContentChildren.Length(); i > 0; i--) {
    mGMPContentChildren[i - 1]->CloseActive();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPChild::RecvInitGMPContentChild(
    Endpoint<PGMPContentChild>&& aEndpoint) {
  GMPContentChild* child =
      mGMPContentChildren.AppendElement(new GMPContentChild(this))->get();
  aEndpoint.Bind(child);
  return IPC_OK();
}

void GMPChild::GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild) {
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

}  // namespace gmp
}  // namespace mozilla

#undef LOG
#undef LOGD
