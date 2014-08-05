/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerprivate_h__
#define mozilla_dom_workers_workerprivate_h__

#include "Workers.h"

#include "nsIContentSecurityPolicy.h"
#include "nsPIDOMWindow.h"

#include "mozilla/CondVar.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "StructuredCloneTags.h"

#include "Queue.h"
#include "WorkerFeature.h"

class JSAutoStructuredCloneBuffer;
class nsIChannel;
class nsIDocument;
class nsIEventTarget;
class nsIPrincipal;
class nsIScriptContext;
class nsIThread;
class nsIThreadInternal;
class nsITimer;
class nsIURI;

namespace JS {
struct RuntimeStats;
}

namespace mozilla {
namespace dom {
class Function;
}
}

#ifdef DEBUG
struct PRThread;
#endif

BEGIN_WORKERS_NAMESPACE

class AutoSyncLoopHolder;
class MessagePort;
class SharedWorker;
class WorkerControlRunnable;
class WorkerGlobalScope;
class WorkerPrivate;
class WorkerRunnable;

enum WorkerType
{
  WorkerTypeDedicated,
  WorkerTypeShared,
  WorkerTypeService
};

// SharedMutex is a small wrapper around an (internal) reference-counted Mutex
// object. It exists to avoid changing a lot of code to use Mutex* instead of
// Mutex&.
class SharedMutex
{
  typedef mozilla::Mutex Mutex;

  class RefCountedMutex MOZ_FINAL : public Mutex
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
    return *mMutex;
  }

  operator const Mutex&() const
  {
    return *mMutex;
  }

  void
  AssertCurrentThreadOwns() const
  {
    mMutex->AssertCurrentThreadOwns();
  }
};

template <class Derived>
class WorkerPrivateParent : public DOMEventTargetHelper
{
  class SynchronizeAndResumeRunnable;

protected:
  class EventTarget;
  friend class EventTarget;

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
    nsString mOrigin;
  };

  struct LoadInfo
  {
    // All of these should be released in ForgetMainThreadObjects.
    nsCOMPtr<nsIURI> mBaseURI;
    nsCOMPtr<nsIURI> mResolvedScriptURI;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    nsCOMPtr<nsIScriptContext> mScriptContext;
    nsCOMPtr<nsPIDOMWindow> mWindow;
    nsCOMPtr<nsIContentSecurityPolicy> mCSP;
    nsCOMPtr<nsIChannel> mChannel;

    nsCString mDomain;

    bool mEvalAllowed;
    bool mReportCSPViolations;
    bool mXHRParamsAllowed;
    bool mPrincipalIsSystem;
    bool mIsInPrivilegedApp;
    bool mIsInCertifiedApp;

    LoadInfo()
    : mEvalAllowed(false), mReportCSPViolations(false),
      mXHRParamsAllowed(false), mPrincipalIsSystem(false),
      mIsInPrivilegedApp(false), mIsInCertifiedApp(false)
    { }

    void
    StealFrom(LoadInfo& aOther)
    {
      MOZ_ASSERT(!mBaseURI);
      aOther.mBaseURI.swap(mBaseURI);

      MOZ_ASSERT(!mResolvedScriptURI);
      aOther.mResolvedScriptURI.swap(mResolvedScriptURI);

      MOZ_ASSERT(!mPrincipal);
      aOther.mPrincipal.swap(mPrincipal);

      MOZ_ASSERT(!mScriptContext);
      aOther.mScriptContext.swap(mScriptContext);

      MOZ_ASSERT(!mWindow);
      aOther.mWindow.swap(mWindow);

      MOZ_ASSERT(!mCSP);
      aOther.mCSP.swap(mCSP);

      MOZ_ASSERT(!mChannel);
      aOther.mChannel.swap(mChannel);

      mDomain = aOther.mDomain;
      mEvalAllowed = aOther.mEvalAllowed;
      mReportCSPViolations = aOther.mReportCSPViolations;
      mXHRParamsAllowed = aOther.mXHRParamsAllowed;
      mPrincipalIsSystem = aOther.mPrincipalIsSystem;
      mIsInPrivilegedApp = aOther.mIsInPrivilegedApp;
      mIsInCertifiedApp = aOther.mIsInCertifiedApp;
    }
  };

protected:
  typedef mozilla::ErrorResult ErrorResult;

  SharedMutex mMutex;
  mozilla::CondVar mCondVar;
  mozilla::CondVar mMemoryReportCondVar;

  // Protected by mMutex.
  nsRefPtr<EventTarget> mEventTarget;
  nsTArray<nsRefPtr<WorkerRunnable>> mPreStartRunnables;

