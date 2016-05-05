/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistration_h
#define mozilla_dom_ServiceWorkerRegistration_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerCommon.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"
#include "nsContentUtils.h" // Required for nsContentUtils::PushEnabled

// Support for Notification API extension.
#include "mozilla/dom/NotificationBinding.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Promise;
class PushManager;
class WorkerListener;

namespace workers {
class ServiceWorker;
class WorkerPrivate;
} // namespace workers

bool
ServiceWorkerRegistrationVisible(JSContext* aCx, JSObject* aObj);

bool
ServiceWorkerNotificationAPIVisible(JSContext* aCx, JSObject* aObj);

// Used by ServiceWorkerManager to notify ServiceWorkerRegistrations of
// updatefound event and invalidating ServiceWorker instances.
class ServiceWorkerRegistrationListener
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

  virtual void
  UpdateFound() = 0;

  virtual void
  InvalidateWorkers(WhichServiceWorker aWhichOnes) = 0;

  virtual void
  RegistrationRemoved() = 0;

  virtual void
  GetScope(nsAString& aScope) const = 0;
};

class ServiceWorkerRegistrationBase : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(updatefound)

  ServiceWorkerRegistrationBase(nsPIDOMWindowInner* aWindow,
                                const nsAString& aScope);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetInstalling() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetWaiting() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetActive() = 0;

protected:
  virtual ~ServiceWorkerRegistrationBase()
  { }

  const nsString mScope;
};

class ServiceWorkerRegistrationMainThread final : public ServiceWorkerRegistrationBase,
                                                  public ServiceWorkerRegistrationListener
{
  friend nsPIDOMWindowInner;
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationMainThread,
                                           ServiceWorkerRegistrationBase)

  already_AddRefed<Promise>
  Update(ErrorResult& aRv);

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv);

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<workers::ServiceWorker>
  GetInstalling() override;

  already_AddRefed<workers::ServiceWorker>
  GetWaiting() override;

  already_AddRefed<workers::ServiceWorker>
  GetActive() override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv);

  // DOMEventTargethelper
  void DisconnectFromOwner() override
  {
    StopListeningForEvents();
    ServiceWorkerRegistrationBase::DisconnectFromOwner();
  }

  // ServiceWorkerRegistrationListener
  void
  UpdateFound() override;

  void
  InvalidateWorkers(WhichServiceWorker aWhichOnes) override;

  void
  RegistrationRemoved() override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

private:
  ServiceWorkerRegistrationMainThread(nsPIDOMWindowInner* aWindow,
                                      const nsAString& aScope);
  ~ServiceWorkerRegistrationMainThread();

  already_AddRefed<workers::ServiceWorker>
  GetWorkerReference(WhichServiceWorker aWhichOne);

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  bool mListeningForEvents;

  // The following properties are cached here to ensure JS equality is satisfied
  // instead of acquiring a new worker instance from the ServiceWorkerManager
  // for every access. A null value is considered a cache miss.
  // These three may change to a new worker at any time.
  RefPtr<workers::ServiceWorker> mInstallingWorker;
  RefPtr<workers::ServiceWorker> mWaitingWorker;
  RefPtr<workers::ServiceWorker> mActiveWorker;

#ifndef MOZ_SIMPLEPUSH
  RefPtr<PushManager> mPushManager;
#endif
};

class ServiceWorkerRegistrationWorkerThread final : public ServiceWorkerRegistrationBase
                                                  , public workers::WorkerFeature
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                           ServiceWorkerRegistrationBase)

  ServiceWorkerRegistrationWorkerThread(workers::WorkerPrivate* aWorkerPrivate,
                                        const nsAString& aScope);

  already_AddRefed<Promise>
  Update(ErrorResult& aRv);

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Partial interface from Notification API.
  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv);

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<workers::ServiceWorker>
  GetInstalling() override;

  already_AddRefed<workers::ServiceWorker>
  GetWaiting() override;

  already_AddRefed<workers::ServiceWorker>
  GetActive() override;

  void
  GetScope(nsAString& aScope) const
  {
    aScope = mScope;
  }

  bool
  Notify(workers::Status aStatus) override;

  already_AddRefed<PushManager>
  GetPushManager(ErrorResult& aRv);

private:
  enum Reason
  {
    RegistrationIsGoingAway = 0,
    WorkerIsGoingAway,
  };

  ~ServiceWorkerRegistrationWorkerThread();

  void
  InitListener();

  void
  ReleaseListener(Reason aReason);

  workers::WorkerPrivate* mWorkerPrivate;
  RefPtr<WorkerListener> mListener;

#ifndef MOZ_SIMPLEPUSH
  RefPtr<PushManager> mPushManager;
#endif
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ServiceWorkerRegistration_h */
