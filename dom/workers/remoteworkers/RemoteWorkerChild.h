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
#include "mozilla/dom/PRemoteWorkerChild.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"

class nsISerialEventTarget;
class nsIConsoleReportCollector;

namespace mozilla::dom {

class ErrorValue;
class FetchEventOpProxyChild;
class RemoteWorkerData;
class RemoteWorkerServiceKeepAlive;
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
class RemoteWorkerChild final : public PRemoteWorkerChild {
  friend class FetchEventOpProxyChild;
  friend class PRemoteWorkerChild;
  friend class ServiceWorkerOp;

  ~RemoteWorkerChild();

 public:
  // Note that all IPC-using methods must only be invoked on the
  // RemoteWorkerService thread which the inherited
  // IProtocol::GetActorEventTarget() will return for us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteWorkerChild, final)

  explicit RemoteWorkerChild(const RemoteWorkerData& aData);

  void ExecWorker(const RemoteWorkerData& aData);

  void ErrorPropagationOnMainThread(const WorkerErrorReport* aReport,
                                    bool aIsErrorEvent);

  void CSPViolationPropagationOnMainThread(const nsAString& aJSON);

  void NotifyLock(bool aCreated);

  void NotifyWebTransport(bool aCreated);

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

  // Initial state, mWorkerPrivate is initially null but will be initialized on
  // the main thread by ExecWorkerOnMainThread when the WorkerPrivate is
  // created.  The state will transition to Running or Canceled, also from the
  // main thread.
  struct Pending : WorkerPrivateAccessibleState {
    nsTArray<RefPtr<Op>> mPendingOps;
  };

  // Running, with the state transition happening on the main thread as a result
  // of the worker successfully processing our initialization runnable,
  // indicating that top-level script execution successfully completed.  Because
  // all of our state transitions happen on the main thread and are posed in
  // terms of the main thread's perspective of the worker's state, it's very
  // possible for us to skip directly from Pending to Canceled because we decide
  // to cancel/terminate the worker prior to it finishing script loading or
  // reporting back to us.
  struct Running : WorkerPrivateAccessibleState {};

  // Cancel() has been called on the WorkerPrivate on the main thread by a
  // TerminationOp, top-level script evaluation has failed and canceled the
  // worker, or in the case of a SharedWorker, close() has been called on
  // the global scope by content code and the worker has advanced to the
  // Canceling state.  (Dedicated Workers can also self close, but they will
  // never be RemoteWorkers.  Although a SharedWorker can own DedicatedWorkers.)
  // Browser shutdown will result in a TerminationOp thanks to use of a shutdown
  // blocker in the parent, so the RuntimeService shouldn't get involved, but we
  // would also handle that case acceptably too.
  //
  // Because worker self-closing is still handled by dispatching a runnable to
  // the main thread to effectively call WorkerPrivate::Cancel(), there isn't
  // a race between a worker deciding to self-close and our termination ops.
  //
  // In this state, we have dropped the reference to the WorkerPrivate and will
  // no longer be dispatching runnables to the worker.  We wait in this state
  // until the termination lambda is invoked letting us know that the worker has
  // entirely shutdown and we can advanced to the Killed state.
  struct Canceled {};

  // The worker termination lambda has been invoked and we know the Worker is
  // entirely shutdown.  (Inherently it is possible for us to advance to this
  // state while the nsThread for the worker is still in the process of
  // shutting down, but no more worker code will run on it.)
  //
  // This name is chosen to match the Worker's own state model.
  struct Killed {};

  using State = Variant<Pending, Running, Canceled, Killed>;

  // The state of the WorkerPrivate as perceived by the owner on the main
  // thread.  All state transitions now happen on the main thread, but the
  // Worker Launcher thread will consult the state and will directly append ops
  // to the Pending queue
  DataMutex<State> mState;

  const RefPtr<RemoteWorkerServiceKeepAlive> mServiceKeepAlive;

  class Op {
   public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

    virtual ~Op() = default;

    virtual bool MaybeStart(RemoteWorkerChild* aOwner, State& aState) = 0;

    virtual void StartOnMainThread(RefPtr<RemoteWorkerChild>& aOwner) = 0;

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

  void ExceptionalErrorTransitionDuringExecWorker();

  void RequestWorkerCancellation();

  void InitializeOnWorker();

  void CreationSucceededOnAnyThread();

  void CreationFailedOnAnyThread();

  void CreationSucceededOrFailedOnAnyThread(bool aDidCreationSucceed);

  // Cancels the worker if it has been started and ensures that we transition
  // to the Terminated state once the worker has been terminated or we have
  // ensured that it will never start.
  void CloseWorkerOnMainThread();

  void ErrorPropagation(const ErrorValue& aValue);

  void ErrorPropagationDispatch(nsresult aError);

  // When the WorkerPrivate Cancellation lambda is invoked, it's possible that
  // we have not yet advanced to running from pending, so we could be in either
  // state.  This method is expected to be called by the Workers' cancellation
  // lambda and will obtain the lock and call the
  // TransitionStateFromPendingToCanceled if appropriate.  Otherwise it will
  // directly move from the running state to the canceled state which does not
  // require additional cleanup.
  void OnWorkerCancellationTransitionStateFromPendingOrRunningToCanceled();
  // A helper used by the above method by the worker cancellation lambda if the
  // the worker hasn't started running, or in exceptional cases where we bail
  // out of the ExecWorker method early.  The caller must be holding the lock
  // (in order to pass in the state).
  void TransitionStateFromPendingToCanceled(State& aState);
  void TransitionStateFromCanceledToKilled();

  void TransitionStateToRunning();

  void TransitionStateToTerminated();

  void TransitionStateToTerminated(State& aState);

  void CancelAllPendingOps(State& aState);

  void MaybeStartOp(RefPtr<Op>&& aOp);

  const bool mIsServiceWorker;

  // Touched on main-thread only.
  nsTArray<uint64_t> mWindowIDs;

  struct LauncherBoundData {
    MozPromiseHolder<GenericNonExclusivePromise> mTerminationPromise;
    // Flag to ensure we report creation at most once.  This could be cleaned up
    // further.
    bool mDidSendCreated = false;
  };

  ThreadBound<LauncherBoundData> mLauncherData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerChild_h
