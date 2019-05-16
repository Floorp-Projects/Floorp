/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerprivate_h__
#define mozilla_dom_workers_workerprivate_h__

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/CondVar.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RelativeTimeline.h"
#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIEventTarget.h"
#include "nsTObserverArray.h"

#include "js/ContextOptions.h"
#include "mozilla/dom/Worker.h"
#include "mozilla/dom/WorkerHolder.h"
#include "mozilla/dom/WorkerLoadInfo.h"
#include "mozilla/dom/workerinternals/JSSettings.h"
#include "mozilla/dom/workerinternals/Queue.h"
#include "mozilla/PerformanceCounter.h"
#include "mozilla/ThreadBound.h"

class nsIThreadInternal;

namespace mozilla {
class ThrottledEventQueue;
namespace dom {

// If you change this, the corresponding list in nsIWorkerDebugger.idl needs
// to be updated too.
enum WorkerType { WorkerTypeDedicated, WorkerTypeShared, WorkerTypeService };

class ClientInfo;
class ClientSource;
class Function;
class MessagePort;
class MessagePortIdentifier;
class PerformanceStorage;
class RemoteWorkerChild;
class WorkerControlRunnable;
class WorkerCSPEventListener;
class WorkerDebugger;
class WorkerDebuggerGlobalScope;
class WorkerErrorReport;
class WorkerEventTarget;
class WorkerGlobalScope;
class WorkerRunnable;
class WorkerDebuggeeRunnable;
class WorkerThread;

// SharedMutex is a small wrapper around an (internal) reference-counted Mutex
// object. It exists to avoid changing a lot of code to use Mutex* instead of
// Mutex&.
class SharedMutex {
  typedef mozilla::Mutex Mutex;

  class RefCountedMutex final : public Mutex {
   public:
    explicit RefCountedMutex(const char* aName) : Mutex(aName) {}

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMutex)

   private:
    ~RefCountedMutex() {}
  };

  RefPtr<RefCountedMutex> mMutex;

 public:
  explicit SharedMutex(const char* aName)
      : mMutex(new RefCountedMutex(aName)) {}

  SharedMutex(SharedMutex& aOther) : mMutex(aOther.mMutex) {}

  operator Mutex&() { return *mMutex; }

  operator const Mutex&() const { return *mMutex; }

  void AssertCurrentThreadOwns() const { mMutex->AssertCurrentThreadOwns(); }
};

class WorkerPrivate : public RelativeTimeline {
 public:
  struct LocationInfo {
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

  NS_INLINE_DECL_REFCOUNTING(WorkerPrivate)

  static already_AddRefed<WorkerPrivate> Constructor(
      JSContext* aCx, const nsAString& aScriptURL, bool aIsChromeWorker,
      WorkerType aWorkerType, const nsAString& aWorkerName,
      const nsACString& aServiceWorkerScope, WorkerLoadInfo* aLoadInfo,
      ErrorResult& aRv);

  enum LoadGroupBehavior { InheritLoadGroup, OverrideLoadGroup };

  static nsresult GetLoadInfo(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                              WorkerPrivate* aParent,
                              const nsAString& aScriptURL, bool aIsChromeWorker,
                              LoadGroupBehavior aLoadGroupBehavior,
                              WorkerType aWorkerType,
                              WorkerLoadInfo* aLoadInfo);

  void Traverse(nsCycleCollectionTraversalCallback& aCb);

  void ClearSelfAndParentEventTargetRef() {
    AssertIsOnParentThread();
    MOZ_ASSERT(mSelfRef);
    mParentEventTargetRef = nullptr;
    mSelfRef = nullptr;
  }

  // May be called on any thread...
  bool Start();

  // Called on the parent thread.
  bool Notify(WorkerStatus aStatus);

  bool Cancel() { return Notify(Canceling); }

  bool Close();

  // The passed principal must be the Worker principal in case of a
  // ServiceWorker and the loading principal for any other type.
  static void OverrideLoadInfoLoadGroup(WorkerLoadInfo& aLoadInfo,
                                        nsIPrincipal* aPrincipal);

  bool IsDebuggerRegistered() {
    AssertIsOnMainThread();

    // No need to lock here since this is only ever modified by the same thread.
    return mDebuggerRegistered;
  }

  void SetIsDebuggerRegistered(bool aDebuggerRegistered) {
    AssertIsOnMainThread();

    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(mDebuggerRegistered != aDebuggerRegistered);
    mDebuggerRegistered = aDebuggerRegistered;

    mCondVar.Notify();
  }

  void WaitForIsDebuggerRegistered(bool aDebuggerRegistered) {
    AssertIsOnParentThread();

    MOZ_ASSERT(!NS_IsMainThread());

    MutexAutoLock lock(mMutex);

    while (mDebuggerRegistered != aDebuggerRegistered) {
      mCondVar.Wait();
    }
  }

  nsresult SetIsDebuggerReady(bool aReady);

