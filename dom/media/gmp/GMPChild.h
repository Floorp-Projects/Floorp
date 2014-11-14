/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPChild_h_
#define GMPChild_h_

#include "mozilla/gmp/PGMPChild.h"
#include "GMPSharedMemManager.h"
#include "GMPTimerChild.h"
#include "GMPStorageChild.h"
#include "GMPLoader.h"
#include "gmp-async-shutdown.h"
#include "gmp-entrypoints.h"
#include "prlink.h"

namespace mozilla {
namespace gmp {

class GMPChild : public PGMPChild
               , public GMPSharedMem
               , public GMPAsyncShutdownHost
{
public:
  GMPChild();
  virtual ~GMPChild();

  bool Init(const std::string& aPluginPath,
            base::ProcessHandle aParentProcessHandle,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
#ifdef XP_WIN
  bool PreLoadLibraries(const std::string& aPluginPath);
#endif
  MessageLoop* GMPMessageLoop();

  // Main thread only.
  GMPTimerChild* GetGMPTimers();
  GMPStorageChild* GetGMPStorage();

  // GMPSharedMem
  virtual void CheckThread() MOZ_OVERRIDE;

  // GMPAsyncShutdownHost
  void ShutdownComplete() MOZ_OVERRIDE;

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  void StartMacSandbox();
#endif

private:

  bool GetLibPath(nsACString& aOutLibPath);

  virtual bool RecvSetNodeId(const nsCString& aNodeId) MOZ_OVERRIDE;
  virtual bool RecvStartPlugin() MOZ_OVERRIDE;

  virtual PCrashReporterChild* AllocPCrashReporterChild(const NativeThreadId& aThread) MOZ_OVERRIDE;
  virtual bool DeallocPCrashReporterChild(PCrashReporterChild*) MOZ_OVERRIDE;

  virtual PGMPVideoDecoderChild* AllocPGMPVideoDecoderChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor) MOZ_OVERRIDE;
  virtual bool RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor) MOZ_OVERRIDE;

  virtual PGMPVideoEncoderChild* AllocPGMPVideoEncoderChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor) MOZ_OVERRIDE;
  virtual bool RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor) MOZ_OVERRIDE;

  virtual PGMPDecryptorChild* AllocPGMPDecryptorChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPDecryptorChild(PGMPDecryptorChild* aActor) MOZ_OVERRIDE;
  virtual bool RecvPGMPDecryptorConstructor(PGMPDecryptorChild* aActor) MOZ_OVERRIDE;

  virtual PGMPAudioDecoderChild* AllocPGMPAudioDecoderChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPAudioDecoderChild(PGMPAudioDecoderChild* aActor) MOZ_OVERRIDE;
  virtual bool RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor) MOZ_OVERRIDE;

  virtual PGMPTimerChild* AllocPGMPTimerChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPTimerChild(PGMPTimerChild* aActor) MOZ_OVERRIDE;

  virtual PGMPStorageChild* AllocPGMPStorageChild() MOZ_OVERRIDE;
  virtual bool DeallocPGMPStorageChild(PGMPStorageChild* aActor) MOZ_OVERRIDE;

  virtual bool RecvCrashPluginNow() MOZ_OVERRIDE;
  virtual bool RecvBeginAsyncShutdown() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual void ProcessingError(Result aWhat) MOZ_OVERRIDE;

  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI);

  GMPAsyncShutdown* mAsyncShutdown;
  nsRefPtr<GMPTimerChild> mTimerChild;
  nsRefPtr<GMPStorageChild> mStorage;

  MessageLoop* mGMPMessageLoop;
  std::string mPluginPath;
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  nsCString mPluginBinaryPath;
#endif
  std::string mNodeId;
  GMPLoader* mGMPLoader;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPChild_h_
