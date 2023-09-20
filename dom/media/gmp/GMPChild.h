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

class ChildProfilerController;

namespace gmp {

class GMPContentChild;

class GMPChild : public PGMPChild {
  friend class PGMPChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(GMPChild, override)

  GMPChild();

  bool Init(const nsAString& aPluginPath, const char* aParentBuildID,
            mozilla::ipc::UntypedEndpoint&& aEndpoint);
  void Shutdown();
  MessageLoop* GMPMessageLoop();

  // Main thread only.
  GMPTimerChild* GetGMPTimers();
  GMPStorageChild* GetGMPStorage();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  bool SetMacSandboxInfo(bool aAllowWindowServer);
#endif

 private:
  friend class GMPContentChild;

  virtual ~GMPChild();

  bool GetUTF8LibPath(nsACString& aOutLibPath);

  bool GetPluginName(nsACString& aPluginName) const;

  mozilla::ipc::IPCResult RecvProvideStorageId(const nsCString& aStorageId);

  mozilla::ipc::IPCResult RecvStartPlugin(const nsString& aAdapter);
  mozilla::ipc::IPCResult RecvPreloadLibs(const nsCString& aLibs);

  PGMPTimerChild* AllocPGMPTimerChild();
  bool DeallocPGMPTimerChild(PGMPTimerChild* aActor);

  PGMPStorageChild* AllocPGMPStorageChild();
  bool DeallocPGMPStorageChild(PGMPStorageChild* aActor);

  void GMPContentChildActorDestroy(GMPContentChild* aGMPContentChild);

  mozilla::ipc::IPCResult RecvCrashPluginNow();
  mozilla::ipc::IPCResult RecvCloseActive();

  mozilla::ipc::IPCResult RecvInitGMPContentChild(
      Endpoint<PGMPContentChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvFlushFOGData(FlushFOGDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvTestTriggerMetrics(
      TestTriggerMetricsResolver&& aResolve);

  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<mozilla::PProfilerChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref);

  mozilla::ipc::IPCResult RecvShutdown(ShutdownResolver&& aResolver);

#if defined(XP_WIN)
  mozilla::ipc::IPCResult RecvInitDllServices(
      const bool& aCanRecordReleaseTelemetry,
      const bool& aIsReadyForBackgroundProcessing);

  mozilla::ipc::IPCResult RecvGetUntrustedModulesData(
      GetUntrustedModulesDataResolver&& aResolver);
  mozilla::ipc::IPCResult RecvUnblockUntrustedModulesThread();
#endif  // defined(XP_WIN)

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI,
                const nsACString& aKeySystem = ""_ns);

  nsTArray<std::pair<nsCString, nsCString>> MakeCDMHostVerificationPaths(
      const nsACString& aPluginLibPath);

  nsTArray<RefPtr<GMPContentChild>> mGMPContentChildren;

  RefPtr<GMPTimerChild> mTimerChild;
  RefPtr<GMPStorageChild> mStorage;

  RefPtr<ChildProfilerController> mProfilerController;

  MessageLoop* mGMPMessageLoop;
  nsString mPluginPath;
  nsCString mStorageId;
  UniquePtr<GMPLoader> mGMPLoader;
#ifdef XP_LINUX
  nsTArray<void*> mLibHandles;
#endif
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPChild_h_