  WorkerDebugger* Debugger() const {
    AssertIsOnMainThread();

    MOZ_ASSERT(mDebugger);
    return mDebugger;
  }

  void SetDebugger(WorkerDebugger* aDebugger) {
    AssertIsOnMainThread();

    MOZ_ASSERT(mDebugger != aDebugger);
    mDebugger = aDebugger;
  }

  JS::UniqueChars AdoptDefaultLocale() {
    MOZ_ASSERT(mDefaultLocale,
               "the default locale must have been successfully set for anyone "
               "to be trying to adopt it");
    return std::move(mDefaultLocale);
  }

  MOZ_CAN_RUN_SCRIPT
  void DoRunLoop(JSContext* aCx);

  bool InterruptCallback(JSContext* aCx);

  bool IsOnCurrentThread();

  void CloseInternal();

  bool FreezeInternal();

  bool ThawInternal();

  void PropagateFirstPartyStorageAccessGrantedInternal();

  void TraverseTimeouts(nsCycleCollectionTraversalCallback& aCallback);

  void UnlinkTimeouts();

  bool ModifyBusyCountFromWorker(bool aIncrease);

  bool AddChildWorker(WorkerPrivate* aChildWorker);

  void RemoveChildWorker(WorkerPrivate* aChildWorker);

  void PostMessageToParent(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           const Sequence<JSObject*>& aTransferable,
                           ErrorResult& aRv);

  void PostMessageToParentMessagePort(JSContext* aCx,
                                      JS::Handle<JS::Value> aMessage,
                                      const Sequence<JSObject*>& aTransferable,
                                      ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void EnterDebuggerEventLoop();

  void LeaveDebuggerEventLoop();

  void PostMessageToDebugger(const nsAString& aMessage);

  void SetDebuggerImmediate(Function& aHandler, ErrorResult& aRv);

  void ReportErrorToDebugger(const nsAString& aFilename, uint32_t aLineno,
                             const nsAString& aMessage);

  bool NotifyInternal(WorkerStatus aStatus);

  void ReportError(JSContext* aCx, JS::ConstUTF8CharsZ aToStringResult,
                   JSErrorReport* aReport);

  static void ReportErrorToConsole(const char* aMessage);

  static void ReportErrorToConsole(const char* aMessage,
                                   const nsTArray<nsString>& aParams);

  int32_t SetTimeout(JSContext* aCx, nsIScriptTimeoutHandler* aHandler,
                     int32_t aTimeout, bool aIsInterval, ErrorResult& aRv);

  void ClearTimeout(int32_t aId);

  MOZ_CAN_RUN_SCRIPT bool RunExpiredTimeouts(JSContext* aCx);

  bool RescheduleTimeoutTimer(JSContext* aCx);

  void UpdateContextOptionsInternal(JSContext* aCx,
                                    const JS::ContextOptions& aContextOptions);

  void UpdateLanguagesInternal(const nsTArray<nsString>& aLanguages);

  void UpdateJSWorkerMemoryParameterInternal(JSContext* aCx, JSGCParamKey key,
                                             uint32_t aValue);

  enum WorkerRanOrNot { WorkerNeverRan = 0, WorkerRan };

  void ScheduleDeletion(WorkerRanOrNot aRanOrNot);

  bool CollectRuntimeStats(JS::RuntimeStats* aRtStats, bool aAnonymize);

#ifdef JS_GC_ZEAL
  void UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal,
                            uint32_t aFrequency);
#endif

  void GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                              bool aCollectChildren);

  void CycleCollectInternal(bool aCollectChildren);

  void OfflineStatusChangeEventInternal(bool aIsOffline);

  void MemoryPressureInternal();

  void SetFetchHandlerWasAdded() {
    MOZ_ASSERT(IsServiceWorker());
    AssertIsOnWorkerThread();
    mFetchHandlerWasAdded = true;
  }

  bool FetchHandlerWasAdded() const {
    MOZ_ASSERT(IsServiceWorker());
    AssertIsOnWorkerThread();
    return mFetchHandlerWasAdded;
  }

  JSContext* GetJSContext() const {
    AssertIsOnWorkerThread();
    return mJSContext;
  }

  WorkerGlobalScope* GlobalScope() const {
    MOZ_ACCESS_THREAD_BOUND(mWorkerThreadAccessible, data);
    return data->mScope;
  }

  WorkerDebuggerGlobalScope* DebuggerGlobalScope() const {
    MOZ_ACCESS_THREAD_BOUND(mWorkerThreadAccessible, data);
    return data->mDebuggerScope;
  }

  nsICSPEventListener* CSPEventListener() const;

  void SetThread(WorkerThread* aThread);

  void SetWorkerPrivateInWorkerThread(WorkerThread* aThread);

  void ResetWorkerPrivateInWorkerThread();

  bool IsOnWorkerThread() const;

  void AssertIsOnWorkerThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  // This may block!
  void BeginCTypesCall();

  // This may block!
  void EndCTypesCall();

