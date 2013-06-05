/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerprivate_h__
#define mozilla_dom_workers_workerprivate_h__

#include "mozilla/Attributes.h"
#include "Workers.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIThreadInternal.h"
#include "nsPIDOMWindow.h"

#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsEventQueue.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsTPriorityQueue.h"
#include "StructuredCloneTags.h"

#include "EventTarget.h"
#include "Queue.h"
#include "WorkerFeature.h"

class JSAutoStructuredCloneBuffer;
class nsIChannel;
class nsIDocument;
class nsIMemoryMultiReporter;
class nsIPrincipal;
class nsIScriptContext;
class nsIURI;
class nsPIDOMWindow;
class nsITimer;
class nsIXPCScriptNotify;
namespace JS { struct RuntimeStats; }

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class WorkerRunnable : public nsIRunnable
{
public:
  enum Target { ParentThread, WorkerThread };
  enum BusyBehavior { ModifyBusyCount, UnchangedBusyCount };
  enum ClearingBehavior { SkipWhenClearing, RunWhenClearing };

protected:
  WorkerPrivate* mWorkerPrivate;
  Target mTarget;
  BusyBehavior mBusyBehavior;
  ClearingBehavior mClearingBehavior;

public:
  NS_DECL_ISUPPORTS

  bool
  Dispatch(JSContext* aCx);

  static bool
  DispatchToMainThread(nsIRunnable*);

  bool
  WantsToRunDuringClear()
  {
    return mClearingBehavior == RunWhenClearing;
  }

protected:
  WorkerRunnable(WorkerPrivate* aWorkerPrivate, Target aTarget,
                 BusyBehavior aBusyBehavior,
                 ClearingBehavior aClearingBehavior)
#ifdef DEBUG
  ;
#else
  : mWorkerPrivate(aWorkerPrivate), mTarget(aTarget),
    mBusyBehavior(aBusyBehavior), mClearingBehavior(aClearingBehavior)
  { }
#endif

  virtual ~WorkerRunnable()
  { }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult);

  virtual bool
  DispatchInternal();

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) = 0;

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult);

  void NotifyScriptExecutedIfNeeded() const;

private:
  NS_DECL_NSIRUNNABLE
};

class WorkerSyncRunnable : public WorkerRunnable
{
protected:
  uint32_t mSyncQueueKey;
  bool mBypassSyncQueue;

protected:
  friend class WorkerPrivate;

  WorkerSyncRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                     bool aBypassSyncQueue = false,
                     ClearingBehavior aClearingBehavior = SkipWhenClearing)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount,
                   aClearingBehavior),
    mSyncQueueKey(aSyncQueueKey), mBypassSyncQueue(aBypassSyncQueue)
  { }

  virtual ~WorkerSyncRunnable()
  { }

  virtual bool
  DispatchInternal() MOZ_OVERRIDE;
};

class MainThreadSyncRunnable : public WorkerSyncRunnable
{
public:
  MainThreadSyncRunnable(WorkerPrivate* aWorkerPrivate,
                         ClearingBehavior aClearingBehavior,
                         uint32_t aSyncQueueKey,
                         bool aBypassSyncEventQueue)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, aBypassSyncEventQueue,
                       aClearingBehavior)
  {
    AssertIsOnMainThread();
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
  }
};

class WorkerControlRunnable : public WorkerRunnable
{
protected:
  WorkerControlRunnable(WorkerPrivate* aWorkerPrivate, Target aTarget,
                        BusyBehavior aBusyBehavior)
  : WorkerRunnable(aWorkerPrivate, aTarget, aBusyBehavior, SkipWhenClearing)
  { }

  virtual ~WorkerControlRunnable()
  { }

  virtual bool
  DispatchInternal() MOZ_OVERRIDE;
};

// SharedMutex is a small wrapper around an (internal) reference-counted Mutex
// object. It exists to avoid changing a lot of code to use Mutex* instead of
// Mutex&.
class SharedMutex
{
  typedef mozilla::Mutex Mutex;