private:
  WorkerPrivate* mParent;
  nsString mScriptURL;
  nsCString mSharedWorkerName;
  LocationInfo mLocationInfo;
  // The lifetime of these objects within LoadInfo is managed explicitly;
  // they do not need to be cycle collected.
  LoadInfo mLoadInfo;

  // Only used for top level workers.
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedRunnables;
  nsRevocableEventPtr<SynchronizeAndResumeRunnable> mSynchronizeRunnable;

  // Only for ChromeWorkers without window and only touched on the main thread.
  nsTArray<nsCString> mHostObjectURIs;

  // Protected by mMutex.
  JSSettings mJSSettings;

  // Only touched on the parent thread (currently this is always the main
  // thread as SharedWorkers are always top-level).
  nsDataHashtable<nsUint64HashKey, SharedWorker*> mSharedWorkers;

  uint64_t mBusyCount;
  uint64_t mMessagePortSerial;
  Status mParentStatus;
  bool mParentSuspended;
  bool mIsChromeWorker;
  bool mMainThreadObjectsForgotten;
  WorkerType mWorkerType;
  TimeStamp mCreationTimeStamp;

protected:
  // The worker is owned by its thread, which is represented here.  This is set
  // in Construct() and emptied by WorkerFinishedRunnable, and conditionally
  // traversed by the cycle collector if the busy count is zero.
  nsRefPtr<WorkerPrivate> mSelfRef;

  WorkerPrivateParent(JSContext* aCx, WorkerPrivate* aParent,
                      const nsAString& aScriptURL, bool aIsChromeWorker,
                      WorkerType aWorkerType,
                      const nsACString& aSharedWorkerName,
                      LoadInfo& aLoadInfo);

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

  void
  PostMessageInternal(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const Optional<Sequence<JS::Value> >& aTransferable,
                      bool aToMessagePort, uint64_t aMessagePortSerial,
                      ErrorResult& aRv);

  nsresult
  DispatchPrivate(WorkerRunnable* aRunnable, nsIEventTarget* aSyncLoopTarget);

public:
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(WorkerPrivateParent,
                                                         DOMEventTargetHelper)

  void
  ClearSelfRef()
  {
    AssertIsOnParentThread();
    MOZ_ASSERT(mSelfRef);
    mSelfRef = nullptr;
  }

  nsresult
  Dispatch(WorkerRunnable* aRunnable)
  {
    return DispatchPrivate(aRunnable, nullptr);
  }

  nsresult
  DispatchControlRunnable(WorkerControlRunnable* aWorkerControlRunnable);

  already_AddRefed<WorkerRunnable>
  MaybeWrapAsWorkerRunnable(nsIRunnable* aRunnable);

  already_AddRefed<nsIEventTarget>
  GetEventTarget();

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

  // We can assume that an nsPIDOMWindow will be available for Suspend, Resume
  // and SynchronizeAndResume as these are only used for globals going in and
  // out of the bfcache.
  bool
  Suspend(JSContext* aCx, nsPIDOMWindow* aWindow);

  bool
  Resume(JSContext* aCx, nsPIDOMWindow* aWindow);

  bool
  SynchronizeAndResume(JSContext* aCx, nsPIDOMWindow* aWindow);

  bool
  Terminate(JSContext* aCx)
  {
    AssertIsOnParentThread();
    return TerminatePrivate(aCx);
  }

  bool
  Close(JSContext* aCx);

  bool
  ModifyBusyCount(JSContext* aCx, bool aIncrease);

  void
  ForgetMainThreadObjects(nsTArray<nsCOMPtr<nsISupports> >& aDoomed);

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value> >& aTransferable,
              ErrorResult& aRv)
  {
    PostMessageInternal(aCx, aMessage, aTransferable, false, 0, aRv);
  }

  void
  PostMessageToMessagePort(JSContext* aCx,
                           uint64_t aMessagePortSerial,
                           JS::Handle<JS::Value> aMessage,
                           const Optional<Sequence<JS::Value> >& aTransferable,
                           ErrorResult& aRv);

  bool
  DispatchMessageEventToMessagePort(
                               JSContext* aCx,
                               uint64_t aMessagePortSerial,
                               JSAutoStructuredCloneBuffer&& aBuffer,
                               nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects);

  uint64_t
  GetInnerWindowId();

  void
  UpdateRuntimeOptions(JSContext* aCx,
                       const JS::RuntimeOptions& aRuntimeOptions);

  void
  UpdatePreference(JSContext* aCx, WorkerPreference aPref, bool aValue);

  void
  UpdateJSWorkerMemoryParameter(JSContext* aCx, JSGCParamKey key,
                                uint32_t value);

