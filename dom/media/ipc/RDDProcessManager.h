/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_media_ipc_RDDProcessManager_h_
#define _include_dom_media_ipc_RDDProcessManager_h_
#include "mozilla/MozPromise.h"
#include "mozilla/PRemoteDecoderManagerChild.h"
#include "mozilla/RDDProcessHost.h"
#include "mozilla/ipc/TaskFactory.h"
#include "nsIObserver.h"

namespace mozilla {

class MemoryReportingProcess;
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

  using EnsureRDDPromise =
      MozPromise<ipc::Endpoint<PRemoteDecoderManagerChild>, nsresult, true>;
  // Launch a new RDD process asynchronously
  RefPtr<GenericNonExclusivePromise> LaunchRDDProcess();
  // If not using a RDD process, launch a new RDD process asynchronously and
  // create a RemoteDecoderManager bridge
  RefPtr<EnsureRDDPromise> EnsureRDDProcessAndCreateBridge(
      base::ProcessId aOtherProcess);

  void OnProcessUnexpectedShutdown(RDDProcessHost* aHost) override;

  // Notify the RDDProcessManager that a top-level PRDD protocol has been
  // terminated. This may be called from any thread.
  void NotifyRemoteActorDestroyed(const uint64_t& aProcessToken);

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
  bool IsRDDProcessLaunching();
  bool IsRDDProcessDestroyed() const;
  bool CreateVideoBridge();

  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();
  void OnPreferenceChange(const char16_t* aData);

  RDDProcessManager();

  // Shutdown the RDD process.
  void CleanShutdown();
  void DestroyProcess();

  bool IsShutdown() const;

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

  bool CreateContentBridge(
      base::ProcessId aOtherProcess,
      ipc::Endpoint<PRemoteDecoderManagerChild>* aOutRemoteDecoderManager);

  const RefPtr<Observer> mObserver;
  ipc::TaskFactory<RDDProcessManager> mTaskFactory;
  uint32_t mNumProcessAttempts = 0;
  uint32_t mNumUnexpectedCrashes = 0;

  // Fields that are associated with the current RDD process.
  RDDProcessHost* mProcess = nullptr;
  uint64_t mProcessToken = 0;
  RDDChild* mRDDChild = nullptr;
  // Collects any pref changes that occur during process launch (after
  // the initial map is passed in command-line arguments) to be sent
  // when the process can receive IPC messages.
  nsTArray<dom::Pref> mQueuedPrefs;
  // Promise will be resolved when the RDD process has been fully started and
  // VideoBridge configured. Only accessed on the main thread.
  RefPtr<GenericNonExclusivePromise> mLaunchRDDPromise;
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDProcessManager_h_