  class RefCountedMutex : public Mutex
  {
  public:
    RefCountedMutex(const char* aName)
    : Mutex(aName)
    { }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMutex)

  private:
    ~RefCountedMutex()
    { }
  };

  nsRefPtr<RefCountedMutex> mMutex;

public:
  SharedMutex(const char* aName)
  : mMutex(new RefCountedMutex(aName))
  { }

  SharedMutex(SharedMutex& aOther)
  : mMutex(aOther.mMutex)
  { }

  operator Mutex&()
  {
    MOZ_ASSERT(mMutex);
    return *mMutex;
  }

  operator const Mutex&() const
  {
    MOZ_ASSERT(mMutex);
    return *mMutex;
  }

  void
  AssertCurrentThreadOwns() const
  {
    MOZ_ASSERT(mMutex);
    mMutex->AssertCurrentThreadOwns();
  }
};

template <class Derived>
class WorkerPrivateParent : public EventTarget
{
public:
  struct LocationInfo
  {
    nsCString mHref;
    nsCString mProtocol;
    nsCString mHost;
    nsCString mHostname;
    nsCString mPort;
    nsCString mPathname;
    nsCString mSearch;
    nsCString mHash;
  };

protected:
  SharedMutex mMutex;
  mozilla::CondVar mCondVar;
  mozilla::CondVar mMemoryReportCondVar;

private:
  JSObject* mJSObject;
  WorkerPrivate* mParent;
  JSContext* mParentJSContext;
  nsString mScriptURL;
  nsCString mDomain;
  LocationInfo mLocationInfo;

  // Main-thread things.
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIScriptContext> mScriptContext;
  nsCOMPtr<nsIXPCScriptNotify> mScriptNotify;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mScriptURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;

  // Only used for top level workers.
  nsTArray<nsRefPtr<WorkerRunnable> > mQueuedRunnables;

  uint64_t mBusyCount;
  Status mParentStatus;
  uint32_t mJSContextOptions;
  uint32_t mJSRuntimeHeapSize;
  uint32_t mJSWorkerAllocationThreshold;
  uint8_t mGCZeal;
  bool mJSObjectRooted;
  bool mParentSuspended;
  bool mIsChromeWorker;
  bool mPrincipalIsSystem;
  bool mMainThreadObjectsForgotten;
  bool mEvalAllowed;
  bool mReportCSPViolations;

protected:
  WorkerPrivateParent(JSContext* aCx, JS::Handle<JSObject*> aObject, WorkerPrivate* aParent,
                      JSContext* aParentJSContext, const nsAString& aScriptURL,
                      bool aIsChromeWorker, const nsACString& aDomain,
                      nsCOMPtr<nsPIDOMWindow>& aWindow,
                      nsCOMPtr<nsIScriptContext>& aScriptContext,
                      nsCOMPtr<nsIURI>& aBaseURI,
                      nsCOMPtr<nsIPrincipal>& aPrincipal,
                      nsCOMPtr<nsIChannel>& aChannel,
                      nsCOMPtr<nsIContentSecurityPolicy>& aCSP,
                      bool aEvalAllowed,
                      bool aReportCSPViolations);

  ~WorkerPrivateParent();

private:
  Derived*
  ParentAsWorkerPrivate() const
  {
    return static_cast<Derived*>(const_cast<WorkerPrivateParent*>(this));
  }

  // aCx is null when called from the finalizer
  bool
  NotifyPrivate(JSContext* aCx, Status aStatus);

  // aCx is null when called from the finalizer
  bool
  TerminatePrivate(JSContext* aCx)
  {
    return NotifyPrivate(aCx, Terminating);
  }

public:
  // May be called on any thread...
  bool
  Start();

  // Called on the parent thread.
  bool
  Notify(JSContext* aCx, Status aStatus)
  {
    return NotifyPrivate(aCx, aStatus);
  }

  bool
  Cancel(JSContext* aCx)
  {
    return Notify(aCx, Canceling);
  }

