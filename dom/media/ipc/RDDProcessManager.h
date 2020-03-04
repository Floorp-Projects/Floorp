/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDProcessManager_h_
#define _include_dom_media_ipc_RDDProcessManager_h_
#include "mozilla/RDDProcessHost.h"

#include "mozilla/ipc/TaskFactory.h"

namespace mozilla {

class MemoryReportingProcess;
class PRemoteDecoderManagerChild;
class RDDChild;

// The RDDProcessManager is a singleton responsible for creating RDD-bound
// objects that may live in another process. Currently, it provides access
// to the RDD process via ContentParent.
class RDDProcessManager final : public RDDProcessHost::Listener {
  friend class RDDChild;

 public:
  static void Initialize();
  static void Shutdown();
  static RDDProcessManager* Get();

  ~RDDProcessManager();

  // If not using a RDD process, launch a new RDD process asynchronously.
  bool LaunchRDDProcess();
  bool IsRDDProcessLaunching();

  // Ensure that RDD-bound methods can be used. If no RDD process is being
  // used, or one is launched and ready, this function returns immediately.
  // Otherwise it blocks until the RDD process has finished launching.
  bool EnsureRDDReady();

  bool CreateContentBridge(base::ProcessId aOtherProcess,
                           mozilla::ipc::Endpoint<PRemoteDecoderManagerChild>*
                               aOutRemoteDecoderManager);

  void OnProcessLaunchComplete(RDDProcessHost* aHost) override;
  void OnProcessUnexpectedShutdown(RDDProcessHost* aHost) override;

  // Notify the RDDProcessManager that a top-level PRDD protocol has been
  // terminated. This may be called from any thread.
  void NotifyRemoteActorDestroyed(const uint64_t& aProcessToken);

  // Used for tests and diagnostics
  void KillProcess();

  // Returns -1 if there is no RDD process, or the platform pid for it.
  base::ProcessId RDDProcessPid();

  // If a RDD process is present, create a MemoryReportingProcess object.
  // Otherwise, return null.
  RefPtr<MemoryReportingProcess> GetProcessMemoryReporter();

  // Returns access to the PRDD protocol if a RDD process is present.
  RDDChild* GetRDDChild() { return mRDDChild; }

  // Returns whether or not a RDD process was ever launched.
  bool AttemptedRDDProcess() const { return mNumProcessAttempts > 0; }

  // Returns the RDD Process
  RDDProcessHost* Process() { return mProcess; }

 private:
  bool CreateVideoBridge();

  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();
  void OnPreferenceChange(const char16_t* aData);

  RDDProcessManager();

  // Shutdown the RDD process.
  void CleanShutdown();
  void DestroyProcess();

  DISALLOW_COPY_AND_ASSIGN(RDDProcessManager);

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(RDDProcessManager* aManager);

   protected:
    ~Observer() = default;

    RDDProcessManager* mManager;
  };
  friend class Observer;

 private:
  RefPtr<Observer> mObserver;
  mozilla::ipc::TaskFactory<RDDProcessManager> mTaskFactory;
  uint32_t mNumProcessAttempts;

  // Fields that are associated with the current RDD process.
  RDDProcessHost* mProcess;
  uint64_t mProcessToken;
  RDDChild* mRDDChild;
  // Collects any pref changes that occur during process launch (after
  // the initial map is passed in command-line arguments) to be sent
  // when the process can receive IPC messages.
  nsTArray<mozilla::dom::Pref> mQueuedPrefs;
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDProcessManager_h_
