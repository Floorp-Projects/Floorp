/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_remoteserviceworkercontainerimpl_h__
#define mozilla_dom_remoteserviceworkercontainerimpl_h__

#include "ServiceWorkerContainer.h"

namespace mozilla {
namespace dom {

class ServiceWorkerContainerChild;

class RemoteServiceWorkerContainerImpl final : public ServiceWorkerContainer::Inner
{
  ServiceWorkerContainerChild* mActor;
  ServiceWorkerContainer* mOuter;
  bool mShutdown;

  ~RemoteServiceWorkerContainerImpl();

  void
  Shutdown();

  // ServiceWorkerContainer::Inner implementation
  void
  AddContainer(ServiceWorkerContainer* aOuter) override;

  void
  RemoveContainer(ServiceWorkerContainer* aOuter) override;

  void
  Register(const ClientInfo& aClientInfo,
           const nsACString& aScopeURL,
           const nsACString& aScriptURL,
           ServiceWorkerUpdateViaCache aUpdateViaCache,
           ServiceWorkerRegistrationCallback&& aSuccessCB,
           ServiceWorkerFailureCallback&& aFailureCB) const override;

  void
  GetRegistration(const ClientInfo& aClientInfo,
                  const nsACString& aURL,
                  ServiceWorkerRegistrationCallback&& aSuccessCB,
                  ServiceWorkerFailureCallback&& aFailureCB) const override;

  void
  GetRegistrations(const ClientInfo& aClientInfo,
                   ServiceWorkerRegistrationListCallback&& aSuccessCB,
                   ServiceWorkerFailureCallback&& aFailureCB) const override;

  void
  GetReady(const ClientInfo& aClientInfo,
           ServiceWorkerRegistrationCallback&& aSuccessCB,
           ServiceWorkerFailureCallback&& aFailureCB) const override;

public:
  RemoteServiceWorkerContainerImpl();

  void
  RevokeActor(ServiceWorkerContainerChild* aActor);

  NS_INLINE_DECL_REFCOUNTING(RemoteServiceWorkerContainerImpl, override)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_remoteserviceworkercontainerimpl_h__