  bool
  Kill(JSContext* aCx)
  {
    return Notify(aCx, Killing);
  }

  bool
  Suspend(JSContext* aCx);

  bool
  Resume(JSContext* aCx);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  void
  Finish(JSContext* aCx)
  {
    RootJSObject(aCx, false);
  }

  bool
  Terminate(JSContext* aCx)
  {
    AssertIsOnParentThread();
    RootJSObject(aCx, false);
    return TerminatePrivate(aCx);
  }

  bool
  Close(JSContext* aCx);

  bool
  ModifyBusyCount(JSContext* aCx, bool aIncrease);

  bool
  RootJSObject(JSContext* aCx, bool aRoot);

  void
  ForgetMainThreadObjects(nsTArray<nsCOMPtr<nsISupports> >& aDoomed);

  bool
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              JS::Handle<JS::Value> aTransferable);

  uint64_t
  GetInnerWindowId();

  void
  UpdateJSContextOptions(JSContext* aCx, uint32_t aOptions);

  void
  UpdateJSWorkerMemoryParameter(JSContext* aCx, JSGCParamKey key, uint32_t value);

#ifdef JS_GC_ZEAL
  void
  UpdateGCZeal(JSContext* aCx, uint8_t aGCZeal);
#endif

  void
  GarbageCollect(JSContext* aCx, bool aShrinking);

  void
  QueueRunnable(WorkerRunnable* aRunnable)
  {
    AssertIsOnMainThread();
    mQueuedRunnables.AppendElement(aRunnable);
  }

  WorkerPrivate*
  GetParent() const
  {
    return mParent;
  }

  bool
  IsSuspended() const
  {
    AssertIsOnParentThread();
    return mParentSuspended;
  }

  bool
  IsAcceptingEvents()
  {
    AssertIsOnParentThread();
    bool acceptingEvents;
    {
      mozilla::MutexAutoLock lock(mMutex);
      acceptingEvents = mParentStatus < Terminating;
    }
    return acceptingEvents;
  }

  Status
  ParentStatus() const
  {
    mMutex.AssertCurrentThreadOwns();
    return mParentStatus;
  }

  JSContext*
  ParentJSContext() const;

  nsIScriptContext*
  GetScriptContext() const
  {
    AssertIsOnMainThread();
    return mScriptContext;
  }

  nsIXPCScriptNotify*
  GetScriptNotify() const
  {
    AssertIsOnMainThread();
    return mScriptNotify;
  }

  JSObject*
  GetJSObject() const
  {
    return mJSObject;
  }

  const nsString&
  ScriptURL() const
  {
    return mScriptURL;
  }

  const nsCString&
  Domain() const
  {
    return mDomain;
  }

  nsIURI*
  GetBaseURI() const
  {
    AssertIsOnMainThread();
    return mBaseURI;
  }

  void
  SetBaseURI(nsIURI* aBaseURI);

  nsIURI*
  GetScriptURI() const
  {
    AssertIsOnMainThread();
    return mScriptURI;
  }

  void
  SetScriptURI(nsIURI* aScriptURI)
  {
    AssertIsOnMainThread();
    mScriptURI = aScriptURI;
  }

  nsIPrincipal*
  GetPrincipal() const
  {
    AssertIsOnMainThread();
    return mPrincipal;
  }

  void
  SetPrincipal(nsIPrincipal* aPrincipal);

  bool
  UsesSystemPrincipal() const
  {
    return mPrincipalIsSystem;
  }

  nsIChannel*
  GetChannel() const
  {
    AssertIsOnMainThread();
    return mChannel;
  }

  nsIDocument*
  GetDocument() const
  {
    AssertIsOnMainThread();
    return mWindow ? mWindow->GetExtantDoc() : nullptr;
  }

  nsPIDOMWindow*
  GetWindow()
  {
    AssertIsOnMainThread();
    return mWindow;
  }

  nsIContentSecurityPolicy*
  GetCSP() const
  {
    AssertIsOnMainThread();
    return mCSP;
  }

  void
  SetCSP(nsIContentSecurityPolicy* aCSP)
  {
    AssertIsOnMainThread();
    mCSP = aCSP;
  }

  bool
  IsEvalAllowed() const
  {
    return mEvalAllowed;
  }

  void
  SetEvalAllowed(bool aEvalAllowed)
  {
    mEvalAllowed = aEvalAllowed;
  }

  bool
  GetReportCSPViolations() const
  {
    return mReportCSPViolations;
  }

  LocationInfo&
  GetLocationInfo()
  {
    return mLocationInfo;
  }

  uint32_t
  GetJSContextOptions() const
  {
    return mJSContextOptions;
  }

  uint32_t
  GetJSRuntimeHeapSize() const
  {
    return mJSRuntimeHeapSize;
  }

  uint32_t
  GetJSWorkerAllocationThreshold() const
  {
    return mJSWorkerAllocationThreshold;
  }

