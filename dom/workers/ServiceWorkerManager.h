/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkermanager_h
#define mozilla_dom_workers_serviceworkermanager_h

#include "nsIServiceWorkerManager.h"
#include "nsCOMPtr.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState
#include "mozilla/dom/ServiceWorkerCommon.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsRefPtrHashtable.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTObserverArray.h"

class mozIApplicationClearPrivateDataParams;

namespace mozilla {

class PrincipalOriginAttributes;

namespace dom {

class ServiceWorkerRegistrationListener;

namespace workers {

class ServiceWorker;
class ServiceWorkerClientInfo;
class ServiceWorkerInfo;
class ServiceWorkerJob;
class ServiceWorkerJobQueue;
class ServiceWorkerManagerChild;
class ServiceWorkerPrivate;

class ServiceWorkerRegistrationInfo final
  : public nsIServiceWorkerRegistrationInfo
{
  uint32_t mControlledDocumentsCounter;

  enum
  {
    NoUpdate,
    NeedTimeCheckAndUpdate,
    NeedUpdate
  } mUpdateState;

  uint64_t mLastUpdateCheckTime;

  virtual ~ServiceWorkerRegistrationInfo();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERREGISTRATIONINFO

  nsCString mScope;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  RefPtr<ServiceWorkerInfo> mActiveWorker;
  RefPtr<ServiceWorkerInfo> mWaitingWorker;
  RefPtr<ServiceWorkerInfo> mInstallingWorker;

  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> mListeners;

  // According to the spec, Soft Update shouldn't queue an update job
  // if the registration queue is not empty. Because our job queue
  // works slightly different, we use a flag to determine if the registration
  // is already updating.
  bool mUpdating;

  // When unregister() is called on a registration, it is not immediately
  // removed since documents may be controlled. It is marked as
  // pendingUninstall and when all controlling documents go away, removed.
  bool mPendingUninstall;

  ServiceWorkerRegistrationInfo(const nsACString& aScope,
                                nsIPrincipal* aPrincipal);

  already_AddRefed<ServiceWorkerInfo>
  Newest() const
  {
    RefPtr<ServiceWorkerInfo> newest;
    if (mInstallingWorker) {
      newest = mInstallingWorker;
    } else if (mWaitingWorker) {
      newest = mWaitingWorker;
    } else {
      newest = mActiveWorker;
    }

    return newest.forget();
  }

  already_AddRefed<ServiceWorkerInfo>
  GetServiceWorkerInfoById(uint64_t aId);

  void
  StartControllingADocument()
  {
    ++mControlledDocumentsCounter;
  }

  void
  StopControllingADocument()
  {
    MOZ_ASSERT(mControlledDocumentsCounter);
    --mControlledDocumentsCounter;
  }

  bool
  IsControllingDocuments() const
  {
    return mActiveWorker && mControlledDocumentsCounter;
  }

  void
  Clear();

  void
  PurgeActiveWorker();

  void
  TryToActivateAsync();

  void
  TryToActivate();

  void
  Activate();

  void
  FinishActivate(bool aSuccess);

  void
  RefreshLastUpdateCheckTime();

  bool
  IsLastUpdateCheckTimeOverOneDay() const;

  void
  NotifyListenersOnChange();

  void
  MaybeScheduleTimeCheckAndUpdate();

  void
  MaybeScheduleUpdate();

  bool
  CheckAndClearIfUpdateNeeded();
};

class ServiceWorkerUpdateFinishCallback
{
protected:
  virtual ~ServiceWorkerUpdateFinishCallback()
  {}

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateFinishCallback)

  virtual
  void UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo) = 0;

  virtual
  void UpdateFailed(ErrorResult& aStatus) = 0;
};

/*
 * Wherever the spec treats a worker instance and a description of said worker
 * as the same thing; i.e. "Resolve foo with
 * _GetNewestWorker(serviceWorkerRegistration)", we represent the description
 * by this class and spawn a ServiceWorker in the right global when required.
 */
class ServiceWorkerInfo final : public nsIServiceWorkerInfo
{
private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mScope;
  const nsCString mScriptSpec;
  const nsString mCacheName;
  ServiceWorkerState mState;

  // This id is shared with WorkerPrivate to match requests issued by service
  // workers to their corresponding serviceWorkerInfo.
  uint64_t mServiceWorkerID;

  // We hold rawptrs since the ServiceWorker constructor and destructor ensure
  // addition and removal.
  // There is a high chance of there being at least one ServiceWorker
  // associated with this all the time.
  AutoTArray<ServiceWorker*, 1> mInstances;

  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  bool mSkipWaitingFlag;

  ~ServiceWorkerInfo();

  // Generates a unique id for the service worker, with zero being treated as
  // invalid.
  uint64_t
  GetNextID() const;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERINFO

  class ServiceWorkerPrivate*
  WorkerPrivate() const
  {
    MOZ_ASSERT(mServiceWorkerPrivate);
    return mServiceWorkerPrivate;
  }

  nsIPrincipal*
  GetPrincipal() const
  {
    return mPrincipal;
  }

  const nsCString&
  ScriptSpec() const
  {
    return mScriptSpec;
  }

  const nsCString&
  Scope() const
  {
    return mScope;
  }

  bool SkipWaitingFlag() const
  {
    AssertIsOnMainThread();
    return mSkipWaitingFlag;
  }

  void SetSkipWaitingFlag()
  {
    AssertIsOnMainThread();
    mSkipWaitingFlag = true;
  }

  ServiceWorkerInfo(nsIPrincipal* aPrincipal,
                    const nsACString& aScope,
                    const nsACString& aScriptSpec,
                    const nsAString& aCacheName);

  ServiceWorkerState
  State() const
  {
    return mState;
  }

  const nsString&
  CacheName() const
  {
    return mCacheName;
  }

  uint64_t
  ID() const
  {
    return mServiceWorkerID;
  }

  void
  UpdateState(ServiceWorkerState aState);

  // Only used to set initial state when loading from disk!
  void
  SetActivateStateUncheckedWithoutEvent(ServiceWorkerState aState)
  {
    AssertIsOnMainThread();
    mState = aState;
  }

  void
  AppendWorker(ServiceWorker* aWorker);

  void
  RemoveWorker(ServiceWorker* aWorker);

  already_AddRefed<ServiceWorker>
  GetOrCreateInstance(nsPIDOMWindowInner* aWindow);
};

#define NS_SERVICEWORKERMANAGER_IMPL_IID                 \
{ /* f4f8755a-69ca-46e8-a65d-775745535990 */             \
  0xf4f8755a,                                            \
  0x69ca,                                                \
  0x46e8,                                                \
  { 0xa6, 0x5d, 0x77, 0x57, 0x45, 0x53, 0x59, 0x90 }     \
}

/*
 * The ServiceWorkerManager is a per-process global that deals with the
 * installation, querying and event dispatch of ServiceWorkers for all the
 * origins in the process.
 */
class ServiceWorkerManager final
  : public nsIServiceWorkerManager
  , public nsIIPCBackgroundChildCreateCallback
  , public nsIObserver
{
  friend class GetReadyPromiseRunnable;
  friend class GetRegistrationsRunnable;
  friend class GetRegistrationRunnable;
  friend class ServiceWorkerJobQueue;
  friend class ServiceWorkerInstallJob;
  friend class ServiceWorkerRegisterJob;
  friend class ServiceWorkerJobBase;
  friend class ServiceWorkerScriptJobBase;
  friend class ServiceWorkerRegistrationInfo;
  friend class ServiceWorkerUnregisterJob;
  friend class UpdateTimerCallback;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERMANAGER
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK
  NS_DECL_NSIOBSERVER

  struct RegistrationDataPerPrincipal;
  nsClassHashtable<nsCStringHashKey, RegistrationDataPerPrincipal> mRegistrationInfos;

  nsTObserverArray<ServiceWorkerRegistrationListener*> mServiceWorkerRegistrationListeners;

  nsRefPtrHashtable<nsISupportsHashKey, ServiceWorkerRegistrationInfo> mControlledDocuments;

  // Set of all documents that may be controlled by a service worker.
  nsTHashtable<nsISupportsHashKey> mAllDocuments;

  // Track all documents that have attempted to register a service worker for a
  // given scope.
  typedef nsTArray<nsCOMPtr<nsIWeakReference>> WeakDocumentList;
  nsClassHashtable<nsCStringHashKey, WeakDocumentList> mRegisteringDocuments;

  // Track all intercepted navigation channels for a given scope.  Channels are
  // placed in the appropriate list before dispatch the FetchEvent to the worker
  // thread and removed once FetchEvent processing dispatches back to the main
  // thread.
  //
  // Note: Its safe to use weak references here because a RAII-style callback
  //       is registered with the channel before its added to this list.  We
  //       are guaranteed the callback will fire before and remove the ref
  //       from this list before the channel is destroyed.
  typedef nsTArray<nsIInterceptedChannel*> InterceptionList;
  nsClassHashtable<nsCStringHashKey, InterceptionList> mNavigationInterceptions;

  bool
  IsAvailable(nsIPrincipal* aPrincipal, nsIURI* aURI);

  bool
  IsControlled(nsIDocument* aDocument, ErrorResult& aRv);

  void
  DispatchFetchEvent(const PrincipalOriginAttributes& aOriginAttributes,
                     nsIDocument* aDoc,
                     const nsAString& aDocumentIdForTopLevelNavigation,
                     nsIInterceptedChannel* aChannel,
                     bool aIsReload,
                     bool aIsSubresourceLoad,
                     ErrorResult& aRv);

  void
  Update(nsIPrincipal* aPrincipal,
         const nsACString& aScope,
         ServiceWorkerUpdateFinishCallback* aCallback);

  void
  SoftUpdate(const PrincipalOriginAttributes& aOriginAttributes,
             const nsACString& aScope);

  void
  PropagateSoftUpdate(const PrincipalOriginAttributes& aOriginAttributes,
                      const nsAString& aScope);

  void
  PropagateRemove(const nsACString& aHost);

  void
  Remove(const nsACString& aHost);

  void
  PropagateRemoveAll();

  void
  RemoveAll();

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetRegistration(nsIPrincipal* aPrincipal, const nsACString& aScope) const;

  ServiceWorkerRegistrationInfo*
  CreateNewRegistration(const nsCString& aScope, nsIPrincipal* aPrincipal);

  void
  RemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration);

