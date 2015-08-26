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
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState
#include "mozilla/dom/ServiceWorkerCommon.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTObserverArray.h"

class mozIApplicationClearPrivateDataParams;

namespace mozilla {

class OriginAttributes;

namespace dom {

class ServiceWorkerRegistrationListener;

namespace workers {

class ServiceWorker;
class ServiceWorkerClientInfo;
class ServiceWorkerInfo;
class ServiceWorkerJob;
class ServiceWorkerJobQueue;
class ServiceWorkerManagerChild;

// Needs to inherit from nsISupports because NS_ProxyRelease() does not support
// non-ISupports classes.
class ServiceWorkerRegistrationInfo final : public nsISupports
{
  uint32_t mControlledDocumentsCounter;

  virtual ~ServiceWorkerRegistrationInfo();

public:
  NS_DECL_ISUPPORTS

  nsCString mScope;
  // The scriptURL for the registration. This may be completely different from
  // the URLs of the following three workers.
  nsCString mScriptSpec;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsRefPtr<ServiceWorkerInfo> mActiveWorker;
  nsRefPtr<ServiceWorkerInfo> mWaitingWorker;
  nsRefPtr<ServiceWorkerInfo> mInstallingWorker;

  // When unregister() is called on a registration, it is not immediately
  // removed since documents may be controlled. It is marked as
  // pendingUninstall and when all controlling documents go away, removed.
  bool mPendingUninstall;

  ServiceWorkerRegistrationInfo(const nsACString& aScope,
                                nsIPrincipal* aPrincipal);

  already_AddRefed<ServiceWorkerInfo>
  Newest()
  {
    nsRefPtr<ServiceWorkerInfo> newest;
    if (mInstallingWorker) {
      newest = mInstallingWorker;
    } else if (mWaitingWorker) {
      newest = mWaitingWorker;
    } else {
      newest = mActiveWorker;
    }

    return newest.forget();
  }

  void
  StartControllingADocument()
  {
    ++mControlledDocumentsCounter;
  }

  void
  StopControllingADocument()
  {
    --mControlledDocumentsCounter;
  }

  bool
  IsControllingDocuments() const
  {
    return mActiveWorker && mControlledDocumentsCounter > 0;
  }

  void
  Clear();

  void
  PurgeActiveWorker();

  void
  TryToActivate();

  void
  Activate();

  void
  FinishActivate(bool aSuccess);

};

class ServiceWorkerUpdateFinishCallback
{
protected:
  virtual ~ServiceWorkerUpdateFinishCallback()
  { }

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerUpdateFinishCallback)

  virtual
  void UpdateSucceeded(ServiceWorkerRegistrationInfo* aInfo)
  { }

  virtual
  void UpdateFailed(nsresult aStatus)
  { }

  virtual
  void UpdateFailed(JSExnType aExnType, const ErrorEventInit& aDesc)
  { }
};

/*
 * Wherever the spec treats a worker instance and a description of said worker
 * as the same thing; i.e. "Resolve foo with
 * _GetNewestWorker(serviceWorkerRegistration)", we represent the description
 * by this class and spawn a ServiceWorker in the right global when required.
 */
class ServiceWorkerInfo final
{
private:
  const ServiceWorkerRegistrationInfo* mRegistration;
  nsCString mScriptSpec;
  nsString mCacheName;
  ServiceWorkerState mState;

  // This id is shared with WorkerPrivate to match requests issued by service
  // workers to their corresponding serviceWorkerInfo.
  uint64_t mServiceWorkerID;

  // We hold rawptrs since the ServiceWorker constructor and destructor ensure
  // addition and removal.
  // There is a high chance of there being at least one ServiceWorker
  // associated with this all the time.
  nsAutoTArray<ServiceWorker*, 1> mInstances;
  bool mSkipWaitingFlag;

  ~ServiceWorkerInfo()
  { }

  // Generates a unique id for the service worker, with zero being treated as
  // invalid.
  uint64_t
  GetNextID() const;

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerInfo)

  const nsCString&
  ScriptSpec() const
  {
    return mScriptSpec;
  }

  const nsCString&
  Scope() const
  {
    return mRegistration->mScope;
  }

  void SetScriptSpec(const nsCString& aSpec)
  {
    MOZ_ASSERT(!aSpec.IsEmpty());
    mScriptSpec = aSpec;
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

  ServiceWorkerInfo(ServiceWorkerRegistrationInfo* aReg,
                    const nsACString& aScriptSpec,
                    const nsAString& aCacheName)
    : mRegistration(aReg)
    , mScriptSpec(aScriptSpec)
    , mCacheName(aCacheName)
    , mState(ServiceWorkerState::EndGuard_)
    , mServiceWorkerID(GetNextID())
    , mSkipWaitingFlag(false)
  {
    MOZ_ASSERT(mRegistration);
    MOZ_ASSERT(!aCacheName.IsEmpty());
  }

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
    mState = aState;
  }

  void
  AppendWorker(ServiceWorker* aWorker);

  void
  RemoveWorker(ServiceWorker* aWorker);
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
  friend class ServiceWorkerRegisterJob;
  friend class ServiceWorkerRegistrationInfo;
  friend class ServiceWorkerUnregisterJob;

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

  bool
  IsAvailable(const OriginAttributes& aOriginAttributes, nsIURI* aURI);

  bool
  IsControlled(nsIDocument* aDocument, ErrorResult& aRv);

  void
  DispatchFetchEvent(const OriginAttributes& aOriginAttributes,
                     nsIDocument* aDoc,
                     nsIInterceptedChannel* aChannel,
                     bool aIsReload,
                     ErrorResult& aRv);