#ifdef JS_GC_ZEAL
  uint8_t
  GetGCZeal() const
  {
    return mGCZeal;
  }
#endif

  bool
  IsChromeWorker() const
  {
    return mIsChromeWorker;
  }

#ifdef DEBUG
  void
  AssertIsOnParentThread() const;

  void
  AssertInnerWindowIsCorrect() const;
#else
  void
  AssertIsOnParentThread() const
  { }

  void
  AssertInnerWindowIsCorrect() const
  { }
#endif
};

class WorkerPrivate : public WorkerPrivateParent<WorkerPrivate>
{
  friend class WorkerPrivateParent<WorkerPrivate>;
  typedef WorkerPrivateParent<WorkerPrivate> ParentType;

  struct TimeoutInfo;

  typedef Queue<WorkerRunnable*, 50> EventQueue;
  EventQueue mQueue;
  EventQueue mControlQueue;

  struct SyncQueue
  {
    Queue<WorkerRunnable*, 10> mQueue;
    bool mComplete;
    bool mResult;

    SyncQueue()
    : mComplete(false), mResult(false)
    { }

    ~SyncQueue()
    {
      WorkerRunnable* event;
      while (mQueue.Pop(event)) {
        event->Release();
      }
    }
  };

  class MemoryReporter;
  friend class MemoryReporter;

  nsTArray<nsAutoPtr<SyncQueue> > mSyncQueues;

  // Touched on multiple threads, protected with mMutex.
  JSContext* mJSContext;
  nsRefPtr<WorkerCrossThreadDispatcher> mCrossThreadDispatcher;

  // Things touched on worker thread only.
  nsTArray<ParentType*> mChildWorkers;
  nsTArray<WorkerFeature*> mFeatures;
  nsTArray<nsAutoPtr<TimeoutInfo> > mTimeouts;

  nsCOMPtr<nsITimer> mTimer;
  nsRefPtr<MemoryReporter> mMemoryReporter;

  mozilla::TimeStamp mKillTime;
  uint32_t mErrorHandlerRecursionCount;
  uint32_t mNextTimeoutId;
  Status mStatus;
  bool mSuspended;
  bool mTimerRunning;
  bool mRunningExpiredTimeouts;
  bool mCloseHandlerStarted;
  bool mCloseHandlerFinished;
  bool mMemoryReporterRunning;
  bool mBlockedForMemoryReporter;
  bool mXHRParamsAllowed;

#ifdef DEBUG
  nsCOMPtr<nsIThread> mThread;
#endif

public:
  ~WorkerPrivate();

  static already_AddRefed<WorkerPrivate>
  Create(JSContext* aCx, JS::Handle<JSObject*> aObj, WorkerPrivate* aParent,
         JS::Handle<JSString*> aScriptURL, bool aIsChromeWorker);

  void
  DoRunLoop(JSContext* aCx);

  bool
  OperationCallback(JSContext* aCx);

  bool
  Dispatch(WorkerRunnable* aEvent)
  {
    return Dispatch(aEvent, &mQueue);
  }

