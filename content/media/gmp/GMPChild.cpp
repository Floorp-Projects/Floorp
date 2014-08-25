/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "GMPAudioDecoderChild.h"
#include "GMPDecryptorChild.h"
#include "GMPVideoHost.h"
#include "nsIFile.h"
#include "nsXULAppAPI.h"
#include "gmp-video-decode.h"
#include "gmp-video-encode.h"
#include "GMPPlatform.h"
#include "mozilla/dom/CrashReporterChild.h"

using mozilla::dom::CrashReporterChild;

#ifdef XP_WIN
#include <stdlib.h> // for _exit()
#else
#include <unistd.h> // for _exit()
#endif

#if defined(XP_WIN)
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
  , mLib(nullptr)
  , mGetAPIFunc(nullptr)
  , mGMPMessageLoop(MessageLoop::current())
{
}

GMPChild::~GMPChild()
{
}

static bool
GetPluginFile(const std::string& aPluginPath,
#if defined(XP_MACOSX)
              nsCOMPtr<nsIFile>& aLibDirectory,
#endif
              nsCOMPtr<nsIFile>& aLibFile)
{
  nsDependentCString pluginPath(aPluginPath.c_str());

  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(pluginPath),
                                true, getter_AddRefs(aLibFile));
  if (NS_FAILED(rv)) {
    return false;
  }

#if defined(XP_MACOSX)
  if (NS_FAILED(aLibFile->Clone(getter_AddRefs(aLibDirectory)))) {
    return false;
  }
#endif

  nsCOMPtr<nsIFile> parent;
  rv = aLibFile->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString baseName(Substring(parentLeafName, 4, parentLeafName.Length() - 1));

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

void
GMPChild::OnChannelConnected(int32_t aPid)
{
  nsAutoCString pluginDirectoryPath, pluginFilePath;
  if (!GetPluginPaths(mPluginPath, pluginDirectoryPath, pluginFilePath)) {
    MOZ_CRASH("Error scanning plugin path");
  }

  MacSandboxInfo info;
  info.type = MacSandboxType_Plugin;
  info.pluginInfo.type = MacSandboxPluginType_GMPlugin_Default;
  info.pluginInfo.pluginPath.Assign(pluginDirectoryPath);
  mPluginBinaryPath.Assign(pluginFilePath);
  info.pluginInfo.pluginBinaryPath.Assign(pluginFilePath);

  nsAutoCString err;
  if (!mozilla::StartMacSandbox(info, err)) {
    NS_WARNING(err.get());
    MOZ_CRASH("sandbox_init() failed");
  }

  if (!LoadPluginLibrary(mPluginPath)) {
    err.AppendPrintf("Failed to load GMP plugin \"%s\"",
                     mPluginPath.c_str());
    NS_WARNING(err.get());
    MOZ_CRASH("Failed to load GMP plugin");
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

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  mPluginPath = aPluginPath;
  return true;
#endif

#if defined(XP_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#endif

  return LoadPluginLibrary(aPluginPath);
}

bool
GMPChild::LoadPluginLibrary(const std::string& aPluginPath)
{
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  nsAutoCString nativePath;
  nativePath.Assign(mPluginBinaryPath);

  mLib = PR_LoadLibrary(nativePath.get());
#else
  nsCOMPtr<nsIFile> libFile;
  if (!GetPluginFile(aPluginPath, libFile)) {
    return false;
  }
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
  nsAutoCString nativePath;
  libFile->GetNativePath(nativePath);

  // Enable sandboxing here -- we know the plugin file's path, but
  // this process's execution hasn't been affected by its content yet.
  MOZ_ASSERT(mozilla::CanSandboxMediaPlugin());
  mozilla::SetMediaPluginSandbox(nativePath.get());
#endif // XP_LINUX && MOZ_GMP_SANDBOX

  libFile->Load(&mLib);
#endif // XP_MACOSX && MOZ_GMP_SANDBOX

  if (!mLib) {
    return false;
  }

  GMPInitFunc initFunc = reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(mLib, "GMPInit"));
  if (!initFunc) {
    return false;
  }

  auto platformAPI = new GMPPlatformAPI();
  InitPlatformAPI(*platformAPI, this);

  if (initFunc(platformAPI) != GMPNoErr) {
    return false;
  }

  mGetAPIFunc = reinterpret_cast<GMPGetAPIFunc>(PR_FindFunctionSymbol(mLib, "GMPGetAPI"));
  if (!mGetAPIFunc) {
    return false;
  }

  void* sh = nullptr;
  GMPAsyncShutdownHost* host = static_cast<GMPAsyncShutdownHost*>(this);
  GMPErr err = mGetAPIFunc("async-shutdown", host, &sh);
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
  if (mLib) {
    GMPShutdownFunc shutdownFunc = reinterpret_cast<GMPShutdownFunc>(PR_FindFunctionSymbol(mLib, "GMPShutdown"));
    if (shutdownFunc) {
      shutdownFunc();
    }
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
  return new GMPDecryptorChild(this);
}

bool
GMPChild::DeallocPGMPDecryptorChild(PGMPDecryptorChild* aActor)
{
  delete aActor;
  return true;
}

bool
GMPChild::RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor)
{
  auto vdc = static_cast<GMPAudioDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGetAPIFunc("decode-audio", &vdc->Host(), &vd);
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
  GMPErr err = mGetAPIFunc("decode-video", &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
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
  GMPErr err = mGetAPIFunc("encode-video", &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
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
  GMPErr err = mGetAPIFunc("eme-decrypt", host, &session);
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
