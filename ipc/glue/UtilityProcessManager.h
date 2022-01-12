/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_ipc_glue_UtilityProcessManager_h_
#define _include_ipc_glue_UtilityProcessManager_h_
#include "mozilla/MozPromise.h"
#include "mozilla/ipc/UtilityProcessHost.h"
#include "nsIObserver.h"

namespace mozilla {

class MemoryReportingProcess;

namespace ipc {

class UtilityProcessParent;

// The UtilityProcessManager is a singleton responsible for creating
// Utility-bound objects that may live in another process. Currently, it
// provides access to the Utility process via ContentParent.
class UtilityProcessManager final : public UtilityProcessHost::Listener {
  friend class UtilityProcessParent;

 public:
  static void Initialize();
  static void Shutdown();
  static RefPtr<UtilityProcessManager> GetSingleton();

  // Launch a new Utility process asynchronously
  RefPtr<GenericNonExclusivePromise> LaunchProcess(SandboxingKind aSandbox);

  void OnProcessUnexpectedShutdown(UtilityProcessHost* aHost);

  // Notify the UtilityProcessManager that a top-level PUtility protocol has
  // been terminated. This may be called from any thread.
  void NotifyRemoteActorDestroyed();

  // Returns the platform pid for the Utility process.
  Maybe<base::ProcessId> ProcessPid();

  // If a Utility process is present, create a MemoryReportingProcess object.
  // Otherwise, return null.
  RefPtr<MemoryReportingProcess> GetProcessMemoryReporter();

  // Returns access to the PUtility protocol if a Utility process is present.
  UtilityProcessParent* GetProcessParent() { return mProcessParent; }

  // Returns whether or not a Utility process was ever launched.
  bool AttemptedProcess() const { return mNumProcessAttempts > 0; }

  // Returns the Utility Process
  UtilityProcessHost* Process() { return mProcess; }

  // Shutdown the Utility process.
  void CleanShutdown();

 private:
  ~UtilityProcessManager();

  bool IsProcessLaunching();
  bool IsProcessDestroyed() const;

  // Called from our xpcom-shutdown observer.
  void OnXPCOMShutdown();
  void OnPreferenceChange(const char16_t* aData);

  UtilityProcessManager();

  void DestroyProcess();

  bool IsShutdown() const;

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(RefPtr<UtilityProcessManager> aManager);

   protected:
    ~Observer() = default;

    RefPtr<UtilityProcessManager> mManager;
  };
  friend class Observer;

  RefPtr<Observer> mObserver;
  uint32_t mNumProcessAttempts = 0;
  uint32_t mNumUnexpectedCrashes = 0;

  // Fields that are associated with the current Utility process.
  UtilityProcessHost* mProcess = nullptr;
  UtilityProcessParent* mProcessParent = nullptr;
  // Collects any pref changes that occur during process launch (after
  // the initial map is passed in command-line arguments) to be sent
  // when the process can receive IPC messages.
  nsTArray<dom::Pref> mQueuedPrefs;
  // Promise will be resolved when the Utility process has been fully started
  // and VideoBridge configured. Only accessed on the main thread.
  RefPtr<GenericNonExclusivePromise> mLaunchPromise;
};

}  // namespace ipc

}  // namespace mozilla

#endif  // _include_ipc_glue_UtilityProcessManager_h_
