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
#include "gmp-entrypoints.h"
#include "prlink.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class GMPChild : public PGMPChild
{
public:
  GMPChild();
  virtual ~GMPChild();

  bool Init(const nsAString& aPluginPath,
            base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);
  MessageLoop* GMPMessageLoop();

  // Main thread only.
  GMPTimerChild* GetGMPTimers();
  GMPStorageChild* GetGMPStorage();

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  bool SetMacSandboxInfo(MacSandboxPluginType aPluginType);
#endif

private:
  friend class GMPContentChild;

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

  void GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild);

  mozilla::ipc::IPCResult RecvCrashPluginNow() override;
  mozilla::ipc::IPCResult RecvCloseActive() override;

  mozilla::ipc::IPCResult RecvInitGMPContentChild(Endpoint<PGMPContentChild>&& aEndpoint) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI, uint32_t aDecryptorId = 0);

  nsTArray<UniquePtr<GMPContentChild>> mGMPContentChildren;

  RefPtr<GMPTimerChild> mTimerChild;
  RefPtr<GMPStorageChild> mStorage;

  MessageLoop* mGMPMessageLoop;
  nsString mPluginPath;
  nsCString mNodeId;
  GMPLoader* mGMPLoader;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPChild_h_