  void BeginCTypesCallback() {
    // If a callback is beginning then we need to do the exact same thing as
    // when a ctypes call ends.
    EndCTypesCall();
  }

  void EndCTypesCallback() {
    // If a callback is ending then we need to do the exact same thing as
    // when a ctypes call begins.
    BeginCTypesCall();
  }

  bool ConnectMessagePort(JSContext* aCx,
                          const MessagePortIdentifier& aIdentifier);

  WorkerGlobalScope* GetOrCreateGlobalScope(JSContext* aCx);

  WorkerDebuggerGlobalScope* CreateDebuggerGlobalScope(JSContext* aCx);

  bool RegisterBindings(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  bool RegisterDebuggerBindings(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  bool OnLine() const {
    MOZ_ACCESS_THREAD_BOUND(mWorkerThreadAccessible, data);
    return data->mOnLine;
  }

  void StopSyncLoop(nsIEventTarget* aSyncLoopTarget, bool aResult);

  bool AllPendingRunnablesShouldBeCanceled() const {
    return mCancelAllPendingRunnables;
  }

  void ClearMainEventQueue(WorkerRanOrNot aRanOrNot);

  void ClearDebuggerEventQueue();

  void OnProcessNextEvent();

  void AfterProcessNextEvent();

  void AssertValidSyncLoop(nsIEventTarget* aSyncLoopTarget)
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  void SetWorkerScriptExecutedSuccessfully() {
    AssertIsOnWorkerThread();
    // Should only be called once!
    MOZ_ASSERT(!mWorkerScriptExecutedSuccessfully);
    mWorkerScriptExecutedSuccessfully = true;
  }

  // Only valid after CompileScriptRunnable has finished running!
  bool WorkerScriptExecutedSuccessfully() const {
    AssertIsOnWorkerThread();
    return mWorkerScriptExecutedSuccessfully;
  }

  // Get the event target to use when dispatching to the main thread
  // from this Worker thread.  This may be the main thread itself or
  // a ThrottledEventQueue to the main thread.
  nsIEventTarget* MainThreadEventTargetForMessaging();

  nsresult DispatchToMainThreadForMessaging(
      nsIRunnable* aRunnable, uint32_t aFlags = NS_DISPATCH_NORMAL);

  nsresult DispatchToMainThreadForMessaging(
      already_AddRefed<nsIRunnable> aRunnable,
      uint32_t aFlags = NS_DISPATCH_NORMAL);

  nsIEventTarget* MainThreadEventTarget();

  nsresult DispatchToMainThread(nsIRunnable* aRunnable,
                                uint32_t aFlags = NS_DISPATCH_NORMAL);

  nsresult DispatchToMainThread(already_AddRefed<nsIRunnable> aRunnable,
                                uint32_t aFlags = NS_DISPATCH_NORMAL);

  nsresult DispatchDebuggeeToMainThread(
      already_AddRefed<WorkerDebuggeeRunnable> aRunnable,
      uint32_t aFlags = NS_DISPATCH_NORMAL);

  // Get an event target that will dispatch runnables as control runnables on
  // the worker thread.  Implement nsICancelableRunnable if you wish to take
  // action on cancelation.
  nsISerialEventTarget* ControlEventTarget();

  // Get an event target that will attempt to dispatch a normal WorkerRunnable,
  // but if that fails will then fall back to a control runnable.
  nsISerialEventTarget* HybridEventTarget();

  void DumpCrashInformation(nsACString& aString);

  bool EnsureClientSource();

  bool EnsureCSPEventListener();

  void EnsurePerformanceStorage();

  void EnsurePerformanceCounter();

  Maybe<ClientInfo> GetClientInfo() const;

  const ClientState GetClientState() const;

  const Maybe<ServiceWorkerDescriptor> GetController();

  void Control(const ServiceWorkerDescriptor& aServiceWorker);

  void ExecutionReady();

  PerformanceStorage* GetPerformanceStorage();

  PerformanceCounter* GetPerformanceCounter();

  bool IsAcceptingEvents() {
    AssertIsOnParentThread();

    MutexAutoLock lock(mMutex);
    return mParentStatus < Canceling;
  }

  WorkerStatus ParentStatusProtected() {
    AssertIsOnParentThread();
    MutexAutoLock lock(mMutex);
    return mParentStatus;
  }

  WorkerStatus ParentStatus() const {
    mMutex.AssertCurrentThreadOwns();
    return mParentStatus;
  }

  Worker* ParentEventTargetRef() const {
    MOZ_DIAGNOSTIC_ASSERT(mParentEventTargetRef);
    return mParentEventTargetRef;
  }

  void SetParentEventTargetRef(Worker* aParentEventTargetRef) {
    MOZ_DIAGNOSTIC_ASSERT(aParentEventTargetRef);
    MOZ_DIAGNOSTIC_ASSERT(!mParentEventTargetRef);
    mParentEventTargetRef = aParentEventTargetRef;
  }

  bool ModifyBusyCount(bool aIncrease);

  // This method is used by RuntimeService to know what is going wrong the
  // shutting down.
  uint32_t BusyCount() { return mBusyCount; }

  // Check whether this worker is a secure context.  For use from the parent
  // thread only; the canonical "is secure context" boolean is stored on the
  // compartment of the worker global.  The only reason we don't
  // AssertIsOnParentThread() here is so we can assert that this value matches
  // the one on the compartment, which has to be done from the worker thread.
  bool IsSecureContext() const { return mIsSecureContext; }

  // Check whether we're running in automation.
  bool IsInAutomation() const { return mIsInAutomation; }

  TimeStamp CreationTimeStamp() const { return mCreationTimeStamp; }

  DOMHighResTimeStamp CreationTime() const { return mCreationTimeHighRes; }

  DOMHighResTimeStamp TimeStampToDOMHighRes(const TimeStamp& aTimeStamp) const {
    MOZ_ASSERT(!aTimeStamp.IsNull());
    TimeDuration duration = aTimeStamp - mCreationTimeStamp;
    return duration.ToMilliseconds();
  }

  LocationInfo& GetLocationInfo() { return mLocationInfo; }

  void CopyJSSettings(workerinternals::JSSettings& aSettings) {
    mozilla::MutexAutoLock lock(mMutex);
    aSettings = mJSSettings;
  }

  void CopyJSRealmOptions(JS::RealmOptions& aOptions) {
    mozilla::MutexAutoLock lock(mMutex);
    aOptions = IsChromeWorker() ? mJSSettings.chrome.realmOptions
                                : mJSSettings.content.realmOptions;
  }

  // The ability to be a chrome worker is orthogonal to the type of
  // worker [Dedicated|Shared|Service].
  bool IsChromeWorker() const { return mIsChromeWorker; }

  WorkerPrivate* GetParent() const { return mParent; }

  bool IsFrozen() const {
    AssertIsOnParentThread();
    return mParentFrozen;
  }

  bool IsParentWindowPaused() const {
    AssertIsOnParentThread();
    return mParentWindowPaused;
  }

  // When we debug a worker, we want to disconnect the window and the worker
  // communication. This happens calling this method.
  // Note: this method doesn't suspend the worker! Use Freeze/Thaw instead.
  void ParentWindowPaused();

  void ParentWindowResumed();

  const nsString& ScriptURL() const { return mScriptURL; }

  const nsString& WorkerName() const { return mWorkerName; }

  WorkerType Type() const { return mWorkerType; }

  bool IsDedicatedWorker() const { return mWorkerType == WorkerTypeDedicated; }

  bool IsSharedWorker() const { return mWorkerType == WorkerTypeShared; }

  bool IsServiceWorker() const { return mWorkerType == WorkerTypeService; }

  nsContentPolicyType ContentPolicyType() const {
    return ContentPolicyType(mWorkerType);
  }

  static nsContentPolicyType ContentPolicyType(WorkerType aWorkerType) {
    switch (aWorkerType) {
      case WorkerTypeDedicated:
        return nsIContentPolicy::TYPE_INTERNAL_WORKER;
      case WorkerTypeShared:
        return nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER;
      case WorkerTypeService:
        return nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid worker type");
        return nsIContentPolicy::TYPE_INVALID;
    }
  }

  nsIScriptContext* GetScriptContext() const {
    AssertIsOnMainThread();
    return mLoadInfo.mScriptContext;
  }

  const nsCString& Domain() const { return mLoadInfo.mDomain; }

  bool IsFromWindow() const { return mLoadInfo.mFromWindow; }

  nsLoadFlags GetLoadFlags() const { return mLoadInfo.mLoadFlags; }

  uint64_t WindowID() const { return mLoadInfo.mWindowID; }

  uint64_t ServiceWorkerID() const { return GetServiceWorkerDescriptor().Id(); }

  const nsCString& ServiceWorkerScope() const {
    return GetServiceWorkerDescriptor().Scope();
  }

  nsIURI* GetBaseURI() const {
    AssertIsOnMainThread();
    return mLoadInfo.mBaseURI;
  }

  void SetBaseURI(nsIURI* aBaseURI);

  nsIURI* GetResolvedScriptURI() const {
    AssertIsOnMainThread();
    return mLoadInfo.mResolvedScriptURI;
  }

  const nsString& ServiceWorkerCacheName() const {
    MOZ_DIAGNOSTIC_ASSERT(IsServiceWorker());
    AssertIsOnMainThread();
    return mLoadInfo.mServiceWorkerCacheName;
  }

  const ServiceWorkerDescriptor& GetServiceWorkerDescriptor() const {
    MOZ_DIAGNOSTIC_ASSERT(IsServiceWorker());
    MOZ_DIAGNOSTIC_ASSERT(mLoadInfo.mServiceWorkerDescriptor.isSome());
    return mLoadInfo.mServiceWorkerDescriptor.ref();
  }

  const ServiceWorkerRegistrationDescriptor&
  GetServiceWorkerRegistrationDescriptor() const {
    MOZ_DIAGNOSTIC_ASSERT(IsServiceWorker());
    MOZ_DIAGNOSTIC_ASSERT(
        mLoadInfo.mServiceWorkerRegistrationDescriptor.isSome());
    return mLoadInfo.mServiceWorkerRegistrationDescriptor.ref();
  }

  void UpdateServiceWorkerState(ServiceWorkerState aState) {
    MOZ_DIAGNOSTIC_ASSERT(IsServiceWorker());
    MOZ_DIAGNOSTIC_ASSERT(mLoadInfo.mServiceWorkerDescriptor.isSome());
    return mLoadInfo.mServiceWorkerDescriptor.ref().SetState(aState);
  }

  const Maybe<ServiceWorkerDescriptor>& GetParentController() const {
    return mLoadInfo.mParentController;
  }

  const ChannelInfo& GetChannelInfo() const { return mLoadInfo.mChannelInfo; }

  void SetChannelInfo(const ChannelInfo& aChannelInfo) {
    AssertIsOnMainThread();
    MOZ_ASSERT(!mLoadInfo.mChannelInfo.IsInitialized());
    MOZ_ASSERT(aChannelInfo.IsInitialized());
    mLoadInfo.mChannelInfo = aChannelInfo;
  }

  void InitChannelInfo(nsIChannel* aChannel) {
    mLoadInfo.mChannelInfo.InitFromChannel(aChannel);
  }

  void InitChannelInfo(const ChannelInfo& aChannelInfo) {
    mLoadInfo.mChannelInfo = aChannelInfo;
  }

  nsIPrincipal* GetPrincipal() const {
    AssertIsOnMainThread();
    return mLoadInfo.mPrincipal;
  }

  nsIPrincipal* GetEffectiveStoragePrincipal() const {
    AssertIsOnMainThread();
    return mLoadInfo.mStoragePrincipal;
  }

  nsIPrincipal* GetLoadingPrincipal() const {
    AssertIsOnMainThread();
    return mLoadInfo.mLoadingPrincipal;
  }

  const nsAString& Origin() const { return mLoadInfo.mOrigin; }

  nsILoadGroup* GetLoadGroup() const {
    AssertIsOnMainThread();
    return mLoadInfo.mLoadGroup;
  }

  bool UsesSystemPrincipal() const { return mLoadInfo.mPrincipalIsSystem; }

  const mozilla::ipc::PrincipalInfo& GetPrincipalInfo() const {
    return *mLoadInfo.mPrincipalInfo;
  }

  // The CSPInfo returned is the same CSP as stored inside the Principal
  // returned from GetPrincipalInfo. Please note that after Bug 965637
  // we do not have a a CSP stored inside the Principal anymore which
  // allows us to clean that part up.
  const nsTArray<mozilla::ipc::ContentSecurityPolicy>& GetCSPInfos() const {
    return mLoadInfo.mCSPInfos;
  }

  const mozilla::ipc::PrincipalInfo& GetEffectiveStoragePrincipalInfo() const {
    return *mLoadInfo.mStoragePrincipalInfo;
  }

  already_AddRefed<nsIChannel> ForgetWorkerChannel() {
    AssertIsOnMainThread();
    return mLoadInfo.mChannel.forget();
  }

  nsPIDOMWindowInner* GetWindow() {
    AssertIsOnMainThread();
    return mLoadInfo.mWindow;
  }

  nsIContentSecurityPolicy* GetCSP() const {
    AssertIsOnMainThread();
    return mLoadInfo.mCSP;
  }

  void SetCSP(nsIContentSecurityPolicy* aCSP);

  nsresult SetCSPFromHeaderValues(const nsACString& aCSPHeaderValue,
                                  const nsACString& aCSPReportOnlyHeaderValue);

  void SetReferrerPolicyFromHeaderValue(
      const nsACString& aReferrerPolicyHeaderValue);

  net::ReferrerPolicy GetReferrerPolicy() const {
    return mLoadInfo.mReferrerPolicy;
  }

  void SetReferrerPolicy(net::ReferrerPolicy aReferrerPolicy) {
    mLoadInfo.mReferrerPolicy = aReferrerPolicy;
  }

  bool IsEvalAllowed() const { return mLoadInfo.mEvalAllowed; }

  void SetEvalAllowed(bool aEvalAllowed) {
    mLoadInfo.mEvalAllowed = aEvalAllowed;
  }

  bool GetReportCSPViolations() const { return mLoadInfo.mReportCSPViolations; }

  void SetReportCSPViolations(bool aReport) {
    mLoadInfo.mReportCSPViolations = aReport;
  }

  bool XHRParamsAllowed() const { return mLoadInfo.mXHRParamsAllowed; }

  void SetXHRParamsAllowed(bool aAllowed) {
    mLoadInfo.mXHRParamsAllowed = aAllowed;
  }

  nsContentUtils::StorageAccess StorageAccess() const {
    AssertIsOnWorkerThread();
    if (mLoadInfo.mFirstPartyStorageAccessGranted) {
      return nsContentUtils::StorageAccess::eAllow;
    }

    return mLoadInfo.mStorageAccess;
  }

  nsICookieSettings* CookieSettings() const {
    // Any thread.
    MOZ_ASSERT(mLoadInfo.mCookieSettings);
    return mLoadInfo.mCookieSettings;
  }

  const OriginAttributes& GetOriginAttributes() const {
    return mLoadInfo.mOriginAttributes;
  }

  // Determine if the SW testing per-window flag is set by devtools
  bool ServiceWorkersTestingInWindow() const {
    return mLoadInfo.mServiceWorkersTestingInWindow;
  }

  bool IsWatchedByDevtools() const {
    return mLoadInfo.mWatchedByDevtools;
  }

  // Determine if the worker is currently loading its top level script.
  bool IsLoadingWorkerScript() const { return mLoadingWorkerScript; }

  // Called by ScriptLoader to track when this worker is loading its
  // top level script.
  void SetLoadingWorkerScript(bool aLoadingWorkerScript) {
    // any thread
    mLoadingWorkerScript = aLoadingWorkerScript;
  }

  RemoteWorkerChild* GetRemoteWorkerController();

  void SetRemoteWorkerController(RemoteWorkerChild* aController);

  // We can assume that an nsPIDOMWindow will be available for Freeze, Thaw
  // as these are only used for globals going in and out of the bfcache.
  bool Freeze(nsPIDOMWindowInner* aWindow);

  bool Thaw(nsPIDOMWindowInner* aWindow);

  void PropagateFirstPartyStorageAccessGranted();

  void EnableDebugger();

  void DisableDebugger();

  already_AddRefed<WorkerRunnable> MaybeWrapAsWorkerRunnable(
      already_AddRefed<nsIRunnable> aRunnable);

  bool ProxyReleaseMainThreadObjects();

  void GarbageCollect(bool aShrinking);

  void CycleCollect(bool aDummy);

  nsresult SetPrincipalsOnMainThread(nsIPrincipal* aPrincipal,
                                     nsIPrincipal* aStoragePrincipal,
                                     nsILoadGroup* aLoadGroup);

  nsresult SetPrincipalsFromChannel(nsIChannel* aChannel);

  bool FinalChannelPrincipalIsValid(nsIChannel* aChannel);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool PrincipalURIMatchesScriptURL();
#endif

  void UpdateOverridenLoadGroup(nsILoadGroup* aBaseLoadGroup);

  void WorkerScriptLoaded();

  Document* GetDocument() const;

  void MemoryPressure(bool aDummy);

  void UpdateContextOptions(const JS::ContextOptions& aContextOptions);

  void UpdateLanguages(const nsTArray<nsString>& aLanguages);

  void UpdateJSWorkerMemoryParameter(JSGCParamKey key, uint32_t value);

#ifdef JS_GC_ZEAL
  void UpdateGCZeal(uint8_t aGCZeal, uint32_t aFrequency);
#endif

  void OfflineStatusChangeEvent(bool aIsOffline);

  nsresult Dispatch(already_AddRefed<WorkerRunnable> aRunnable,
                    nsIEventTarget* aSyncLoopTarget = nullptr);

  nsresult DispatchControlRunnable(
      already_AddRefed<WorkerControlRunnable> aWorkerControlRunnable);

  nsresult DispatchDebuggerRunnable(
      already_AddRefed<WorkerRunnable> aDebuggerRunnable);

#ifdef DEBUG
  void AssertIsOnParentThread() const;

  void AssertInnerWindowIsCorrect() const;
#else
  void AssertIsOnParentThread() const {}

  void AssertInnerWindowIsCorrect() const {}
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool PrincipalIsValid() const;
#endif

  void StartCancelingTimer();

  nsAString& Id();

 private:
  WorkerPrivate(WorkerPrivate* aParent, const nsAString& aScriptURL,
                bool aIsChromeWorker, WorkerType aWorkerType,
                const nsAString& aWorkerName,
                const nsACString& aServiceWorkerScope,
                WorkerLoadInfo& aLoadInfo);

  ~WorkerPrivate();

  bool MayContinueRunning() {
    AssertIsOnWorkerThread();

    WorkerStatus status;
    {
      MutexAutoLock lock(mMutex);
      status = mStatus;
    }

    if (status < Canceling) {
      return true;
    }

    return false;
  }

  void CancelAllTimeouts();

  enum class ProcessAllControlRunnablesResult {
    // We did not process anything.
    Nothing,
    // We did process something, states may have changed, but we can keep
    // executing script.
    MayContinue,
    // We did process something, and should not continue executing script.
    Abort
  };

  ProcessAllControlRunnablesResult ProcessAllControlRunnables() {
    MutexAutoLock lock(mMutex);
    return ProcessAllControlRunnablesLocked();
  }

  ProcessAllControlRunnablesResult ProcessAllControlRunnablesLocked();

  void EnableMemoryReporter();

  void DisableMemoryReporter();

  void WaitForWorkerEvents();

  // If the worker shutdown status is equal or greater then aFailStatus, this
  // operation will fail and nullptr will be returned. See WorkerHolder.h for
  // more information about the correct value to use.
  already_AddRefed<nsIEventTarget> CreateNewSyncLoop(WorkerStatus aFailStatus);

  bool RunCurrentSyncLoop();

  bool DestroySyncLoop(uint32_t aLoopIndex);

  void InitializeGCTimers();

  enum GCTimerMode { PeriodicTimer = 0, IdleTimer, NoTimer };

  void SetGCTimerMode(GCTimerMode aMode);

  void ShutdownGCTimers();

  bool AddHolder(WorkerHolder* aHolder, WorkerStatus aFailStatus);

  void RemoveHolder(WorkerHolder* aHolder);

  void NotifyHolders(WorkerStatus aStatus);

  bool HasActiveHolders() {
    MOZ_ACCESS_THREAD_BOUND(mWorkerThreadAccessible, data);
    return !(data->mChildWorkers.IsEmpty() && data->mTimeouts.IsEmpty() &&
             data->mHolders.IsEmpty());
  }

  // Internal logic to dispatch a runnable. This is separate from Dispatch()
  // to allow runnables to be atomically dispatched in bulk.
  nsresult DispatchLockHeld(already_AddRefed<WorkerRunnable> aRunnable,
                            nsIEventTarget* aSyncLoopTarget,
                            const MutexAutoLock& aProofOfLock);

  // This method dispatches a simple runnable that starts the shutdown procedure
  // after a self.close(). This method is called after a ClearMainEventQueue()
  // to be sure that the canceling runnable is the only one in the queue.  We
  // need this async operation to be sure that all the current JS code is
  // executed.
  void DispatchCancelingRunnable();

  class EventTarget;
  friend class EventTarget;
  friend class mozilla::dom::WorkerHolder;
  friend class AutoSyncLoopHolder;

  struct TimeoutInfo;

  class MemoryReporter;
  friend class MemoryReporter;

  friend class mozilla::dom::WorkerThread;

  SharedMutex mMutex;
  mozilla::CondVar mCondVar;

  WorkerPrivate* mParent;

  nsString mScriptURL;

  // This is the worker name for shared workers and dedicated workers.
  nsString mWorkerName;

  WorkerType mWorkerType;

  // The worker is owned by its thread, which is represented here.  This is set
  // in Constructor() and emptied by WorkerFinishedRunnable, and conditionally
  // traversed by the cycle collector if the busy count is zero.
  //
  // There are 4 ways a worker can be terminated:
  // 1. GC/CC - When the worker is in idle state (busycount == 0), it allows to
  //    traverse the 'hidden' mParentEventTargetRef pointer. This is the exposed
  //    Worker webidl object. Doing this, CC will be able to detect a cycle and
  //    Unlink is called. In Unlink, Worker calls Cancel().
  // 2. Worker::Cancel() is called - the shutdown procedure starts immediately.
  // 3. WorkerScope::Close() is called - Similar to point 2.
  // 4. xpcom-shutdown notification - We call Kill().
  RefPtr<Worker> mParentEventTargetRef;
  RefPtr<WorkerPrivate> mSelfRef;

  // The lifetime of these objects within LoadInfo is managed explicitly;
  // they do not need to be cycle collected.
  WorkerLoadInfo mLoadInfo;
  LocationInfo mLocationInfo;

  // Protected by mMutex.
  workerinternals::JSSettings mJSSettings;

  WorkerDebugger* mDebugger;

  workerinternals::Queue<WorkerControlRunnable*, 4> mControlQueue;
  workerinternals::Queue<WorkerRunnable*, 4> mDebuggerQueue;

  // Touched on multiple threads, protected with mMutex.
  JSContext* mJSContext;
  RefPtr<WorkerThread> mThread;
  PRThread* mPRThread;

  // Accessed from main thread
  RefPtr<ThrottledEventQueue> mMainThreadEventTargetForMessaging;
  RefPtr<ThrottledEventQueue> mMainThreadEventTarget;

  // Accessed from worker thread and destructing thread
  RefPtr<WorkerEventTarget> mWorkerControlEventTarget;
  RefPtr<WorkerEventTarget> mWorkerHybridEventTarget;

  // A pauseable queue for WorkerDebuggeeRunnables directed at the main thread.
  // See WorkerDebuggeeRunnable for details.
  RefPtr<ThrottledEventQueue> mMainThreadDebuggeeEventTarget;

  struct SyncLoopInfo {
    explicit SyncLoopInfo(EventTarget* aEventTarget);

    RefPtr<EventTarget> mEventTarget;
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

  nsCOMPtr<nsITimer> mCancelingTimer;

  // fired on the main thread if the worker script fails to load
  nsCOMPtr<nsIRunnable> mLoadFailedRunnable;

  RefPtr<PerformanceStorage> mPerformanceStorage;

  RefPtr<WorkerCSPEventListener> mCSPEventListener;

  // Protected by mMutex.
  nsTArray<RefPtr<WorkerRunnable>> mPreStartRunnables;

  // Only touched on the parent thread. This is set only if IsSharedWorker().
  RefPtr<RemoteWorkerChild> mRemoteWorkerController;

  JS::UniqueChars mDefaultLocale;  // nulled during worker JSContext init
  TimeStamp mKillTime;
  WorkerStatus mParentStatus;
  WorkerStatus mStatus;

  // This is touched on parent thread only, but it can be read on a different
  // thread before crashing because hanging.
  Atomic<uint64_t> mBusyCount;

  Atomic<bool> mLoadingWorkerScript;

  TimeStamp mCreationTimeStamp;
  DOMHighResTimeStamp mCreationTimeHighRes;

  // Things touched on worker thread only.
  struct WorkerThreadAccessible {
    explicit WorkerThreadAccessible(WorkerPrivate* aParent);

    RefPtr<WorkerGlobalScope> mScope;
    RefPtr<WorkerDebuggerGlobalScope> mDebuggerScope;
    nsTArray<WorkerPrivate*> mChildWorkers;
    nsTObserverArray<WorkerHolder*> mHolders;
    nsTArray<nsAutoPtr<TimeoutInfo>> mTimeouts;

    nsCOMPtr<nsITimer> mTimer;
    nsCOMPtr<nsITimerCallback> mTimerRunnable;

    nsCOMPtr<nsITimer> mGCTimer;

    RefPtr<MemoryReporter> mMemoryReporter;

    UniquePtr<ClientSource> mClientSource;

    uint32_t mNumHoldersPreventingShutdownStart;
    uint32_t mDebuggerEventLoopLevel;

    uint32_t mErrorHandlerRecursionCount;
    uint32_t mNextTimeoutId;

    bool mFrozen;
    bool mTimerRunning;
    bool mRunningExpiredTimeouts;
    bool mPeriodicGCTimerRunning;
    bool mIdleGCTimerRunning;
    bool mOnLine;
  };
  ThreadBound<WorkerThreadAccessible> mWorkerThreadAccessible;

  uint32_t mPostSyncLoopOperations;

  // List of operations to do at the end of the last sync event loop.
  enum {
    ePendingEventQueueClearing = 0x01,
    eDispatchCancelingRunnable = 0x02,
  };

  bool mParentWindowPaused;

  bool mCancelAllPendingRunnables;
  bool mWorkerScriptExecutedSuccessfully;
  bool mFetchHandlerWasAdded;
  bool mMainThreadObjectsForgotten;
  bool mIsChromeWorker;
  bool mParentFrozen;

  // mIsSecureContext is set once in our constructor; after that it can be read
  // from various threads.
  //
  // It's a bit unfortunate that we have to have an out-of-band boolean for
  // this, but we need access to this state from the parent thread, and we can't
  // use our global object's secure state there.
  const bool mIsSecureContext;

  bool mDebuggerRegistered;

  // During registration, this worker may be marked as not being ready to
  // execute debuggee runnables or content.
  //
  // Protected by mMutex.
  bool mDebuggerReady;
  nsTArray<RefPtr<WorkerRunnable>> mDelayedDebuggeeRunnables;

  // mIsInAutomation is true when we're running in test automation.
  // We expose some extra testing functions in that case.
  bool mIsInAutomation;

  RefPtr<mozilla::PerformanceCounter> mPerformanceCounter;

  nsString mID;
};

class AutoSyncLoopHolder {
  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mTarget;
  uint32_t mIndex;

 public:
  // See CreateNewSyncLoop() for more information about the correct value to use
  // for aFailStatus.
  AutoSyncLoopHolder(WorkerPrivate* aWorkerPrivate, WorkerStatus aFailStatus)
      : mWorkerPrivate(aWorkerPrivate),
        mTarget(aWorkerPrivate->CreateNewSyncLoop(aFailStatus)),
        mIndex(aWorkerPrivate->mSyncLoopStack.Length() - 1) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  ~AutoSyncLoopHolder() {
    if (mWorkerPrivate && mTarget) {
      mWorkerPrivate->AssertIsOnWorkerThread();
      mWorkerPrivate->StopSyncLoop(mTarget, false);
      mWorkerPrivate->DestroySyncLoop(mIndex);
    }
  }

  bool Run() {
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate = nullptr;

    workerPrivate->AssertIsOnWorkerThread();

    return workerPrivate->RunCurrentSyncLoop();
  }

  nsIEventTarget* GetEventTarget() const {
    // This can be null if CreateNewSyncLoop() fails.
    return mTarget;
  }
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_workerprivate_h__ */