  void StoreRegistration(nsIPrincipal* aPrincipal,
                         ServiceWorkerRegistrationInfo* aRegistration);

  void
  FinishFetch(ServiceWorkerRegistrationInfo* aRegistration);

  void
  ReportToAllClients(const nsCString& aScope,
                     const nsString& aMessage,
                     const nsString& aFilename,
                     const nsString& aLine,
                     uint32_t aLineNumber,
                     uint32_t aColumnNumber,
                     uint32_t aFlags);

  // Always consumes the error by reporting to consoles of all controlled
  // documents.
  void
  HandleError(JSContext* aCx,
              nsIPrincipal* aPrincipal,
              const nsCString& aScope,
              const nsString& aWorkerURL,
              const nsString& aMessage,
              const nsString& aFilename,
              const nsString& aLine,
              uint32_t aLineNumber,
              uint32_t aColumnNumber,
              uint32_t aFlags,
              JSExnType aExnType);

  UniquePtr<ServiceWorkerClientInfo>
  GetClient(nsIPrincipal* aPrincipal,
            const nsAString& aClientId,
            ErrorResult& aRv);

  void
  GetAllClients(nsIPrincipal* aPrincipal,
                const nsCString& aScope,
                bool aIncludeUncontrolled,
                nsTArray<ServiceWorkerClientInfo>& aDocuments);

  void
  MaybeClaimClient(nsIDocument* aDocument,
                   ServiceWorkerRegistrationInfo* aWorkerRegistration);

  nsresult
  ClaimClients(nsIPrincipal* aPrincipal, const nsCString& aScope, uint64_t aId);

  nsresult
  SetSkipWaitingFlag(nsIPrincipal* aPrincipal, const nsCString& aScope,
                     uint64_t aServiceWorkerID);

  static already_AddRefed<ServiceWorkerManager>
  GetInstance();

 void
 LoadRegistration(const ServiceWorkerRegistrationData& aRegistration);

  void
  LoadRegistrations(const nsTArray<ServiceWorkerRegistrationData>& aRegistrations);

  // Used by remove() and removeAll() when clearing history.
  // MUST ONLY BE CALLED FROM UnregisterIfMatchesHost!
  void
  ForceUnregister(RegistrationDataPerPrincipal* aRegistrationData,
                  ServiceWorkerRegistrationInfo* aRegistration);

  NS_IMETHOD
  AddRegistrationEventListener(const nsAString& aScope,
                               ServiceWorkerRegistrationListener* aListener);

  NS_IMETHOD
  RemoveRegistrationEventListener(const nsAString& aScope,
                                  ServiceWorkerRegistrationListener* aListener);

  void
  MaybeCheckNavigationUpdate(nsIDocument* aDoc);

  nsresult
  SendPushEvent(const nsACString& aOriginAttributes,
                const nsACString& aScope,
                const nsAString& aMessageId,
                const Maybe<nsTArray<uint8_t>>& aData);

  nsresult
  NotifyUnregister(nsIPrincipal* aPrincipal, const nsAString& aScope);

private:
  ServiceWorkerManager();
  ~ServiceWorkerManager();

  void
  Init();

  ServiceWorkerJobQueue*
  GetOrCreateJobQueue(const nsACString& aOriginSuffix,
                      const nsACString& aScope);

  void
  MaybeRemoveRegistrationInfo(const nsACString& aScopeKey);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetRegistration(const nsACString& aScopeKey,
                  const nsACString& aScope) const;

  void
  AbortCurrentUpdate(ServiceWorkerRegistrationInfo* aRegistration);

  nsresult
  Update(ServiceWorkerRegistrationInfo* aRegistration);

  nsresult
  GetDocumentRegistration(nsIDocument* aDoc,
                          ServiceWorkerRegistrationInfo** aRegistrationInfo);

