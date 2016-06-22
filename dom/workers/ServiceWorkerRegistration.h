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
#include "mozilla/dom/workers/bindings/WorkerHolder.h"
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

class ServiceWorkerRegistration : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(updatefound)

  static bool
  Visible(JSContext* aCx, JSObject* aObj);

  static bool
  NotificationAPIVisible(JSContext* aCx, JSObject* aObj);


  static already_AddRefed<ServiceWorkerRegistration>
  CreateForMainThread(nsPIDOMWindowInner* aWindow,
                      const nsAString& aScope);

  static already_AddRefed<ServiceWorkerRegistration>
  CreateForWorker(workers::WorkerPrivate* aWorkerPrivate,
                  const nsAString& aScope);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual already_AddRefed<workers::ServiceWorker>
  GetInstalling() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetWaiting() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetActive() = 0;

  virtual void
  GetScope(nsAString& aScope) const = 0;

  virtual already_AddRefed<Promise>
  Update(ErrorResult& aRv) = 0;

  virtual already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) = 0;

  virtual already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) = 0;

  virtual already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) = 0;

  virtual already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) = 0;

protected:
  ServiceWorkerRegistration(nsPIDOMWindowInner* aWindow,
                            const nsAString& aScope);

  virtual ~ServiceWorkerRegistration()
  { }

  const nsString mScope;
};


} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ServiceWorkerRegistration_h */
