/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainerImpl.h"

#include "ServiceWorkerRegistration.h"

namespace mozilla {
namespace dom {

ServiceWorkerContainerImpl::~ServiceWorkerContainerImpl()
{
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
}

ServiceWorkerContainerImpl::ServiceWorkerContainerImpl()
  : mOuter(nullptr)
{
}

void
ServiceWorkerContainerImpl::AddContainer(ServiceWorkerContainer* aOuter)
{
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aOuter;
}

void
ServiceWorkerContainerImpl::RemoveContainer(ServiceWorkerContainer* aOuter)
{
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(mOuter == aOuter);
  mOuter = nullptr;
}

void
ServiceWorkerContainerImpl::Register(const ClientInfo& aClientInfo,
                                     const nsACString& aScopeURL,
                                     const nsACString& aScriptURL,
                                     ServiceWorkerUpdateViaCache aUpdateViaCache,
                                     ServiceWorkerRegistrationCallback&& aSuccessCB,
                                     ServiceWorkerFailureCallback&& aFailureCB) const
{
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsIGlobalObject* global = mOuter->GetParentObject();
  if (NS_WARN_IF(!global)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  auto holder =
    MakeRefPtr<DOMMozPromiseRequestHolder<ServiceWorkerRegistrationPromise>>(global);

  swm->Register(aClientInfo, aScopeURL, aScriptURL, aUpdateViaCache)->Then(
    global->EventTargetFor(TaskCategory::Other), __func__,
    [successCB = std::move(aSuccessCB), holder] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      holder->Complete();
      successCB(aDescriptor);
    }, [failureCB = std::move(aFailureCB), holder] (const CopyableErrorResult& aResult) {
      holder->Complete();
      failureCB(CopyableErrorResult(aResult));
    })->Track(*holder);
}

void
ServiceWorkerContainerImpl::GetRegistration(const ClientInfo& aClientInfo,
                                            const nsACString& aURL,
                                            ServiceWorkerRegistrationCallback&& aSuccessCB,
                                            ServiceWorkerFailureCallback&& aFailureCB) const
{
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsIGlobalObject* global = mOuter->GetParentObject();
  if (NS_WARN_IF(!global)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  auto holder =
    MakeRefPtr<DOMMozPromiseRequestHolder<ServiceWorkerRegistrationPromise>>(global);

  swm->GetRegistration(aClientInfo, aURL)->Then(
    global->EventTargetFor(TaskCategory::Other), __func__,
    [successCB = std::move(aSuccessCB), holder] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      holder->Complete();
      successCB(aDescriptor);
    }, [failureCB = std::move(aFailureCB), holder] (const CopyableErrorResult& aResult) {
      holder->Complete();
      failureCB(CopyableErrorResult(aResult));
    })->Track(*holder);
}

void
ServiceWorkerContainerImpl::GetRegistrations(const ClientInfo& aClientInfo,
                                             ServiceWorkerRegistrationListCallback&& aSuccessCB,
                                             ServiceWorkerFailureCallback&& aFailureCB) const
{
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsIGlobalObject* global = mOuter->GetParentObject();
  if (NS_WARN_IF(!global)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  auto holder =
    MakeRefPtr<DOMMozPromiseRequestHolder<ServiceWorkerRegistrationListPromise>>(global);

  swm->GetRegistrations(aClientInfo)->Then(
    global->EventTargetFor(TaskCategory::Other), __func__,
    [successCB = std::move(aSuccessCB), holder] (const nsTArray<ServiceWorkerRegistrationDescriptor>& aList) {
      holder->Complete();
      successCB(aList);
    }, [failureCB = std::move(aFailureCB), holder] (const CopyableErrorResult& aResult) {
      holder->Complete();
      failureCB(CopyableErrorResult(aResult));
    })->Track(*holder);
}

void
ServiceWorkerContainerImpl::GetReady(const ClientInfo& aClientInfo,
                                     ServiceWorkerRegistrationCallback&& aSuccessCB,
                                     ServiceWorkerFailureCallback&& aFailureCB) const
{
  MOZ_DIAGNOSTIC_ASSERT(mOuter);

  nsIGlobalObject* global = mOuter->GetParentObject();
  if (NS_WARN_IF(!global)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (NS_WARN_IF(!swm)) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  auto holder =
    MakeRefPtr<DOMMozPromiseRequestHolder<ServiceWorkerRegistrationPromise>>(global);

  swm->WhenReady(aClientInfo)->Then(
    global->EventTargetFor(TaskCategory::Other), __func__,
    [successCB = std::move(aSuccessCB), holder] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      holder->Complete();
      successCB(aDescriptor);
    }, [failureCB = std::move(aFailureCB), holder] (const CopyableErrorResult& aResult) {
      holder->Complete();
      failureCB(CopyableErrorResult(aResult));
    })->Track(*holder);
}

} // namespace dom
} // namespace mozilla
