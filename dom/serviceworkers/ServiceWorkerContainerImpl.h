/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkercontainerimpl_h__
#define mozilla_dom_serviceworkercontainerimpl_h__

#include "ServiceWorkerContainer.h"

namespace mozilla {
namespace dom {

// Lightweight serviceWorker APIs collection.
class ServiceWorkerContainerImpl final : public ServiceWorkerContainer::Inner
{
  ~ServiceWorkerContainerImpl() = default;

public:
  ServiceWorkerContainerImpl() = default;

  RefPtr<ServiceWorkerRegistrationPromise>
  Register(const nsAString& aScriptURL,
           const RegistrationOptions& aOptions) override;

  RefPtr<ServiceWorkerRegistrationPromise>
  GetRegistration(const ClientInfo& aClientInfo,
                  const nsACString& aURL) const override;

  RefPtr<ServiceWorkerRegistrationListPromise>
  GetRegistrations() override;

  RefPtr<ServiceWorkerRegistrationPromise>
  GetReady(const ClientInfo& aClientInfo) const override;

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerContainerImpl, override)
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_serviceworkercontainerimpl_h__ */