#ifdef JS_GC_ZEAL
  void
  UpdateGCZeal(JSContext* aCx, uint8_t aGCZeal, uint32_t aFrequency);
#endif

  void
  GarbageCollect(JSContext* aCx, bool aShrinking);

  void
  CycleCollect(JSContext* aCx, bool aDummy);

  void
  OfflineStatusChangeEvent(JSContext* aCx, bool aIsOffline);

  bool
  RegisterSharedWorker(JSContext* aCx, SharedWorker* aSharedWorker);

  void
  UnregisterSharedWorker(JSContext* aCx, SharedWorker* aSharedWorker);

  void
  BroadcastErrorToSharedWorkers(JSContext* aCx,
                                const nsAString& aMessage,
                                const nsAString& aFilename,
                                const nsAString& aLine,
                                uint32_t aLineNumber,
                                uint32_t aColumnNumber,
                                uint32_t aFlags);

  void
  WorkerScriptLoaded();

  void
  QueueRunnable(nsIRunnable* aRunnable)
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

    MutexAutoLock lock(mMutex);
    return mParentStatus < Terminating;
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
    return mLoadInfo.mScriptContext;
  }

  const nsString&
  ScriptURL() const
  {
    return mScriptURL;
  }

  const nsCString&
  Domain() const
  {
    return mLoadInfo.mDomain;
  }

  nsIURI*
  GetBaseURI() const
  {
    AssertIsOnMainThread();
    return mLoadInfo.mBaseURI;
  }

  void
  SetBaseURI(nsIURI* aBaseURI);

  nsIURI*
  GetResolvedScriptURI() const
  {
    AssertIsOnMainThread();
    return mLoadInfo.mResolvedScriptURI;
  }

  TimeStamp CreationTimeStamp() const
  {
    return mCreationTimeStamp;
  }

  nsIPrincipal*
  GetPrincipal() const
  {
    AssertIsOnMainThread();
    return mLoadInfo.mPrincipal;
  }

  // This method allows the principal to be retrieved off the main thread.
  // Principals are main-thread objects so the caller must ensure that all
  // access occurs on the main thread.
  nsIPrincipal*
  GetPrincipalDontAssertMainThread() const
  {
      return mLoadInfo.mPrincipal;
  }

  void
  SetPrincipal(nsIPrincipal* aPrincipal);

  bool
  UsesSystemPrincipal() const
  {
    return mLoadInfo.mPrincipalIsSystem;
  }

  bool
  IsInPrivilegedApp() const
  {
    return mLoadInfo.mIsInPrivilegedApp;
  }

  bool
  IsInCertifiedApp() const
  {
    return mLoadInfo.mIsInCertifiedApp;
  }

  already_AddRefed<nsIChannel>
  ForgetWorkerChannel()
  {
    AssertIsOnMainThread();
    return mLoadInfo.mChannel.forget();
  }

  nsIDocument*
  GetDocument() const
  {
    AssertIsOnMainThread();
    return mLoadInfo.mWindow ? mLoadInfo.mWindow->GetExtantDoc() : nullptr;
  }

  nsPIDOMWindow*
  GetWindow()
  {
    AssertIsOnMainThread();
    return mLoadInfo.mWindow;
  }

  nsIContentSecurityPolicy*
  GetCSP() const
  {
    AssertIsOnMainThread();
    return mLoadInfo.mCSP;
  }

  void
  SetCSP(nsIContentSecurityPolicy* aCSP)
  {
    AssertIsOnMainThread();
    mLoadInfo.mCSP = aCSP;
  }

  bool
  IsEvalAllowed() const
  {
    return mLoadInfo.mEvalAllowed;
  }

  void
  SetEvalAllowed(bool aEvalAllowed)
  {
    mLoadInfo.mEvalAllowed = aEvalAllowed;
  }

  bool
  GetReportCSPViolations() const
  {
    return mLoadInfo.mReportCSPViolations;
  }

  bool
  XHRParamsAllowed() const
  {
    return mLoadInfo.mXHRParamsAllowed;
  }

  void
  SetXHRParamsAllowed(bool aAllowed)
  {
    mLoadInfo.mXHRParamsAllowed = aAllowed;
  }

  LocationInfo&
  GetLocationInfo()
  {
    return mLocationInfo;
  }

  void
  CopyJSSettings(JSSettings& aSettings)
  {
    mozilla::MutexAutoLock lock(mMutex);
    aSettings = mJSSettings;
  }

  void
  CopyJSCompartmentOptions(JS::CompartmentOptions& aOptions)
  {
    mozilla::MutexAutoLock lock(mMutex);
    aOptions = IsChromeWorker() ? mJSSettings.chrome.compartmentOptions
                                : mJSSettings.content.compartmentOptions;
  }

  // The ability to be a chrome worker is orthogonal to the type of
  // worker [Dedicated|Shared|Service].
  bool
  IsChromeWorker() const
  {
    return mIsChromeWorker;
  }

  bool
  IsDedicatedWorker() const
  {
    return mWorkerType == WorkerTypeDedicated;
  }

  bool
  IsSharedWorker() const
  {
    return mWorkerType == WorkerTypeShared;
  }

  bool
  IsServiceWorker() const
  {
    return mWorkerType == WorkerTypeService;
  }

  const nsCString&
  SharedWorkerName() const
  {
    return mSharedWorkerName;
  }

  uint64_t
  NextMessagePortSerial()
  {
    AssertIsOnMainThread();
    return mMessagePortSerial++;
  }

  void
  GetAllSharedWorkers(nsTArray<nsRefPtr<SharedWorker>>& aSharedWorkers);

  void
  CloseSharedWorkersForWindow(nsPIDOMWindow* aWindow);

  void
  RegisterHostObjectURI(const nsACString& aURI);

  void
  UnregisterHostObjectURI(const nsACString& aURI);

  void
  StealHostObjectURIs(nsTArray<nsCString>& aArray);

  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(error)

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
  friend class AutoSyncLoopHolder;

  struct TimeoutInfo;

  class MemoryReporter;
  friend class MemoryReporter;

  enum GCTimerMode
  {
    PeriodicTimer = 0,
    IdleTimer,
    NoTimer
  };

  Queue<WorkerControlRunnable*, 4> mControlQueue;

  // Touched on multiple threads, protected with mMutex.
  JSContext* mJSContext;
  nsRefPtr<WorkerCrossThreadDispatcher> mCrossThreadDispatcher;
  nsTArray<nsCOMPtr<nsIRunnable>> mUndispatchedRunnablesForSyncLoop;
  nsCOMPtr<nsIThread> mThread;

  // Things touched on worker thread only.
  nsRefPtr<WorkerGlobalScope> mScope;
  nsTArray<ParentType*> mChildWorkers;
  nsTArray<WorkerFeature*> mFeatures;
  nsTArray<nsAutoPtr<TimeoutInfo>> mTimeouts;

  struct SyncLoopInfo
  {
    SyncLoopInfo(EventTarget* aEventTarget);

    nsRefPtr<EventTarget> mEventTarget;
    bool mCompleted;
    bool mResult;
#ifdef DEBUG
    bool mHasRun;
#endif
  };

  // This is only modified on the worker thread, but in DEBUG builds
  // AssertValidSyncLoop function iterates it on other threads. Therefore
  // modifications are done with mMutex held *only* in DEBUG builds.
  nsTArray<nsAutoPtr<SyncLoopInfo>> mSyncLoopStack;

  nsCOMPtr<nsITimer> mTimer;

  nsCOMPtr<nsITimer> mGCTimer;
  nsCOMPtr<nsIEventTarget> mPeriodicGCTimerTarget;
  nsCOMPtr<nsIEventTarget> mIdleGCTimerTarget;

  nsRefPtr<MemoryReporter> mMemoryReporter;

  nsRefPtrHashtable<nsUint64HashKey, MessagePort> mWorkerPorts;

  TimeStamp mKillTime;
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
  bool mCancelAllPendingRunnables;
  bool mPeriodicGCTimerRunning;
  bool mIdleGCTimerRunning;
  bool mWorkerScriptExecutedSuccessfully;

