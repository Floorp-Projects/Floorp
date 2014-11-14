/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPChild.h"
#include "GMPProcessChild.h"
#include "GMPLoader.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "GMPAudioDecoderChild.h"
#include "GMPDecryptorChild.h"
#include "GMPVideoHost.h"
#include "nsDebugImpl.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"
#include "mozilla/dom/CrashReporterChild.h"
#ifdef XP_WIN
#include <fstream>
#include "nsCRT.h"
#endif

using mozilla::dom::CrashReporterChild;

#ifdef XP_WIN
#include <stdlib.h> // for _exit()
#else
#include <unistd.h> // for _exit()
#endif

#if defined(MOZ_SANDBOX) && defined(XP_WIN)
#define TARGET_SANDBOX_EXPORTS
#include "mozilla/sandboxTarget.h"
#elif defined (MOZ_GMP_SANDBOX)
#if defined(XP_LINUX) || defined(XP_MACOSX)
#include "mozilla/Sandbox.h"
#endif
#endif

namespace mozilla {
namespace gmp {

GMPChild::GMPChild()
  : mAsyncShutdown(nullptr)
  , mGMPMessageLoop(MessageLoop::current())
  , mGMPLoader(nullptr)
{
  nsDebugImpl::SetMultiprocessMode("GMP");
}

GMPChild::~GMPChild()
{
}

static bool
GetFileBase(const std::string& aPluginPath,
#if defined(XP_MACOSX)
            nsCOMPtr<nsIFile>& aLibDirectory,
#endif
            nsCOMPtr<nsIFile>& aFileBase,
            nsAutoString& aBaseName)
{
  nsDependentCString pluginPath(aPluginPath.c_str());

  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(pluginPath),
                                true, getter_AddRefs(aFileBase));
  if (NS_FAILED(rv)) {
    return false;
  }

#if defined(XP_MACOSX)
  if (NS_FAILED(aFileBase->Clone(getter_AddRefs(aLibDirectory)))) {
    return false;
  }
#endif

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
GetPluginFile(const std::string& aPluginPath,
#if defined(XP_MACOSX)
              nsCOMPtr<nsIFile>& aLibDirectory,
#endif
              nsCOMPtr<nsIFile>& aLibFile)
{
  nsAutoString baseName;
#ifdef XP_MACOSX
  GetFileBase(aPluginPath, aLibDirectory, aLibFile, baseName);
#else
  GetFileBase(aPluginPath, aLibFile, baseName);
#endif

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

#ifdef XP_WIN
static bool
GetInfoFile(const std::string& aPluginPath,
            nsCOMPtr<nsIFile>& aInfoFile)
{
  nsAutoString baseName;
  GetFileBase(aPluginPath, aInfoFile, baseName);
  nsAutoString infoFileName = baseName + NS_LITERAL_STRING(".info");
  aInfoFile->AppendRelativePath(infoFileName);
  return true;
}
#endif

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
static bool
GetPluginPaths(const std::string& aPluginPath,
               nsCString &aPluginDirectoryPath,
               nsCString &aPluginFilePath)
{
  nsCOMPtr<nsIFile> libDirectory, libFile;
  if (!GetPluginFile(aPluginPath, libDirectory, libFile)) {
    return false;
  }

  // Mac sandbox rules expect paths to actual files and directories -- not
  // soft links.
  bool isLink;
  libDirectory->IsSymlink(&isLink);
  if (isLink) {
    libDirectory->GetNativeTarget(aPluginDirectoryPath);
  } else {
    libDirectory->GetNativePath(aPluginDirectoryPath);
  }
  libFile->IsSymlink(&isLink);
  if (isLink) {
    libFile->GetNativeTarget(aPluginFilePath);
  } else {
    libFile->GetNativePath(aPluginFilePath);
  }

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

  bool isLink;
  app->IsSymlink(&isLink);
  if (isLink) {
    app->GetNativeTarget(aAppPath);
  } else {
    app->GetNativePath(aAppPath);
  }
  appBinary->IsSymlink(&isLink);
  if (isLink) {
    appBinary->GetNativeTarget(aAppBinaryPath);
  } else {
    appBinary->GetNativePath(aAppBinaryPath);
  }

  return true;
}

void
GMPChild::StartMacSandbox()
{
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    MOZ_CRASH("Error scanning plugin path");
  }
  nsAutoCString appPath, appBinaryPath;
  if (!GetAppPaths(appPath, appBinaryPath)) {
    MOZ_CRASH("Error resolving child process path");
  }

  MacSandboxInfo info;
  info.type = MacSandboxType_Plugin;
  info.pluginInfo.type = MacSandboxPluginType_GMPlugin_Default;
  info.pluginInfo.pluginPath.Assign(pluginDirectoryPath);
  mPluginBinaryPath.Assign(pluginFilePath);
  info.pluginInfo.pluginBinaryPath.Assign(pluginFilePath);
  info.appPath.Assign(appPath);
  info.appBinaryPath.Assign(appBinaryPath);

  nsAutoCString err;
  if (!mozilla::StartMacSandbox(info, err)) {
    NS_WARNING(err.get());
    MOZ_CRASH("sandbox_init() failed");
  }
}
#endif // XP_MACOSX && MOZ_GMP_SANDBOX

void
GMPChild::CheckThread()
{
  MOZ_ASSERT(mGMPMessageLoop == MessageLoop::current());
}

bool
GMPChild::Init(const std::string& aPluginPath,
               base::ProcessHandle aParentProcessHandle,
               MessageLoop* aIOLoop,
               IPC::Channel* aChannel)
{
  if (!Open(aChannel, aParentProcessHandle, aIOLoop)) {
    return false;
  }

#ifdef MOZ_CRASHREPORTER
  SendPCrashReporterConstructor(CrashReporter::CurrentThreadId());
#endif

  mPluginPath = aPluginPath;
  return true;
}

bool
GMPChild::RecvSetNodeId(const nsCString& aNodeId)
{
  // Store the per origin salt for the node id. Note: we do this in a
  // separate message than RecvStartPlugin() so that the string is not
  // sitting in a string on the IPC code's call stack.
  mNodeId = std::string(aNodeId.BeginReading(), aNodeId.EndReading());
  return true;
}

GMPErr
GMPChild::GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI)
{
  if (!mGMPLoader) {
    return GMPGenericErr;
  }
  return mGMPLoader->GetAPI(aAPIName, aHostAPI, aPluginAPI);
}

#ifdef XP_WIN
// Pre-load DLLs that need to be used by the EME plugin but that can't be
// loaded after the sandbox has started
bool
GMPChild::PreLoadLibraries(const std::string& aPluginPath)
{
  // This must be in sorted order and lowercase!
  static const char* whitelist[] =
    {
       "d3d9.dll", // Create an `IDirect3D9` to get adapter information
       "dxva2.dll", // Get monitor information
       "msauddecmft.dll", // AAC decoder (on Windows 8)
       "msmpeg2adec.dll", // AAC decoder (on Windows 7)
       "msmpeg2vdec.dll", // H.264 decoder
    };
  static const int whitelistLen = sizeof(whitelist) / sizeof(whitelist[0]);

  nsCOMPtr<nsIFile> infoFile;
  GetInfoFile(aPluginPath, infoFile);

  nsString path;
  infoFile->GetPath(path);

  std::ifstream stream;
  stream.open(path.get());
  if (!stream.good()) {
    NS_WARNING("Failure opening info file for required DLLs");
    return false;
  }

  do {
    std::string line;
    getline(stream, line);
    if (stream.fail()) {
      NS_WARNING("Failure reading info file for required DLLs");
      return false;
    }
    std::transform(line.begin(), line.end(), line.begin(), tolower);
    static const char* prefix = "libraries:";
    static const int prefixLen = strlen(prefix);
    if (0 == line.compare(0, prefixLen, prefix)) {
      char* lineCopy = strdup(line.c_str() + prefixLen);
      char* start = lineCopy;
      while (char* tok = nsCRT::strtok(start, ", ", &start)) {
        for (int i = 0; i < whitelistLen; i++) {
          if (0 == strcmp(whitelist[i], tok)) {
            LoadLibraryA(tok);
            break;
          }
        }
      }
      free(lineCopy);
      break;
    }
  } while (!stream.eof());

  return true;
}
#endif

#if defined(MOZ_GMP_SANDBOX)

#if defined(XP_LINUX)
class LinuxSandboxStarter : public SandboxStarter {
public:
  LinuxSandboxStarter(const std::string& aLibPath)
    : mLibPath(aLibPath)
  {}
  virtual void Start() MOZ_OVERRIDE {
    if (mozilla::MediaPluginSandboxStatus() != mozilla::kSandboxingWouldFail) {
      mozilla::SetMediaPluginSandbox(mLibPath.c_str());
    } else {
      printf_stderr("GMPChild::LoadPluginLibrary: Loading media plugin %s unsandboxed.\n",
                    mLibPath.c_str());
    }
  }
private:
  std::string mLibPath;
};
#endif

#if defined(XP_MACOSX)
class MacOSXSandboxStarter : public SandboxStarter {
public:
  MacOSXSandboxStarter(GMPChild* aGMPChild)
    : mGMPChild(aGMPChild)
  {}
  virtual void Start() MOZ_OVERRIDE {
    mGMPChild->StartMacSandbox();
  }
private:
  GMPChild* mGMPChild;
};
#endif

#endif // MOZ_GMP_SANDBOX

bool
GMPChild::GetLibPath(nsACString& aOutLibPath)
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
  return NS_SUCCEEDED(libFile->GetNativePath(aOutLibPath));
#endif
}

