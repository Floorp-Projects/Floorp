/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerrunnable_h__
#define mozilla_dom_workers_workerrunnable_h__

#include "Workers.h"

#include "nsICancelableRunnable.h"

#include "mozilla/Atomics.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h" /* nsRunnable */

struct JSContext;
class nsIEventTarget;

namespace mozilla {
class ErrorResult;
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

// Use this runnable to communicate from the worker to its parent or vice-versa.
// The busy count must be taken into consideration and declared at construction
// time.
class WorkerRunnable : public nsIRunnable,
                       public nsICancelableRunnable
{
public:
  enum TargetAndBusyBehavior {
    // Target the main thread for top-level workers, otherwise target the
    // WorkerThread of the worker's parent. No change to the busy count.
    ParentThreadUnchangedBusyCount,

    // Target the thread where the worker event loop runs. The busy count will
    // be incremented before dispatching and decremented (asynchronously) after
    // running.
    WorkerThreadModifyBusyCount,

    // Target the thread where the worker event loop runs. The busy count will
    // not be modified in any way. Besides worker-internal runnables this is
    // almost always the wrong choice.
    WorkerThreadUnchangedBusyCount
  };

protected:
  // The WorkerPrivate that this runnable is associated with.
  WorkerPrivate* mWorkerPrivate;

  // See above.
  TargetAndBusyBehavior mBehavior;

  // It's unclear whether or not Cancel() is supposed to work when called on any
  // thread. To be safe we're using an atomic but it's likely overkill.
  Atomic<uint32_t> mCanceled;

private:
  // Whether or not Cancel() is currently being called from inside the Run()
  // method. Avoids infinite recursion when a subclass calls Run() from inside
  // Cancel(). Only checked and modified on the target thread.
  bool mCallingCancelWithinRun;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // If you override Cancel() then you'll need to either call the base class
  // Cancel() method or override IsCanceled() so that the Run() method bails out
  // appropriately.
  nsresult
  Cancel() override;

  // The return value is true if and only if both PreDispatch and
  // DispatchInternal return true.
  bool
  Dispatch();

  // See above note about Cancel().
  virtual bool
  IsCanceled() const
  {
    return mCanceled != 0;
  }

  static WorkerRunnable*
  FromRunnable(nsIRunnable* aRunnable);

protected:
  WorkerRunnable(WorkerPrivate* aWorkerPrivate,
                 TargetAndBusyBehavior aBehavior = WorkerThreadModifyBusyCount)
#ifdef DEBUG
  ;
#else
  : mWorkerPrivate(aWorkerPrivate), mBehavior(aBehavior), mCanceled(0),
    mCallingCancelWithinRun(false)
  { }
#endif

  // This class is reference counted.
  virtual ~WorkerRunnable()
  { }

  // Returns true if this runnable should be dispatched to the debugger queue,
  // and false otherwise.
  virtual bool
  IsDebuggerRunnable() const;

  nsIGlobalObject*
  DefaultGlobalObject() const;

  // By default asserts that Dispatch() is being called on the right thread
  // (ParentThread if |mTarget| is WorkerThread, or WorkerThread otherwise).
  // Also increments the busy count of |mWorkerPrivate| if targeting the
  // WorkerThread.
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate);

