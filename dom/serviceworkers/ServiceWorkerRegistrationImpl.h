/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerregistrationimpl_h
#define mozilla_dom_serviceworkerregistrationimpl_h

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
class WeakWorkerRef;

////////////////////////////////////////////////////
// Main Thread implementation

class ServiceWorkerRegistrationMainThread final : public ServiceWorkerRegistration::Inner
                                                , public ServiceWorkerRegistrationListener
{
public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerRegistrationMainThread, override)

  explicit ServiceWorkerRegistrationMainThread(const ServiceWorkerRegistrationDescriptor& aDescriptor);

  // ServiceWorkerRegistration::Inner
  void
  SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  void
  ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

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

  bool
  MatchesDescriptor(const ServiceWorkerRegistrationDescriptor& aDescriptor) override;

private:
  ~ServiceWorkerRegistrationMainThread();

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  void
  RegistrationRemovedInternal();

  ServiceWorkerRegistration* mOuter;
  const nsString mScope;
  bool mListeningForEvents;
};

////////////////////////////////////////////////////
// Worker Thread implementation

class WorkerListener;

class ServiceWorkerRegistrationWorkerThread final : public ServiceWorkerRegistration::Inner
{
  friend class WorkerListener;

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerRegistrationWorkerThread, override)

  explicit ServiceWorkerRegistrationWorkerThread(const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void
  RegistrationRemoved();

  // ServiceWorkerRegistration::Inner
  void
  SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  void
  ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  already_AddRefed<Promise>
  Update(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv) override;

  already_AddRefed<Promise>
  ShowNotification(JSContext* aCx,
                   const nsAString& aTitle,
                   const NotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<Promise>
  GetNotifications(const GetNotificationOptions& aOptions,
                   ErrorResult& aRv) override;

  already_AddRefed<PushManager>
  GetPushManager(JSContext* aCx, ErrorResult& aRv) override;

  void
  UpdateFound();

private:
  ~ServiceWorkerRegistrationWorkerThread();

  void
  InitListener();

  void
  ReleaseListener();

  // This can be called only by WorkerListener.
  WorkerPrivate*
  GetWorkerPrivate(const MutexAutoLock& aProofOfLock);

  ServiceWorkerRegistration* mOuter;
  const nsString mScope;
  RefPtr<WorkerListener> mListener;
  RefPtr<WeakWorkerRef> mWorkerRef;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_serviceworkerregistrationimpl_h
