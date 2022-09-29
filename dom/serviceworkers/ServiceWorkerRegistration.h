/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistration_h
#define mozilla_dom_ServiceWorkerRegistration_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "mozilla/dom/ServiceWorkerUtils.h"

// Support for Notification API extension.
#include "mozilla/dom/NotificationBinding.h"

class nsIGlobalObject;

namespace mozilla::dom {

class NavigationPreloadManager;
class Promise;
class PushManager;
class WorkerPrivate;
class ServiceWorker;
class ServiceWorkerRegistrationChild;

#define NS_DOM_SERVICEWORKERREGISTRATION_IID         \
  {                                                  \
    0x4578a90e, 0xa427, 0x4237, {                    \
      0x98, 0x4a, 0xbd, 0x98, 0xe4, 0xcd, 0x5f, 0x3a \
    }                                                \
  }

class ServiceWorkerRegistration final : public DOMEventTargetHelper {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_SERVICEWORKERREGISTRATION_IID)
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistration,
                                           DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(updatefound)

  static already_AddRefed<ServiceWorkerRegistration> CreateForMainThread(
      nsPIDOMWindowInner* aWindow,
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  static already_AddRefed<ServiceWorkerRegistration> CreateForWorker(
      WorkerPrivate* aWorkerPrivate, nsIGlobalObject* aGlobal,
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void DisconnectFromOwner() override;

  void RegistrationCleared();

  already_AddRefed<ServiceWorker> GetInstalling() const;

  already_AddRefed<ServiceWorker> GetWaiting() const;

  already_AddRefed<ServiceWorker> GetActive() const;

  already_AddRefed<NavigationPreloadManager> NavigationPreload();

  void UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor);

  bool MatchesDescriptor(
      const ServiceWorkerRegistrationDescriptor& aDescriptor) const;

  void GetScope(nsAString& aScope) const;

  ServiceWorkerUpdateViaCache GetUpdateViaCache(ErrorResult& aRv) const;

  already_AddRefed<Promise> Update(ErrorResult& aRv);

  already_AddRefed<Promise> Unregister(ErrorResult& aRv);

  already_AddRefed<PushManager> GetPushManager(JSContext* aCx,
                                               ErrorResult& aRv);

  already_AddRefed<Promise> ShowNotification(
      JSContext* aCx, const nsAString& aTitle,
      const NotificationOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> GetNotifications(
      const GetNotificationOptions& aOptions, ErrorResult& aRv);

  void SetNavigationPreloadEnabled(bool aEnabled,
                                   ServiceWorkerBoolCallback&& aSuccessCB,
                                   ServiceWorkerFailureCallback&& aFailureCB);

  void SetNavigationPreloadHeader(const nsCString& aHeader,
                                  ServiceWorkerBoolCallback&& aSuccessCB,
                                  ServiceWorkerFailureCallback&& aFailureCB);

  void GetNavigationPreloadState(NavigationPreloadGetStateCallback&& aSuccessCB,
                                 ServiceWorkerFailureCallback&& aFailureCB);

  const ServiceWorkerRegistrationDescriptor& Descriptor() const;

  void WhenVersionReached(uint64_t aVersion,
                          ServiceWorkerBoolCallback&& aCallback);

  void MaybeDispatchUpdateFoundRunnable();

  void RevokeActor(ServiceWorkerRegistrationChild* aActor);

  void FireUpdateFound();

 private:
  ServiceWorkerRegistration(
      nsIGlobalObject* aGlobal,
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  ~ServiceWorkerRegistration();

  void UpdateStateInternal(const Maybe<ServiceWorkerDescriptor>& aInstalling,
                           const Maybe<ServiceWorkerDescriptor>& aWaiting,
                           const Maybe<ServiceWorkerDescriptor>& aActive);

  void MaybeScheduleUpdateFound(
      const Maybe<ServiceWorkerDescriptor>& aInstallingDescriptor);

  void MaybeDispatchUpdateFound();

  void Shutdown();

  ServiceWorkerRegistrationDescriptor mDescriptor;
  RefPtr<ServiceWorkerRegistrationChild> mActor;
  bool mShutdown;

  RefPtr<ServiceWorker> mInstallingWorker;
  RefPtr<ServiceWorker> mWaitingWorker;
  RefPtr<ServiceWorker> mActiveWorker;
  RefPtr<NavigationPreloadManager> mNavigationPreloadManager;
  RefPtr<PushManager> mPushManager;

  struct VersionCallback {
    uint64_t mVersion;
    ServiceWorkerBoolCallback mFunc;

    VersionCallback(uint64_t aVersion, ServiceWorkerBoolCallback&& aFunc)
        : mVersion(aVersion), mFunc(std::move(aFunc)) {
      MOZ_DIAGNOSTIC_ASSERT(mFunc);
    }
  };
  nsTArray<UniquePtr<VersionCallback>> mVersionCallbackList;

  uint64_t mScheduledUpdateFoundId;
  uint64_t mDispatchedUpdateFoundId;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ServiceWorkerRegistration,
                              NS_DOM_SERVICEWORKERREGISTRATION_IID)

}  // namespace mozilla::dom

#endif /* mozilla_dom_ServiceWorkerRegistration_h */