  // By default asserts that Dispatch() is being called on the right thread
  // (ParentThread if |mTarget| is WorkerThread, or WorkerThread otherwise).
  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult);

  // May be implemented by subclasses if desired if they need to do some sort of
  // setup before we try to set up our JSContext and compartment for real.
  // Typically the only thing that should go in here is creation of the worker's
  // global.
  //
  // If false is returned, WorkerRun will not be called at all.  PostRun will
  // still be called, with false passed for aRunResult.
  virtual bool
  PreRun(WorkerPrivate* aWorkerPrivate);

  // Must be implemented by subclasses. Called on the target thread.  The return
  // value will be passed to PostRun().  The JSContext passed in here comes from
  // an AutoJSAPI (or AutoEntryScript) that we set up on the stack.  If
  // mBehavior is ParentThreadUnchangedBusyCount, it is in the compartment of
  // mWorkerPrivate's reflector (i.e. the worker object in the parent thread),
  // unless that reflector is null, in which case it's in the compartment of the
  // parent global (which is the compartment reflector would have been in), or
  // in the null compartment if there is no parent global.  For other mBehavior
  // values, we're running on the worker thread and aCx is in whatever
  // compartment GetCurrentThreadJSContext() was in when nsIRunnable::Run() got
  // called.  This is actually important for cases when a runnable spins a
  // syncloop and wants everything that happens during the syncloop to happen in
  // the compartment that runnable set up (which may, for example, be a debugger
  // sandbox compartment!).  If aCx wasn't in a compartment to start with, aCx
  // will be in either the debugger global's compartment or the worker's
  // global's compartment depending on whether IsDebuggerRunnable() is true.
  //
  // Immediately after WorkerRun returns, the caller will assert that either it
  // returns false or there is no exception pending on aCx.  Then it will report
  // any pending exceptions on aCx.
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) = 0;

  // By default asserts that Run() (and WorkerRun()) were called on the correct
  // thread.  Also sends an asynchronous message to the ParentThread if the
  // busy count was previously modified in PreDispatch().
  //
  // The aCx passed here is the same one as was passed to WorkerRun and is
  // still in the same compartment.  PostRun implementations must NOT leave an
  // exception on the JSContext and must not run script, because the incoming
  // JSContext may be in the null compartment.
  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult);

  virtual bool
  DispatchInternal();

  // Calling Run() directly is not supported. Just call Dispatch() and
  // WorkerRun() will be called on the correct thread automatically.
  NS_DECL_NSIRUNNABLE
};

// This runnable is used to send a message to a worker debugger.
class WorkerDebuggerRunnable : public WorkerRunnable
{
protected:
  explicit WorkerDebuggerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {
  }

  virtual ~WorkerDebuggerRunnable()
  { }

private:
  virtual bool
  IsDebuggerRunnable() const override
  {
    return true;
  }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override final
  {
    AssertIsOnMainThread();

    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override;
};

// This runnable is used to send a message directly to a worker's sync loop.
class WorkerSyncRunnable : public WorkerRunnable
{
protected:
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;

  // Passing null for aSyncLoopTarget is allowed and will result in the behavior
  // of a normal WorkerRunnable.
  WorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                     nsIEventTarget* aSyncLoopTarget);

  WorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                     already_AddRefed<nsIEventTarget>&& aSyncLoopTarget);

  virtual ~WorkerSyncRunnable();

  virtual bool
  DispatchInternal() override;
};

// This runnable is identical to WorkerSyncRunnable except it is meant to be
// created on and dispatched from the main thread only.  Its WorkerRun/PostRun
// will run on the worker thread.
class MainThreadWorkerSyncRunnable : public WorkerSyncRunnable
{
protected:
  // Passing null for aSyncLoopTarget is allowed and will result in the behavior
  // of a normal WorkerRunnable.
  MainThreadWorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                               nsIEventTarget* aSyncLoopTarget)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncLoopTarget)
  {
    AssertIsOnMainThread();
  }

  MainThreadWorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                               already_AddRefed<nsIEventTarget>&& aSyncLoopTarget)
  : WorkerSyncRunnable(aWorkerPrivate, Move(aSyncLoopTarget))
  {
    AssertIsOnMainThread();
  }

  virtual ~MainThreadWorkerSyncRunnable()
  { }

private:
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override;
};

// This runnable is processed as soon as it is received by the worker,
// potentially running before previously queued runnables and perhaps even with
// other JS code executing on the stack. These runnables must not alter the
// state of the JS runtime and should only twiddle state values. The busy count
// is never modified.
class WorkerControlRunnable : public WorkerRunnable
{
  friend class WorkerPrivate;

protected:
  WorkerControlRunnable(WorkerPrivate* aWorkerPrivate,
                        TargetAndBusyBehavior aBehavior = WorkerThreadModifyBusyCount)
#ifdef DEBUG
  ;
#else
  : WorkerRunnable(aWorkerPrivate, aBehavior)
  { }
#endif

  virtual ~WorkerControlRunnable()
  { }

  nsresult
  Cancel() override;

public:
  NS_DECL_ISUPPORTS_INHERITED

private:
  virtual bool
  DispatchInternal() override;

  // Should only be called by WorkerPrivate::DoRunLoop.
  using WorkerRunnable::Cancel;
};

// A convenience class for WorkerRunnables that are originated on the main
// thread.
class MainThreadWorkerRunnable : public WorkerRunnable
{
protected:
  explicit MainThreadWorkerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {
    AssertIsOnMainThread();
  }