bool
GMPChild::RecvStartPlugin()
{
#if defined(XP_WIN)
  PreLoadLibraries(mPluginPath);
#endif

  nsCString libPath;
  if (!GetLibPath(libPath)) {
    return false;
  }

  auto platformAPI = new GMPPlatformAPI();
  InitPlatformAPI(*platformAPI, this);

  mGMPLoader = GMPProcessChild::GetGMPLoader();
  if (!mGMPLoader) {
    NS_WARNING("Failed to get GMPLoader");
    return false;
  }

#if defined(MOZ_GMP_SANDBOX)
#if defined(XP_MACOSX)
  nsAutoPtr<SandboxStarter> starter(new MacOSXSandboxStarter(this));
  mGMPLoader->SetStartSandboxStarter(starter);
#elif defined(XP_LINUX)
  nsAutoPtr<SandboxStarter> starter(new
    LinuxSandboxStarter(std::string(libPath.get(), libPath.get()+libPath.Length())));
  mGMPLoader->SetStartSandboxStarter(starter);
#endif
#endif // MOZ_GMP_SANDBOX

  if (!mGMPLoader->Load(libPath.get(),
                        libPath.Length(),
                        &mNodeId[0],
                        mNodeId.size(),
                        platformAPI)) {
    NS_WARNING("Failed to load GMP");
    return false;
  }

  void* sh = nullptr;
  GMPAsyncShutdownHost* host = static_cast<GMPAsyncShutdownHost*>(this);
  GMPErr err = GetAPI("async-shutdown", host, &sh);
  if (err == GMPNoErr && sh) {
    mAsyncShutdown = reinterpret_cast<GMPAsyncShutdown*>(sh);
    SendAsyncShutdownRequired();
  }

  return true;
}