#ifdef DEBUG
  PRThread* mPRThread;
#endif

  bool mPreferences[WORKERPREF_COUNT];
  bool mOnLine;

protected:
  ~WorkerPrivate();

public:
  static already_AddRefed<WorkerPrivate>
  Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
              ErrorResult& aRv);

  static already_AddRefed<WorkerPrivate>
  Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
              bool aIsChromeWorker, WorkerType aWorkerType,
              const nsACString& aSharedWorkerName,
              LoadInfo* aLoadInfo, ErrorResult& aRv);

  static already_AddRefed<WorkerPrivate>
  Constructor(JSContext* aCx, const nsAString& aScriptURL, bool aIsChromeWorker,
              WorkerType aWorkerType, const nsACString& aSharedWorkerName,
              LoadInfo* aLoadInfo, ErrorResult& aRv);

  static bool
  WorkerAvailable(JSContext* /* unused */, JSObject* /* unused */);

  static nsresult
  GetLoadInfo(JSContext* aCx, nsPIDOMWindow* aWindow, WorkerPrivate* aParent,
              const nsAString& aScriptURL, bool aIsChromeWorker,
              LoadInfo* aLoadInfo);

  void
  DoRunLoop(JSContext* aCx);

  bool
  InterruptCallback(JSContext* aCx);

  nsresult
  IsOnCurrentThread(bool* aIsOnCurrentThread);

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
  TraceTimeouts(const TraceCallbacks& aCallbacks, void* aClosure) const;

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

  void
  PostMessageToParent(JSContext* aCx,
                      JS::Handle<JS::Value> aMessage,
                      const Optional<Sequence<JS::Value>>& aTransferable,
                      ErrorResult& aRv)
  {
    PostMessageToParentInternal(aCx, aMessage, aTransferable, false, 0, aRv);
  }

  void
  PostMessageToParentMessagePort(
                             JSContext* aCx,
                             uint64_t aMessagePortSerial,
                             JS::Handle<JS::Value> aMessage,
                             const Optional<Sequence<JS::Value>>& aTransferable,
                             ErrorResult& aRv);

  bool
  NotifyInternal(JSContext* aCx, Status aStatus);

  void
  ReportError(JSContext* aCx, const char* aMessage, JSErrorReport* aReport);

  int32_t
  SetTimeout(JSContext* aCx,
             Function* aHandler,
             const nsAString& aStringHandler,
             int32_t aTimeout,
             const Sequence<JS::Value>& aArguments,
             bool aIsInterval,
             ErrorResult& aRv);

  void
  ClearTimeout(int32_t aId);

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
  UpdateRuntimeOptionsInternal(JSContext* aCx, const JS::RuntimeOptions& aRuntimeOptions);

  void
  UpdatePreferenceInternal(JSContext* aCx, WorkerPreference aPref, bool aValue);

  void
  UpdateJSWorkerMemoryParameterInternal(JSContext* aCx, JSGCParamKey key, uint32_t aValue);

  enum WorkerRanOrNot {
    WorkerNeverRan = 0,
    WorkerRan
  };

  void
  ScheduleDeletion(WorkerRanOrNot aRanOrNot);

  bool
  BlockAndCollectRuntimeStats(JS::RuntimeStats* aRtStats, bool aAnonymize);

