/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerrunnable_h__
#define mozilla_dom_workers_workerrunnable_h__

#include "Workers.h"

#include "nsICancelableRunnable.h"

#include "mozilla/Atomics.h"
#include "nsISupportsImpl.h"

class JSContext;
class nsIEventTarget;

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

// Use this runnable to communicate from the worker to its parent or vice-versa.
// The busy count must be taken into consideration and declared at construction
// time.
class WorkerRunnable : public nsICancelableRunnable
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
  NS_DECL_NSICANCELABLERUNNABLE

  // Passing a JSContext here is required for the WorkerThreadModifyBusyCount
  // behavior. It also guarantees that any failure (false return) will throw an
  // exception on the given context. If a context is not passed then failures
  // must be dealt with by the caller.
  bool
  Dispatch(JSContext* aCx);

  // See above note about Cancel().
  virtual bool
  IsCanceled() const
  {
    return mCanceled != 0;
  }

  static WorkerRunnable*
  FromRunnable(nsIRunnable* aRunnable);

protected:
  WorkerRunnable(WorkerPrivate* aWorkerPrivate, TargetAndBusyBehavior aBehavior)
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

  // By default asserts that Dispatch() is being called on the right thread
  // (ParentThread if |mTarget| is WorkerThread, or WorkerThread otherwise).
  // Also increments the busy count of |mWorkerPrivate| if targeting the
  // WorkerThread.
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  // By default asserts that Dispatch() is being called on the right thread
  // (ParentThread if |mTarget| is WorkerThread, or WorkerThread otherwise).
  // Also reports any Dispatch() failures as an exception on |aCx|, and
  // busy count if targeting the WorkerThread and Dispatch() failed.
  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult);

  // Must be implemented by subclasses. Called on the target thread.
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) = 0;

  // By default asserts that Run() (and WorkerRun()) were called on the correct
  // thread. Any failures (false return from WorkerRun) are reported on |aCx|.
  // Also sends an asynchronous message to the ParentThread if the busy
  // count was previously modified in PreDispatch().
  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult);

  virtual bool
  DispatchInternal();

  // Calling Run() directly is not supported. Just call Dispatch() and
  // WorkerRun() will be called on the correct thread automatically.
  NS_DECL_NSIRUNNABLE
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

private:
  virtual bool
  DispatchInternal() MOZ_OVERRIDE;
};

// This runnable is identical to WorkerSyncRunnable except it is meant to be
// used on the main thread only.
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
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE;
};

// This runnable is used to stop a sync loop . As sync loops keep the busy count
// incremented as long as they run this runnable does not modify the busy count
// in any way.
class StopSyncLoopRunnable : public WorkerSyncRunnable
{
  bool mResult;

public:
  // Passing null for aSyncLoopTarget is not allowed.
  StopSyncLoopRunnable(WorkerPrivate* aWorkerPrivate,
                       already_AddRefed<nsIEventTarget>&& aSyncLoopTarget,
                       bool aResult);

  // By default StopSyncLoopRunnables cannot be canceled since they could leave
  // a sync loop spinning forever.
  NS_DECL_NSICANCELABLERUNNABLE

protected:
  virtual ~StopSyncLoopRunnable()
  { }

  // Called on the worker thread to set an exception on the context if mResult
  // is false. Override if you need an exception.
  virtual void
  MaybeSetException(JSContext* aCx)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE;

  virtual bool
  DispatchInternal() MOZ_OVERRIDE;
};

// This runnable is identical to StopSyncLoopRunnable except it is meant to be
// used on the main thread only.
class MainThreadStopSyncLoopRunnable : public StopSyncLoopRunnable
{
public:
  // Passing null for aSyncLoopTarget is not allowed.
  MainThreadStopSyncLoopRunnable(
                               WorkerPrivate* aWorkerPrivate,
                               already_AddRefed<nsIEventTarget>&& aSyncLoopTarget,
                               bool aResult)
  : StopSyncLoopRunnable(aWorkerPrivate, Move(aSyncLoopTarget), aResult)
  {
    AssertIsOnMainThread();
  }

protected:
  virtual ~MainThreadStopSyncLoopRunnable()
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE;
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
                        TargetAndBusyBehavior aBehavior)
#ifdef DEBUG
  ;
#else
  : WorkerRunnable(aWorkerPrivate, aBehavior)
  { }
#endif

  virtual ~WorkerControlRunnable()
  { }

public:
  NS_DECL_ISUPPORTS_INHERITED

private:
  virtual bool
  DispatchInternal() MOZ_OVERRIDE;

  // Should only be called by WorkerPrivate::DoRunLoop.
  using WorkerRunnable::Cancel;
};

// A convenience class for WorkerControlRunnables that originate on the main
// thread.
class MainThreadWorkerControlRunnable : public WorkerControlRunnable
{
protected:
  MainThreadWorkerControlRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

  virtual ~MainThreadWorkerControlRunnable()
  { }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE;
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
  WorkerSameThreadRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
  { }

  virtual ~WorkerSameThreadRunnable()
  { }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE;

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE;

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
          bool aRunResult) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerrunnable_h__