MessageLoop*
GMPChild::GMPMessageLoop()
{
  return mGMPMessageLoop;
}

void
GMPChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mGMPLoader) {
    mGMPLoader->Shutdown();
  }
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Abnormal shutdown of GMP process!");
    _exit(0);
  }

  XRE_ShutdownChildProcess();
}

void
GMPChild::ProcessingError(Result aWhat)
{
  switch (aWhat) {
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

PGMPAudioDecoderChild*
GMPChild::AllocPGMPAudioDecoderChild()
{
  return new GMPAudioDecoderChild(this);
}

bool
GMPChild::DeallocPGMPAudioDecoderChild(PGMPAudioDecoderChild* aActor)
{
  delete aActor;
  return true;
}

mozilla::dom::PCrashReporterChild*
GMPChild::AllocPCrashReporterChild(const NativeThreadId& aThread)
{
  return new CrashReporterChild();
}

bool
GMPChild::DeallocPCrashReporterChild(PCrashReporterChild* aCrashReporter)
{
  delete aCrashReporter;
  return true;
}

PGMPVideoDecoderChild*
GMPChild::AllocPGMPVideoDecoderChild()
{
  return new GMPVideoDecoderChild(this);
}

bool
GMPChild::DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor)
{
  delete aActor;
  return true;
}

PGMPDecryptorChild*
GMPChild::AllocPGMPDecryptorChild()
{
  GMPDecryptorChild* actor = new GMPDecryptorChild(this, mNodeId);
  actor->AddRef();
  return actor;
}

bool
GMPChild::DeallocPGMPDecryptorChild(PGMPDecryptorChild* aActor)
{
  static_cast<GMPDecryptorChild*>(aActor)->Release();
  return true;
}

bool
GMPChild::RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor)
{
  auto vdc = static_cast<GMPAudioDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = GetAPI("decode-audio", &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    return false;
  }

  vdc->Init(static_cast<GMPAudioDecoder*>(vd));

  return true;
}

PGMPVideoEncoderChild*
GMPChild::AllocPGMPVideoEncoderChild()
{
  return new GMPVideoEncoderChild(this);
}

bool
GMPChild::DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor)
{
  delete aActor;
  return true;
}

bool
GMPChild::RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor)
{
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = GetAPI("decode-video", &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    NS_WARNING("GMPGetAPI call failed trying to construct decoder.");
    return false;
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return true;
}

bool
GMPChild::RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor)
{
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = GetAPI("encode-video", &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    NS_WARNING("GMPGetAPI call failed trying to construct encoder.");
    return false;
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return true;
}

bool
GMPChild::RecvPGMPDecryptorConstructor(PGMPDecryptorChild* aActor)
{
  GMPDecryptorChild* child = static_cast<GMPDecryptorChild*>(aActor);
  GMPDecryptorHost* host = static_cast<GMPDecryptorHost*>(child);

  void* session = nullptr;
  GMPErr err = GetAPI("eme-decrypt", host, &session);
  if (err != GMPNoErr || !session) {
    return false;
  }

  child->Init(static_cast<GMPDecryptor*>(session));

  return true;
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

bool
GMPChild::RecvCrashPluginNow()
{
  MOZ_CRASH();
  return true;
}

bool
GMPChild::RecvBeginAsyncShutdown()
{
  MOZ_ASSERT(mGMPMessageLoop == MessageLoop::current());
  if (mAsyncShutdown) {
    mAsyncShutdown->BeginShutdown();
  } else {
    ShutdownComplete();
  }
  return true;
}

void
GMPChild::ShutdownComplete()
{
  MOZ_ASSERT(mGMPMessageLoop == MessageLoop::current());
  SendAsyncShutdownComplete();
}

} // namespace gmp
} // namespace mozilla
