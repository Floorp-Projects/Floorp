/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerChild.h"
#include "ServiceWorkerManager.h"
#include "mozilla/Unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

mozilla::ipc::IPCResult
ServiceWorkerManagerChild::RecvNotifyRegister(
                                     const ServiceWorkerRegistrationData& aData)
{
  if (mShuttingDown) {
    return IPC_OK();
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->LoadRegistration(aData);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ServiceWorkerManagerChild::RecvNotifySoftUpdate(
                                      const OriginAttributes& aOriginAttributes,
                                      const nsString& aScope)
{
  if (mShuttingDown) {
    return IPC_OK();
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->SoftUpdate(aOriginAttributes, NS_ConvertUTF16toUTF8(aScope));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ServiceWorkerManagerChild::RecvNotifyUnregister(const PrincipalInfo& aPrincipalInfo,
                                                const nsString& aScope)
{
  if (mShuttingDown) {
    return IPC_OK();
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown
    return IPC_OK();
  }

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);
  if (NS_WARN_IF(!principal)) {
    return IPC_OK();
  }

  nsresult rv = swm->NotifyUnregister(principal, aScope);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return IPC_OK();
}

mozilla::ipc::IPCResult
ServiceWorkerManagerChild::RecvNotifyRemove(const nsCString& aHost)
{
  if (mShuttingDown) {
    return IPC_OK();
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->Remove(aHost);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ServiceWorkerManagerChild::RecvNotifyRemoveAll()
{
  if (mShuttingDown) {
    return IPC_OK();
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (swm) {
    swm->RemoveAll();
  }

  return IPC_OK();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