  bool
  Dispatch(WorkerSyncRunnable* aEvent)
  {
    if (aEvent->mBypassSyncQueue) {
      return Dispatch(aEvent, &mQueue);
    }

    return DispatchToSyncQueue(aEvent);
  }

  bool
  Dispatch(WorkerControlRunnable* aEvent)
  {
    return Dispatch(aEvent, &mControlQueue);
  }

  bool
  CloseInternal(JSContext* aCx)
  {
    AssertIsOnWorkerThread();
    return NotifyInternal(aCx, Closing);
  }

  bool
  SuspendInternal(JSContext* aCx);

  bool
  ResumeInternal(JSContext* aCx);

  void
  TraceInternal(JSTracer* aTrc);

  bool
  ModifyBusyCountFromWorker(JSContext* aCx, bool aIncrease);

  bool
  AddChildWorker(JSContext* aCx, ParentType* aChildWorker);

  void
  RemoveChildWorker(JSContext* aCx, ParentType* aChildWorker);

  bool
  AddFeature(JSContext* aCx, WorkerFeature* aFeature);

  void
  RemoveFeature(JSContext* aCx, WorkerFeature* aFeature);

  void
  NotifyFeatures(JSContext* aCx, Status aStatus);

  bool
  HasActiveFeatures()
  {
    return !(mChildWorkers.IsEmpty() && mTimeouts.IsEmpty() &&
             mFeatures.IsEmpty());
  }

  uint32_t
  CreateNewSyncLoop();

  bool
  RunSyncLoop(JSContext* aCx, uint32_t aSyncLoopKey);

  void
  StopSyncLoop(uint32_t aSyncLoopKey, bool aSyncResult);

  void
  DestroySyncLoop(uint32_t aSyncLoopKey);

  bool
  PostMessageToParent(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      JS::Handle<JS::Value> transferable);

  bool
  NotifyInternal(JSContext* aCx, Status aStatus);

  void
  ReportError(JSContext* aCx, const char* aMessage, JSErrorReport* aReport);

  bool
  SetTimeout(JSContext* aCx, unsigned aArgc, jsval* aVp, bool aIsInterval);

  bool
  ClearTimeout(JSContext* aCx, uint32_t aId);

  bool
  RunExpiredTimeouts(JSContext* aCx);

  bool
  RescheduleTimeoutTimer(JSContext* aCx);

  void
  CloseHandlerStarted()
  {
    AssertIsOnWorkerThread();
    mCloseHandlerStarted = true;
  }

  void
  CloseHandlerFinished()
  {
    AssertIsOnWorkerThread();
    mCloseHandlerFinished = true;
  }

  void
  UpdateJSContextOptionsInternal(JSContext* aCx, uint32_t aOptions);

  void
  UpdateJSWorkerMemoryParameterInternal(JSContext* aCx, JSGCParamKey key, uint32_t aValue);

  void
  ScheduleDeletion(bool aWasPending);

  bool
  BlockAndCollectRuntimeStats(JS::RuntimeStats* aRtStats);

  bool
  XHRParamsAllowed() const
  {
    return mXHRParamsAllowed;
  }

  void
  SetXHRParamsAllowed(bool aAllowed)
  {
    mXHRParamsAllowed = aAllowed;
  }

#ifdef JS_GC_ZEAL
  void
  UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal);
#endif

  void
  GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                         bool aCollectChildren);

  JSContext*
  GetJSContext() const
  {
    AssertIsOnWorkerThread();
    return mJSContext;
  }

#ifdef DEBUG
  void
  AssertIsOnWorkerThread() const;

  void
  SetThread(nsIThread* aThread)
  {
    mThread = aThread;
  }
#else
  void
  AssertIsOnWorkerThread() const
  { }
