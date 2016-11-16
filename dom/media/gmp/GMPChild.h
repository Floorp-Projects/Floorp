/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPChild_h_
#define GMPChild_h_

#include "mozilla/gmp/PGMPChild.h"
#include "GMPTimerChild.h"
#include "GMPStorageChild.h"
#include "GMPLoader.h"
#include "gmp-async-shutdown.h"
#include "gmp-entrypoints.h"
#include "prlink.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class GMPChild : public PGMPChild
               , public GMPAsyncShutdownHost
{
public:
  GMPChild();
  virtual ~GMPChild();

  bool Init(const nsAString& aPluginPath,
            const nsAString& aVoucherPath,
            base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
  MessageLoop* GMPMessageLoop();

  // Main thread only.
  GMPTimerChild* GetGMPTimers();
  GMPStorageChild* GetGMPStorage();

  // GMPAsyncShutdownHost
  void ShutdownComplete() override;

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  bool SetMacSandboxInfo(MacSandboxPluginType aPluginType);
#endif

private:
  friend class GMPContentChild;

  bool PreLoadPluginVoucher();
  void PreLoadSandboxVoucher();

  bool GetUTF8LibPath(nsACString& aOutLibPath);

  mozilla::ipc::IPCResult RecvSetNodeId(const nsCString& aNodeId) override;
  mozilla::ipc::IPCResult AnswerStartPlugin(const nsString& aAdapter) override;
  mozilla::ipc::IPCResult RecvPreloadLibs(const nsCString& aLibs) override;

  PCrashReporterChild* AllocPCrashReporterChild(const NativeThreadId& aThread) override;
  bool DeallocPCrashReporterChild(PCrashReporterChild*) override;

  PGMPTimerChild* AllocPGMPTimerChild() override;
  bool DeallocPGMPTimerChild(PGMPTimerChild* aActor) override;

  PGMPStorageChild* AllocPGMPStorageChild() override;
  bool DeallocPGMPStorageChild(PGMPStorageChild* aActor) override;

  PGMPContentChild* AllocPGMPContentChild(Transport* aTransport,
                                          ProcessId aOtherPid) override;
  void GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild);

  mozilla::ipc::IPCResult RecvCrashPluginNow() override;
  mozilla::ipc::IPCResult RecvBeginAsyncShutdown() override;
  mozilla::ipc::IPCResult RecvCloseActive() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI, uint32_t aDecryptorId = 0);

  nsTArray<UniquePtr<GMPContentChild>> mGMPContentChildren;

  GMPAsyncShutdown* mAsyncShutdown;
  RefPtr<GMPTimerChild> mTimerChild;
  RefPtr<GMPStorageChild> mStorage;

  MessageLoop* mGMPMessageLoop;
  nsString mPluginPath;
  nsString mSandboxVoucherPath;
  nsCString mNodeId;
  GMPLoader* mGMPLoader;
  nsTArray<uint8_t> mPluginVoucher;
  nsTArray<uint8_t> mSandboxVoucher;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPChild_h_
