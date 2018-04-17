/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainerImpl.h"

namespace mozilla {
namespace dom {

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerContainerImpl::Register(const nsAString& aScriptURL,
                                     const RegistrationOptions& aOptions)
{
  // TODO
  return nullptr;
}

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerContainerImpl::GetRegistration(const ClientInfo& aClientInfo,
                                            const nsACString& aURL) const
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                                             __func__);
  }

  return swm->GetRegistration(aClientInfo, aURL);
}

RefPtr<ServiceWorkerRegistrationListPromise>
ServiceWorkerContainerImpl::GetRegistrations()
{
  // TODO
  return nullptr;
}

RefPtr<ServiceWorkerRegistrationPromise>
ServiceWorkerContainerImpl::GetReady(const ClientInfo& aClientInfo) const
{
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    return ServiceWorkerRegistrationPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                                             __func__);
  }

  return swm->WhenReady(aClientInfo);
}

} // namespace dom
} // namespace mozilla