  virtual ~MainThreadWorkerRunnable()
  {}

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    AssertIsOnMainThread();
  }
};

// A convenience class for WorkerControlRunnables that originate on the main
// thread.
class MainThreadWorkerControlRunnable : public WorkerControlRunnable
{
protected:
  explicit MainThreadWorkerControlRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

  virtual ~MainThreadWorkerControlRunnable()
  { }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    AssertIsOnMainThread();
  }
};

// A WorkerRunnable that should be dispatched from the worker to itself for
// async tasks. This will increment the busy count PostDispatch() (only if
// dispatch was successful) and decrement it in PostRun().
//
// Async tasks will almost always want to use this since
// a WorkerSameThreadRunnable keeps the Worker from being GCed.
class WorkerSameThreadRunnable : public WorkerRunnable
{
protected:
  explicit WorkerSameThreadRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
  { }

  virtual ~WorkerSameThreadRunnable()
  { }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override;

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override;

  // We just delegate PostRun to WorkerRunnable, since it does exactly
  // what we want.
};

// Base class for the runnable objects, which makes a synchronous call to
// dispatch the tasks from the worker thread to the main thread.
//
// Note that the derived class must override MainThreadRun.
class WorkerMainThreadRunnable : public Runnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  const nsCString mTelemetryKey;

  explicit WorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                    const nsACString& aTelemetryKey);
  ~WorkerMainThreadRunnable() {}

  virtual bool MainThreadRun() = 0;

public:
  // Dispatch the runnable to the main thread.  If dispatch to main thread
  // fails, or if the worker is shut down while dispatching, an error will be
  // reported on aRv.  In that case the error MUST be propagated out to script.
  void Dispatch(ErrorResult& aRv);

private:
  NS_IMETHOD Run() override;
};

// This runnable is an helper class for dispatching something from a worker
// thread to the main-thread and back to the worker-thread. During this
// operation, this class will keep the worker alive.
class WorkerProxyToMainThreadRunnable : public Runnable
{
protected:
  explicit WorkerProxyToMainThreadRunnable(WorkerPrivate* aWorkerPrivate);

  virtual ~WorkerProxyToMainThreadRunnable();

  // First this method is called on the main-thread.
  virtual void RunOnMainThread() = 0;

  // After this second method is called on the worker-thread.
  virtual void RunBackOnWorkerThread() = 0;

public:
  bool Dispatch();

private:
  NS_IMETHOD Run() override;

  void PostDispatchOnMainThread();

  bool HoldWorker();
  void ReleaseWorker();

protected:
  WorkerPrivate* mWorkerPrivate;
  UniquePtr<WorkerHolder> mWorkerHolder;
};

// Class for checking API exposure.  This totally violates the "MUST" in the
// comments on WorkerMainThreadRunnable::Dispatch, because API exposure checks
// can't throw.  Maybe we should change it so they _could_ throw.  But for now
// we are bad people and should be ashamed of ourselves.  Let's hope none of
// them happen while a worker is shutting down.
//
// Do NOT copy what this class is doing elsewhere.  Just don't.
class WorkerCheckAPIExposureOnMainThreadRunnable
  : public WorkerMainThreadRunnable
{
public:
  explicit
  WorkerCheckAPIExposureOnMainThreadRunnable(WorkerPrivate* aWorkerPrivate);
  virtual
  ~WorkerCheckAPIExposureOnMainThreadRunnable();

  // Returns whether the dispatch succeeded.  If this returns false, the API
  // should not be exposed.
  bool Dispatch();
};

// This runnable is used to stop a sync loop and it's meant to be used on the
// main-thread only. As sync loops keep the busy count incremented as long as
// they run this runnable does not modify the busy count
// in any way.
class MainThreadStopSyncLoopRunnable : public WorkerSyncRunnable
{
  bool mResult;

public:
  // Passing null for aSyncLoopTarget is not allowed.
  MainThreadStopSyncLoopRunnable(
                               WorkerPrivate* aWorkerPrivate,
                               already_AddRefed<nsIEventTarget>&& aSyncLoopTarget,
                               bool aResult);

  // By default StopSyncLoopRunnables cannot be canceled since they could leave
  // a sync loop spinning forever.
  nsresult
  Cancel() override;

protected:
  virtual ~MainThreadStopSyncLoopRunnable()
  { }

private:
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override final
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override;

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  virtual bool
  DispatchInternal() override final;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerrunnable_h__
