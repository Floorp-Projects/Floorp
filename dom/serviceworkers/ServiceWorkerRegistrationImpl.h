/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Unused.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerRegistrationListener.h"

namespace mozilla {
namespace dom {

class Promise;
class PushManager;
class ServiceWorker;

////////////////////////////////////////////////////
// Main Thread implementation

class ServiceWorkerRegistrationMainThread final : public ServiceWorkerRegistration,
                                                  public ServiceWorkerRegistrationListener
{
  friend nsPIDOMWindowInner;
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationMainThread,
                                           ServiceWorkerRegistration)

  ServiceWorkerRegistrationMainThread(nsPIDOMWindowInner* aWindow,
                                      const nsAString& aScope);

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<ServiceWorker>
  GetInstalling() override;

  already_AddRefed<ServiceWorker>
  GetWaiting() override;

  already_AddRefed<ServiceWorker>
  GetActive() override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

  // DOMEventTargethelper
  void DisconnectFromOwner() override
  {
    StopListeningForEvents();
    ServiceWorkerRegistration::DisconnectFromOwner();
  }

  // ServiceWorkerRegistrationListener
  void
  UpdateFound() override;

  void
  UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor) override;

  void
  RegistrationRemoved() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

  ServiceWorkerUpdateViaCache
  GetUpdateViaCache(ErrorResult& aRv) const override
  {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);

    nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
    MOZ_ASSERT(window);

    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    MOZ_ASSERT(doc);

    nsCOMPtr<nsIServiceWorkerRegistrationInfo> registration;
    nsresult rv = swm->GetRegistrationByPrincipal(doc->NodePrincipal(), mScope,
                                                  getter_AddRefs(registration));

    /*
     *  xxx: We should probably store the `updateViaCache` value on the
     *  ServiceWorkerRegistration object and update it when necessary.
     *  We don't have a good way to reach all ServiceWorkerRegistration objects
     *  from the ServiceWorkerRegistratinInfo right now, though.
     *  This is a short term fix to avoid crashing.
     */
    if (NS_FAILED(rv) || !registration) {
      aRv = NS_ERROR_DOM_INVALID_STATE_ERR;
      return ServiceWorkerUpdateViaCache::None;
    }

    uint16_t updateViaCache;
    rv = registration->GetUpdateViaCache(&updateViaCache);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Silence possible compiler warnings.
    Unused << rv;

    return static_cast<ServiceWorkerUpdateViaCache>(updateViaCache);
  }

  bool
  MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) override;

private:
  ~ServiceWorkerRegistrationMainThread();

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  bool mListeningForEvents;

  // The following properties are cached here to ensure JS equality is satisfied
  // instead of acquiring a new worker instance from the ServiceWorkerManager
  // for every access. A null value is considered a cache miss.
  // These three may change to a new worker at any time.
  RefPtr<ServiceWorker> mInstallingWorker;
  RefPtr<ServiceWorker> mWaitingWorker;
  RefPtr<ServiceWorker> mActiveWorker;

  RefPtr<PushManager> mPushManager;
};

////////////////////////////////////////////////////
// Worker Thread implementation

class WorkerListener;

class ServiceWorkerRegistrationWorkerThread final : public ServiceWorkerRegistration
                                                  , public WorkerHolder
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                           ServiceWorkerRegistration)

  ServiceWorkerRegistrationWorkerThread(WorkerPrivate* aWorkerPrivate,
                                        const nsAString& aScope);

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<ServiceWorker>
  GetInstalling() override;

  already_AddRefed<ServiceWorker>
  GetWaiting() override;

  already_AddRefed<ServiceWorker>
  GetActive() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

  ServiceWorkerUpdateViaCache
  GetUpdateViaCache(ErrorResult& aRv) const override
  {
    // FIXME(hopang): Will be implemented after Bug 1113522.
    return ServiceWorkerUpdateViaCache::Imports;
  }

  bool
  Notify(WorkerStatus aStatus) override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

private:
  ~ServiceWorkerRegistrationWorkerThread();

  void
  InitListener();

  void
  ReleaseListener();

  WorkerPrivate* mWorkerPrivate;
  RefPtr<WorkerListener> mListener;

  RefPtr<PushManager> mPushManager;
};

} // dom namespace
} // mozilla namespace
