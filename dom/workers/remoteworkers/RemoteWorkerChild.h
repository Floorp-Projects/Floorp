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
class FetchEventOpProxyChild;
class RemoteWorkerData;
class ServiceWorkerOp;
class UniqueMessagePortId;
class WeakWorkerRef;
class WorkerErrorReport;
class WorkerPrivate;

/**
 * Background-managed "Worker Launcher"-thread-resident created via the
 * RemoteWorkerManager to actually spawn the worker. Currently, the worker will
 * be spawned from the main thread due to nsIPrincipal not being able to be
 * created on background threads and other ownership invariants, most of which
 * can be relaxed in the future.
 */
class RemoteWorkerChild final
    : public SupportsThreadSafeWeakPtr<RemoteWorkerChild>,
      public PRemoteWorkerChild {
  friend class FetchEventOpProxyChild;
  friend class PRemoteWorkerChild;
  friend class ServiceWorkerOp;

 public:
  MOZ_DECLARE_THREADSAFEWEAKREFERENCE_TYPENAME(RemoteWorkerChild)

  MOZ_DECLARE_REFCOUNTED_TYPENAME(RemoteWorkerChild)

  explicit RemoteWorkerChild(const RemoteWorkerData& aData);

  ~RemoteWorkerChild();

  nsISerialEventTarget* GetOwningEventTarget() const;

  void ExecWorker(const RemoteWorkerData& aData);

  void ErrorPropagationOnMainThread(const WorkerErrorReport* aReport,
                                    bool aIsErrorEvent);

  void NotifyLock(bool aCreated);

  void FlushReportsOnMainThread(nsIConsoleReportCollector* aReporter);

  void AddPortIdentifier(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                         UniqueMessagePortId& aPortIdentifier);

  RefPtr<GenericNonExclusivePromise> GetTerminationPromise();

  RefPtr<GenericPromise> MaybeSendSetServiceWorkerSkipWaitingFlag();

  const nsTArray<uint64_t>& WindowIDs() const { return mWindowIDs; }

 private:
  class InitializeWorkerRunnable;

  class Op;
  class SharedWorkerOp;

  struct WorkerPrivateAccessibleState {
    ~WorkerPrivateAccessibleState();
    RefPtr<WorkerPrivate> mWorkerPrivate;
  };

  struct Pending : WorkerPrivateAccessibleState {
    nsTArray<RefPtr<Op>> mPendingOps;
  };

  struct PendingTerminated {};

  struct Running : WorkerPrivateAccessibleState {
    ~Running();
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

  mozilla::ipc::IPCResult RecvExecServiceWorkerOp(
      ServiceWorkerOpArgs&& aArgs, ExecServiceWorkerOpResolver&& aResolve);

  already_AddRefed<PFetchEventOpProxyChild> AllocPFetchEventOpProxyChild(
      const ParentToChildServiceWorkerFetchEventOpArgs& aArgs);

  mozilla::ipc::IPCResult RecvPFetchEventOpProxyConstructor(
      PFetchEventOpProxyChild* aActor,
      const ParentToChildServiceWorkerFetchEventOpArgs& aArgs) override;

  nsresult ExecWorkerOnMainThread(RemoteWorkerData&& aData);

  void InitializeOnWorker();

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

  const bool mIsServiceWorker;
  const nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;

  // Touched on main-thread only.
  nsTArray<uint64_t> mWindowIDs;

  struct LauncherBoundData {
    bool mIPCActive = true;
    MozPromiseHolder<GenericNonExclusivePromise> mTerminationPromise;
  };

  ThreadBound<LauncherBoundData> mLauncherData;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerChild_h
