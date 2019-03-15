/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerChild_h
#define mozilla_dom_RemoteWorkerChild_h

#include "mozilla/dom/PRemoteWorkerChild.h"
#include "mozilla/DataMutex.h"
#include "mozilla/ThreadBound.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"

class nsIConsoleReportCollector;

namespace mozilla {
namespace dom {

class RemoteWorkerData;
class WeakWorkerRef;
class WorkerErrorReport;
class WorkerPrivate;
class OptionalMessagePortIdentifier;

class RemoteWorkerChild final : public PRemoteWorkerChild {
  friend class PRemoteWorkerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteWorkerChild)

  RemoteWorkerChild();

  void ExecWorker(const RemoteWorkerData& aData);

  void InitializeOnWorker(WorkerPrivate* aWorkerPrivate);

  void ShutdownOnWorker();

  void AddPortIdentifier(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                         const MessagePortIdentifier& aPortIdentifier);

  void ErrorPropagationOnMainThread(const WorkerErrorReport* aReport,
                                    bool aIsErrorEvent);

  void CloseWorkerOnMainThread();

  void FlushReportsOnMainThread(nsIConsoleReportCollector* aReporter);

 private:
  class InitializeWorkerRunnable;

  ~RemoteWorkerChild();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvExecOp(const RemoteWorkerOp& aOp);

  // This member is a function template because DataMutex<SharedData>::AutoLock
  // is private, yet it must be passed by const reference into ExecuteOperation.
  // There should only be one instantiation of this template.
  template <typename T>
  mozilla::ipc::IPCResult ExecuteOperation(const RemoteWorkerOp&,
                                           const T& aLock);

  void RecvExecOpOnMainThread(const RemoteWorkerOp& aOp);

  nsresult ExecWorkerOnMainThread(const RemoteWorkerData& aData);

  void ErrorPropagation(const ErrorValue& aValue);

  void ErrorPropagationDispatch(nsresult aError);

  void CreationSucceededOnAnyThread();

  void CreationSucceeded();

  void CreationFailedOnAnyThread();

  void CreationFailed();

  void WorkerTerminated();

  // Touched on main-thread only.
  nsTArray<uint64_t> mWindowIDs;

  RefPtr<WeakWorkerRef> mWorkerRef;
  bool mIPCActive;

  enum WorkerState {
    // CreationSucceeded/CreationFailed not called yet.
    ePending,

    // The worker is not created yet, but we want to terminate as soon as
    // possible.
    ePendingTerminated,

    // Worker up and running.
    eRunning,

    // Worker terminated.
    eTerminated,
  };

  struct SharedData {
    SharedData();

    RefPtr<WorkerPrivate> mWorkerPrivate;
    WorkerState mWorkerState;
  };

  DataMutex<SharedData> mSharedData;

  // Touched only on the owning thread (Worker Launcher).
  struct LauncherBoundData {
    nsTArray<RemoteWorkerOp> mPendingOps;
  };

  ThreadBound<LauncherBoundData> mLauncherData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerChild_h