  void
  SoftUpdate(nsIPrincipal* aPrincipal,
             const nsACString& aScope,
             ServiceWorkerUpdateFinishCallback* aCallback = nullptr);

  void
  SoftUpdate(const OriginAttributes& aOriginAttributes,
             const nsACString& aScope,
             ServiceWorkerUpdateFinishCallback* aCallback = nullptr);

  void
  PropagateSoftUpdate(const OriginAttributes& aOriginAttributes,
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

  // Returns true if the error was handled, false if normal worker error
  // handling should continue.
  bool
  HandleError(JSContext* aCx,
              nsIPrincipal* aPrincipal,
              const nsCString& aScope,
              const nsString& aWorkerURL,
              nsString aMessage,
              nsString aFilename,
              nsString aLine,
              uint32_t aLineNumber,
              uint32_t aColumnNumber,
              uint32_t aFlags,
              JSExnType aExnType);

  void
  GetAllClients(nsIPrincipal* aPrincipal,
                const nsCString& aScope,
                nsTArray<ServiceWorkerClientInfo>& aControlledDocuments);

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

  void
  SoftUpdate(const nsACString& aScopeKey,
             const nsACString& aScope,
             ServiceWorkerUpdateFinishCallback* aCallback = nullptr);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetRegistration(const nsACString& aScopeKey,
                  const nsACString& aScope) const;

  void
  AbortCurrentUpdate(ServiceWorkerRegistrationInfo* aRegistration);

  nsresult
  Update(ServiceWorkerRegistrationInfo* aRegistration);

  nsresult
  GetDocumentRegistration(nsIDocument* aDoc, ServiceWorkerRegistrationInfo** aRegistrationInfo);

  NS_IMETHOD
  CreateServiceWorkerForWindow(nsPIDOMWindow* aWindow,
                               ServiceWorkerInfo* aInfo,
                               nsIRunnable* aLoadFailedRunnable,
                               ServiceWorker** aServiceWorker);

  NS_IMETHOD
  CreateServiceWorker(nsIPrincipal* aPrincipal,
                      ServiceWorkerInfo* aInfo,
                      nsIRunnable* aLoadFailedRunnable,
                      ServiceWorker** aServiceWorker);

  NS_IMETHODIMP
  GetServiceWorkerForScope(nsIDOMWindow* aWindow,
                           const nsAString& aScope,
                           WhichServiceWorker aWhichWorker,
                           nsISupports** aServiceWorker);

  already_AddRefed<ServiceWorker>
  CreateServiceWorkerForScope(const OriginAttributes& aOriginAttributes,
                              const nsACString& aScope,
                              nsIRunnable* aLoadFailedRunnable);

  void
  InvalidateServiceWorkerRegistrationWorker(ServiceWorkerRegistrationInfo* aRegistration,
                                            WhichServiceWorker aWhichOnes);

  void
  StartControllingADocument(ServiceWorkerRegistrationInfo* aRegistration,
                            nsIDocument* aDoc);

  void
  StopControllingADocument(ServiceWorkerRegistrationInfo* aRegistration);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsPIDOMWindow* aWindow);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsIDocument* aDoc);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(nsIPrincipal* aPrincipal, nsIURI* aURI);

  already_AddRefed<ServiceWorkerRegistrationInfo>
  GetServiceWorkerRegistrationInfo(const OriginAttributes& aOriginAttributes,
                                   nsIURI* aURI);

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

#ifdef DEBUG
  static bool
  HasScope(nsIPrincipal* aPrincipal, const nsACString& aScope);
#endif

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
  StorePendingReadyPromise(nsPIDOMWindow* aWindow, nsIURI* aURI, Promise* aPromise);

  void
  CheckPendingReadyPromises();

  bool
  CheckReadyPromise(nsPIDOMWindow* aWindow, nsIURI* aURI, Promise* aPromise);

  struct PendingReadyPromise
  {
    PendingReadyPromise(nsIURI* aURI, Promise* aPromise)
      : mURI(aURI), mPromise(aPromise)
    { }

    nsCOMPtr<nsIURI> mURI;
    nsRefPtr<Promise> mPromise;
  };

  void AppendPendingOperation(nsIRunnable* aRunnable);
  void AppendPendingOperation(ServiceWorkerJobQueue* aQueue,
                              ServiceWorkerJob* aJob);

  bool HasBackgroundActor() const
  {
    return !!mActor;
  }

  static PLDHashOperator
  CheckPendingReadyPromisesEnumerator(nsISupports* aSupports,
                                      nsAutoPtr<PendingReadyPromise>& aData,
                                      void* aUnused);

  nsClassHashtable<nsISupportsHashKey, PendingReadyPromise> mPendingReadyPromises;

  void
  MaybeRemoveRegistration(ServiceWorkerRegistrationInfo* aRegistration);

  // Does all cleanup except removing the registration from
  // mServiceWorkerRegistrationInfos. This is useful when we clear
  // registrations via remove()/removeAll() since we are iterating over the
  // hashtable and can cleanly remove within the hashtable enumeration
  // function.
  void
  RemoveRegistrationInternal(ServiceWorkerRegistrationInfo* aRegistration);

  // Removes all service worker registrations that matches the given
  // mozIApplicationClearPrivateDataParams.
  void
  RemoveAllRegistrations(OriginAttributes* aParams);

  nsRefPtr<ServiceWorkerManagerChild> mActor;

  struct PendingOperation;
  nsTArray<PendingOperation> mPendingOperations;

  bool mShuttingDown;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkermanager_h