#ifdef JS_GC_ZEAL
  void
  UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal, uint32_t aFrequency);
#endif

  void
  GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                         bool aCollectChildren);

  void
  CycleCollectInternal(JSContext* aCx, bool aCollectChildren);

  void
  OfflineStatusChangeEventInternal(JSContext* aCx, bool aIsOffline);

  JSContext*
  GetJSContext() const
  {
    AssertIsOnWorkerThread();
    return mJSContext;
  }

  WorkerGlobalScope*
  GlobalScope() const
  {
    AssertIsOnWorkerThread();
    return mScope;
  }

  void
  SetThread(nsIThread* aThread);

  void
  AssertIsOnWorkerThread() const
#ifdef DEBUG
  ;
#else
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

  bool
  ConnectMessagePort(JSContext* aCx, uint64_t aMessagePortSerial);

  void
  DisconnectMessagePort(uint64_t aMessagePortSerial);

  MessagePort*
  GetMessagePort(uint64_t aMessagePortSerial);

  JSObject*
  CreateGlobalScope(JSContext* aCx);

  bool
  RegisterBindings(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  bool
  DumpEnabled() const
  {
    AssertIsOnWorkerThread();
    return mPreferences[WORKERPREF_DUMP];
  }

  bool
  DOMFetchEnabled() const
  {
    AssertIsOnWorkerThread();
    return mPreferences[WORKERPREF_DOM_FETCH];
  }

  bool
  OnLine() const
  {
    AssertIsOnWorkerThread();
    return mOnLine;
  }

  void
  StopSyncLoop(nsIEventTarget* aSyncLoopTarget, bool aResult);

  bool
  AllPendingRunnablesShouldBeCanceled() const
  {
    return mCancelAllPendingRunnables;
  }

  void
  OnProcessNextEvent(uint32_t aRecursionDepth);

  void
  AfterProcessNextEvent(uint32_t aRecursionDepth);

  void
  AssertValidSyncLoop(nsIEventTarget* aSyncLoopTarget)
#ifdef DEBUG
  ;
#else
  { }
#endif

  void
  SetWorkerScriptExecutedSuccessfully()
  {
    AssertIsOnWorkerThread();
    // Should only be called once!
    MOZ_ASSERT(!mWorkerScriptExecutedSuccessfully);
    mWorkerScriptExecutedSuccessfully = true;
  }

  // Only valid after CompileScriptRunnable has finished running!
  bool
  WorkerScriptExecutedSuccessfully() const
  {
    AssertIsOnWorkerThread();
    return mWorkerScriptExecutedSuccessfully;
  }

private:
  WorkerPrivate(JSContext* aCx, WorkerPrivate* aParent,
                const nsAString& aScriptURL, bool aIsChromeWorker,
                WorkerType aWorkerType, const nsACString& aSharedWorkerName,
                LoadInfo& aLoadInfo);

  void
  ClearMainEventQueue(WorkerRanOrNot aRanOrNot);

  bool
  MayContinueRunning()
  {
    AssertIsOnWorkerThread();

    Status status;
    {
      MutexAutoLock lock(mMutex);
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

  bool
  ProcessAllControlRunnables()
  {
    MutexAutoLock lock(mMutex);
    return ProcessAllControlRunnablesLocked();
  }

  bool
  ProcessAllControlRunnablesLocked();

  void
  EnableMemoryReporter();

  void
  DisableMemoryReporter();

  void
  WaitForWorkerEvents(PRIntervalTime interval = PR_INTERVAL_NO_TIMEOUT);

  void
  PostMessageToParentInternal(JSContext* aCx,
                              JS::Handle<JS::Value> aMessage,
                              const Optional<Sequence<JS::Value>>& aTransferable,
                              bool aToMessagePort,
                              uint64_t aMessagePortSerial,
                              ErrorResult& aRv);

  void
  GetAllPreferences(bool aPreferences[WORKERPREF_COUNT]) const
  {
    AssertIsOnWorkerThread();
    memcpy(aPreferences, mPreferences, WORKERPREF_COUNT * sizeof(bool));
  }

  already_AddRefed<nsIEventTarget>
  CreateNewSyncLoop();

  bool
  RunCurrentSyncLoop();

  bool
  DestroySyncLoop(uint32_t aLoopIndex, nsIThreadInternal* aThread = nullptr);

  void
  InitializeGCTimers();

  void
  SetGCTimerMode(GCTimerMode aMode);

  void
  ShutdownGCTimers();
};

// This class is only used to trick the DOM bindings.  We never create
// instances of it, and static_casting to it is fine since it doesn't add
// anything to WorkerPrivate.
class ChromeWorkerPrivate : public WorkerPrivate
{
public:
  static already_AddRefed<ChromeWorkerPrivate>
  Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
              ErrorResult& rv);

  static bool
  WorkerAvailable(JSContext* aCx, JSObject* /* unused */);

private:
  ChromeWorkerPrivate() MOZ_DELETE;
  ChromeWorkerPrivate(const ChromeWorkerPrivate& aRHS) MOZ_DELETE;
  ChromeWorkerPrivate& operator =(const ChromeWorkerPrivate& aRHS) MOZ_DELETE;
};

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx);

WorkerPrivate*
GetCurrentThreadWorkerPrivate();

bool
IsCurrentThreadRunningChromeWorker();

JSContext*
GetCurrentThreadJSContext();

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
  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mTarget;
  uint32_t mIndex;

public:
  AutoSyncLoopHolder(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  , mTarget(aWorkerPrivate->CreateNewSyncLoop())
  , mIndex(aWorkerPrivate->mSyncLoopStack.Length() - 1)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  ~AutoSyncLoopHolder()
  {
    if (mWorkerPrivate) {
      mWorkerPrivate->AssertIsOnWorkerThread();
      mWorkerPrivate->StopSyncLoop(mTarget, false);
      mWorkerPrivate->DestroySyncLoop(mIndex);
    }
  }

  bool
  Run()
  {
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate = nullptr;

    workerPrivate->AssertIsOnWorkerThread();

    return workerPrivate->RunCurrentSyncLoop();
  }

  nsIEventTarget*
  EventTarget() const
  {
    return mTarget;
  }
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_workerprivate_h__ */