  nsresult
  GetServiceWorkerForScope(nsPIDOMWindowInner* aWindow,
                           const nsAString& aScope,
                           WhichServiceWorker aWhichWorker,
                           nsISupports** aServiceWorker);

  ServiceWorkerInfo*
  GetActiveWorkerInfoForScope(const PrincipalOriginAttributes& aOriginAttributes,
                              const nsACString& aScope);

  ServiceWorkerInfo*
  GetActiveWorkerInfoForDocument(nsIDocument* aDocument);

  void
  InvalidateServiceWorkerRegistrationWorker(ServiceWorkerRegistrationInfo* aRegistration,
                                            WhichServiceWorker aWhichOnes);

  void
  NotifyServiceWorkerRegistrationRemoved(ServiceWorkerRegistrationInfo* aRegistration);

  void
  StartControllingADocument(ServiceWorkerRegistrationInfo* aRegistration,
                            nsIDocument* aDoc,
                            const nsAString& aDocumentId);

  void
  StopControllingADocument(ServiceWorkerRegistrationInfo* aRegistration);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsPIDOMWindowInner* aWindow);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsIDocument* aDoc);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsIPrincipal* aPrincipal, nsIURI* aURI);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(const nsACString& aScopeKey,
                                   nsIURI* aURI);

  // This method generates a key using appId and isInElementBrowser from the
  // principal. We don't use the origin because it can change during the
  // loading.
  static nsresult
  PrincipalToScopeKey(nsIPrincipal* aPrincipal, nsACString& aKey);

  static void
  AddScopeAndRegistration(const nsACString& aScope,
                          ServiceWorkerRegistrationInfo* aRegistation);

  static bool
  FindScopeForPath(const nsACString& aScopeKey,
                   const nsACString& aPath,
                   RegistrationDataPerPrincipal** aData, nsACString& aMatch);

  static bool
  HasScope(nsIPrincipal* aPrincipal, const nsACString& aScope);

  static void
  RemoveScopeAndRegistration(ServiceWorkerRegistrationInfo* aRegistration);

  void
  QueueFireEventOnServiceWorkerRegistrations(ServiceWorkerRegistrationInfo* aRegistration,
                                             const nsAString& aName);

  void
  FireUpdateFoundOnServiceWorkerRegistrations(ServiceWorkerRegistrationInfo* aRegistration);

  void
  FireControllerChange(ServiceWorkerRegistrationInfo* aRegistration);

  void
  StorePendingReadyPromise(nsPIDOMWindowInner* aWindow, nsIURI* aURI,
                           Promise* aPromise);

  void
  CheckPendingReadyPromises();

  bool
  CheckReadyPromise(nsPIDOMWindowInner* aWindow, nsIURI* aURI,
                    Promise* aPromise);

  struct PendingReadyPromise final
  {
    PendingReadyPromise(nsIURI* aURI, Promise* aPromise)
      : mURI(aURI), mPromise(aPromise)
    {}

    nsCOMPtr<nsIURI> mURI;
    RefPtr<Promise> mPromise;
  };

  void AppendPendingOperation(nsIRunnable* aRunnable);
  void AppendPendingOperation(ServiceWorkerJobQueue* aQueue,
                              ServiceWorkerJob* aJob);

  bool HasBackgroundActor() const
  {
    return !!mActor;
  }

  nsClassHashtable<nsISupportsHashKey, PendingReadyPromise> mPendingReadyPromises;

  void
  MaybeRemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration);

  // Removes all service worker registrations that matches the given pattern.
  void
  RemoveAllRegistrations(OriginAttributesPattern* aPattern);

  RefPtr<ServiceWorkerManagerChild> mActor;

  struct PendingOperation;
  nsTArray<PendingOperation> mPendingOperations;

  bool mShuttingDown;

  nsTArray<nsCOMPtr<nsIServiceWorkerManagerListener>> mListeners;

  void
  NotifyListenersOnRegister(nsIServiceWorkerRegistrationInfo* aRegistration);

  void
  NotifyListenersOnUnregister(nsIServiceWorkerRegistrationInfo* aRegistration);

  void
  AddRegisteringDocument(const nsACString& aScope, nsIDocument* aDoc);

  class InterceptionReleaseHandle;

  void
  AddNavigationInterception(const nsACString& aScope,
                            nsIInterceptedChannel* aChannel);

  void
  RemoveNavigationInterception(const nsACString& aScope,
                               nsIInterceptedChannel* aChannel);

  void
  ScheduleUpdateTimer(nsIPrincipal* aPrincipal, const nsACString& aScope);

  void
  UpdateTimerFired(nsIPrincipal* aPrincipal, const nsACString& aScope);
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkermanager_h
