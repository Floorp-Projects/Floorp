/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistration_h
#define mozilla_dom_ServiceWorkerRegistration_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"

// Support for Notification API extension.
#include "mozilla/dom/NotificationBinding.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class Promise;
class PushManager;
class WorkerPrivate;
class ServiceWorker;

class ServiceWorkerRegistration final : public DOMEventTargetHelper
{
public:
  class Inner
  {
  public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

    virtual void
    SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg) = 0;

    virtual void
    ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg) = 0;

    virtual already_AddRefed<Promise>
    Update(ErrorResult& aRv) = 0;

    virtual already_AddRefed<Promise>
    Unregister(ErrorResult& aRv) = 0;

    virtual already_AddRefed<Promise>
    ShowNotification(JSContext* aCx,
                     const nsAString& aTitle,
                     const NotificationOptions& aOptions,
                     ErrorResult& aRv) = 0;

    virtual already_AddRefed<Promise>
    GetNotifications(const GetNotificationOptions& aOptions,
                     ErrorResult& aRv) = 0;

    virtual already_AddRefed<PushManager>
    GetPushManager(JSContext* aCx, ErrorResult& aRv) = 0;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(updatefound)

  static already_AddRefed<ServiceWorkerRegistration>
  CreateForMainThread(nsPIDOMWindowInner* aWindow,
                      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  static already_AddRefed<ServiceWorkerRegistration>
  CreateForWorker(WorkerPrivate* aWorkerPrivate,
                  const ServiceWorkerRegistrationDescriptor& aDescriptor);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void DisconnectFromOwner() override;

  already_AddRefed<ServiceWorker>
  GetInstalling() const;

  already_AddRefed<ServiceWorker>
  GetWaiting() const;

  already_AddRefed<ServiceWorker>
  GetActive() const;

  void
  UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor);

  bool
  MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) const;

  void
  GetScope(nsAString& aScope) const;

  ServiceWorkerUpdateViaCache
  GetUpdateViaCache(ErrorResult& aRv) const;

  already_AddRefed<Promise>
  Update(ErrorResult& aRv);

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv);

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv);

private:
  ServiceWorkerRegistration(nsIGlobalObject* aGlobal,
                            const ServiceWorkerRegistrationDescriptor& aDescriptor,
                            Inner* aInner);

  ~ServiceWorkerRegistration();

  ServiceWorkerRegistrationDescriptor mDescriptor;
  RefPtr<Inner> mInner;
  RefPtr<ServiceWorker> mInstallingWorker;
  RefPtr<ServiceWorker> mWaitingWorker;
  RefPtr<ServiceWorker> mActiveWorker;
  RefPtr<PushManager> mPushManager;
};


} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ServiceWorkerRegistration_h */