#endif

  WorkerCrossThreadDispatcher*
  GetCrossThreadDispatcher();

  // This may block!
  void
  BeginCTypesCall();

  // This may block!
  void
  EndCTypesCall();

  void
  BeginCTypesCallback()
  {
    // If a callback is beginning then we need to do the exact same thing as
    // when a ctypes call ends.
    EndCTypesCall();
  }

  void
  EndCTypesCallback()
  {
    // If a callback is ending then we need to do the exact same thing as
    // when a ctypes call begins.
    BeginCTypesCall();
  }

private:
  WorkerPrivate(JSContext* aCx, JS::Handle<JSObject*> aObject, WorkerPrivate* aParent,
                JSContext* aParentJSContext, const nsAString& aScriptURL,
                bool aIsChromeWorker, const nsACString& aDomain,
                nsCOMPtr<nsPIDOMWindow>& aWindow,
                nsCOMPtr<nsIScriptContext>& aScriptContext,
                nsCOMPtr<nsIURI>& aBaseURI, nsCOMPtr<nsIPrincipal>& aPrincipal,
                nsCOMPtr<nsIChannel>& aChannel,
                nsCOMPtr<nsIContentSecurityPolicy>& aCSP, bool aEvalAllowed,
                bool aReportCSPViolations, bool aXHRParamsAllowed);

  static bool
  GetContentSecurityPolicy(JSContext *aCx,
                           nsIContentSecurityPolicy** aCsp);

  bool
  Dispatch(WorkerRunnable* aEvent, EventQueue* aQueue);

  bool
  DispatchToSyncQueue(WorkerSyncRunnable* aEvent);

  void
  ClearQueue(EventQueue* aQueue);

  bool
  MayContinueRunning()
  {
    AssertIsOnWorkerThread();

    Status status;
    {
      mozilla::MutexAutoLock lock(mMutex);
      status = mStatus;
    }

    if (status >= Killing) {
      return false;
    }
    if (status >= Running) {
      return mKillTime.IsNull() || RemainingRunTimeMS() > 0;
    }
    return true;
  }

  uint32_t
  RemainingRunTimeMS() const;

  void
  CancelAllTimeouts(JSContext* aCx);

  bool
  ScheduleKillCloseEventRunnable(JSContext* aCx);

  void
  StopAcceptingEvents()
  {
    AssertIsOnWorkerThread();

    mozilla::MutexAutoLock lock(mMutex);

    mStatus = Dead;
    mJSContext = nullptr;

    ClearQueue(&mControlQueue);
    ClearQueue(&mQueue);
  }

  bool
  ProcessAllControlRunnables();

  void
  EnableMemoryReporter();

  void
  DisableMemoryReporter();

  void
  WaitForWorkerEvents(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT);

  static bool
  CheckXHRParamsAllowed(nsPIDOMWindow* aWindow);
};

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx);

enum WorkerStructuredDataType
{
  DOMWORKER_SCTAG_FILE = SCTAG_DOM_MAX,
  DOMWORKER_SCTAG_BLOB,

  DOMWORKER_SCTAG_END
};

JSStructuredCloneCallbacks*
WorkerStructuredCloneCallbacks(bool aMainRuntime);

JSStructuredCloneCallbacks*
ChromeWorkerStructuredCloneCallbacks(bool aMainRuntime);

class AutoSyncLoopHolder
{
public:
  AutoSyncLoopHolder(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate), mSyncLoopKey(UINT32_MAX)
  {
    mSyncLoopKey = mWorkerPrivate->CreateNewSyncLoop();
  }

  ~AutoSyncLoopHolder()
  {
    if (mWorkerPrivate) {
      mWorkerPrivate->StopSyncLoop(mSyncLoopKey, false);
      mWorkerPrivate->DestroySyncLoop(mSyncLoopKey);
    }
  }

  bool
  RunAndForget(JSContext* aCx)
  {
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate = nullptr;
    return workerPrivate->RunSyncLoop(aCx, mSyncLoopKey);
  }

  uint32_t
  SyncQueueKey() const
  {
    return mSyncLoopKey;
  }

private:
  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncLoopKey;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_workerprivate_h__ */
