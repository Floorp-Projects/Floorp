/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef moz_dom_ServiceWorkerRegistrationProxy_h
#define moz_dom_ServiceWorkerRegistrationProxy_h

#include "mozilla/dom/PServiceWorkerRegistrationParent.h"
#include "nsProxyRelease.h"
#include "ServiceWorkerRegistrationDescriptor.h"
#include "ServiceWorkerRegistrationListener.h"
#include "ServiceWorkerUtils.h"

namespace mozilla::dom {

class ServiceWorkerRegistrationInfo;
class ServiceWorkerRegistrationParent;

class ServiceWorkerRegistrationProxy final
    : public ServiceWorkerRegistrationListener {
  // Background thread only
  RefPtr<ServiceWorkerRegistrationParent> mActor;

  // Written on background thread and read on main thread
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  // Main thread only
  ServiceWorkerRegistrationDescriptor mDescriptor;
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mReg;

  ~ServiceWorkerRegistrationProxy();

  // Background thread methods
  void MaybeShutdownOnBGThread();

  void UpdateStateOnBGThread(
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void FireUpdateFoundOnBGThread();

  // Main thread methods
  void InitOnMainThread();

  void MaybeShutdownOnMainThread();

  void StopListeningOnMainThread();

  // The timer callback to perform the delayed update
  class DelayedUpdate;
  RefPtr<DelayedUpdate> mDelayedUpdate;

  // ServiceWorkerRegistrationListener interface
  void UpdateState(
      const ServiceWorkerRegistrationDescriptor& aDescriptor) override;

  void FireUpdateFound() override;

  void RegistrationCleared() override;

  void GetScope(nsAString& aScope) const override;

  bool MatchesDescriptor(
      const ServiceWorkerRegistrationDescriptor& aDescriptor) override;

 public:
  explicit ServiceWorkerRegistrationProxy(
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void Init(ServiceWorkerRegistrationParent* aActor);

  void RevokeActor(ServiceWorkerRegistrationParent* aActor);

  RefPtr<GenericPromise> Unregister();

  RefPtr<ServiceWorkerRegistrationPromise> Update(
      const nsACString& aNewestWorkerScriptUrl);

  RefPtr<GenericPromise> SetNavigationPreloadEnabled(const bool& aEnabled);

  RefPtr<GenericPromise> SetNavigationPreloadHeader(const nsACString& aHeader);

  RefPtr<NavigationPreloadStatePromise> GetNavigationPreloadState();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServiceWorkerRegistrationProxy,
                                        override);
};

}  // namespace mozilla::dom

#endif  // moz_dom_ServiceWorkerRegistrationProxy_h
