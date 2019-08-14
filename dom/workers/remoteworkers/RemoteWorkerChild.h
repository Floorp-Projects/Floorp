/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerChild_h
#define mozilla_dom_RemoteWorkerChild_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include "mozilla/DataMutex.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadBound.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/dom/PRemoteWorkerChild.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

class nsISerialEventTarget;
class nsIConsoleReportCollector;

namespace mozilla {
namespace dom {

class ErrorValue;
class RemoteWorkerData;
class WeakWorkerRef;
class WorkerErrorReport;
class WorkerPrivate;

class RemoteWorkerChild final
    : public SupportsThreadSafeWeakPtr<RemoteWorkerChild>,
      public PRemoteWorkerChild {
  friend class PRemoteWorkerChild;

 public:
  MOZ_DECLARE_THREADSAFEWEAKREFERENCE_TYPENAME(RemoteWorkerChild)

  MOZ_DECLARE_REFCOUNTED_TYPENAME(RemoteWorkerChild)

  explicit RemoteWorkerChild(const RemoteWorkerData& aData);

  ~RemoteWorkerChild();

  nsISerialEventTarget* GetOwningEventTarget() const;

  void ExecWorker(const RemoteWorkerData& aData);

  void ErrorPropagationOnMainThread(const WorkerErrorReport* aReport,
                                    bool aIsErrorEvent);

  void FlushReportsOnMainThread(nsIConsoleReportCollector* aReporter);

  void AddPortIdentifier(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                         const MessagePortIdentifier& aPortIdentifier);

  RefPtr<GenericNonExclusivePromise> GetTerminationPromise();

  void CloseWorkerOnMainThread();

 private:
  class InitializeWorkerRunnable;

  class Op;
  class SharedWorkerOp;

  struct Pending {
    nsTArray<RefPtr<Op>> mPendingOps;
  };

  struct PendingTerminated {};

  struct Running {
    ~Running();

    RefPtr<WorkerPrivate> mWorkerPrivate;
    RefPtr<WeakWorkerRef> mWorkerRef;
  };

  struct Terminated {};

  using State = Variant<Pending, Running, PendingTerminated, Terminated>;

  DataMutex<State> mState;

  class Op {
   public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

    virtual ~Op() = default;

    virtual bool MaybeStart(RemoteWorkerChild* aOwner, State& aState) = 0;

    virtual void Cancel() = 0;
  };

  void ActorDestroy(ActorDestroyReason) override;

  mozilla::ipc::IPCResult RecvExecOp(RemoteWorkerOp&& aOp);

  nsresult ExecWorkerOnMainThread(RemoteWorkerData&& aData);

  void InitializeOnWorker(already_AddRefed<WorkerPrivate> aWorkerPrivate);

  void ShutdownOnWorker();

  void CreationSucceededOnAnyThread();

  void CreationFailedOnAnyThread();

  void CreationSucceededOrFailedOnAnyThread(bool aDidCreationSucceed);

  void CloseWorkerOnMainThread(State& aState);

  void ErrorPropagation(const ErrorValue& aValue);

  void ErrorPropagationDispatch(nsresult aError);

  void TransitionStateToPendingTerminated(State& aState);

  void TransitionStateToRunning(already_AddRefed<WorkerPrivate> aWorkerPrivate,
                                already_AddRefed<WeakWorkerRef> aWorkerRef);

  void TransitionStateToTerminated();

  void TransitionStateToTerminated(State& aState);

  void CancelAllPendingOps(State& aState);

  void MaybeStartOp(RefPtr<Op>&& aOp);

  MozPromiseHolder<GenericNonExclusivePromise> mTerminationPromise;

  const bool mIsServiceWorker;
  const nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;

  // Touched on main-thread only.
  nsTArray<uint64_t> mWindowIDs;

  struct LauncherBoundData {
    bool mIPCActive = true;
  };

  ThreadBound<LauncherBoundData> mLauncherData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerChild_h
