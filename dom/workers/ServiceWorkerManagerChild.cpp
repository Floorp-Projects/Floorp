/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerChild.h"
#include "ServiceWorkerManager.h"
#include "mozilla/unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

bool
ServiceWorkerManagerChild::RecvNotifyRegister(
                                     const ServiceWorkerRegistrationData& aData)
{
  if (mShuttingDown) {
    return true;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->LoadRegistration(aData);
  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifySoftUpdate(
                                      const OriginAttributes& aOriginAttributes,
                                      const nsString& aScope)
{
  if (mShuttingDown) {
    return true;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->SoftUpdate(aOriginAttributes, NS_ConvertUTF16toUTF8(aScope));
  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyUnregister(const PrincipalInfo& aPrincipalInfo,
                                                const nsString& aScope)
{
  if (mShuttingDown) {
    return true;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);
  if (NS_WARN_IF(!principal)) {
    return true;
  }

  nsresult rv = swm->Unregister(principal, nullptr, aScope);
  unused << NS_WARN_IF(NS_FAILED(rv));
  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyRemove(const nsCString& aHost)
{
  if (mShuttingDown) {
    return true;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->Remove(aHost);
  return true;
}

bool
ServiceWorkerManagerChild::RecvNotifyRemoveAll()
{
  if (mShuttingDown) {
    return true;
  }

  nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->RemoveAll();
  return true;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
