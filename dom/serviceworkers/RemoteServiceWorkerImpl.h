/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_remoteserviceworkerimpl_h__
#define mozilla_dom_remoteserviceworkerimpl_h__

#include "ServiceWorker.h"

namespace mozilla {
namespace dom {

class ServiceWorkerChild;

class RemoteServiceWorkerImpl final : public ServiceWorker::Inner
{
  ServiceWorkerChild* mActor;
  ServiceWorker* mWorker;
  bool mShutdown;

  ~RemoteServiceWorkerImpl();

  void
  Shutdown();

  // ServiceWorker::Inner implementation
  void
  AddServiceWorker(ServiceWorker* aWorker) override;

  void
  RemoveServiceWorker(ServiceWorker* aWorker) override;

  void
  GetRegistration(ServiceWorkerRegistrationCallback&& aSuccessCB,
                  ServiceWorkerFailureCallback&& aFailureCB) override;

  void
  PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
              const ClientInfo& aClientInfo,
              const ClientState& aClientState) override;

public:
  explicit RemoteServiceWorkerImpl(const ServiceWorkerDescriptor& aDescriptor);

  void
  RevokeActor(ServiceWorkerChild* aActor);

  NS_INLINE_DECL_REFCOUNTING(RemoteServiceWorkerImpl, override)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_remoteserviceworkerimpl_h__
