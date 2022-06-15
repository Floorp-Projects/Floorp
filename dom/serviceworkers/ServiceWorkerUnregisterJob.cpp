/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUnregisterJob.h"

#include "mozilla/Unused.h"
#include "nsIPushService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "ServiceWorkerManager.h"

namespace mozilla::dom {

class ServiceWorkerUnregisterJob::PushUnsubscribeCallback final
    : public nsIUnsubscribeResultCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit PushUnsubscribeCallback(ServiceWorkerUnregisterJob* aJob)
      : mJob(aJob) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  OnUnsubscribe(nsresult aStatus, bool) override {
    // Warn if unsubscribing fails, but don't prevent the worker from
    // unregistering.
    Unused << NS_WARN_IF(NS_FAILED(aStatus));
    mJob->Unregister();
    return NS_OK;
  }

 private:
  ~PushUnsubscribeCallback() = default;

  RefPtr<ServiceWorkerUnregisterJob> mJob;
};

NS_IMPL_ISUPPORTS(ServiceWorkerUnregisterJob::PushUnsubscribeCallback,
                  nsIUnsubscribeResultCallback)

ServiceWorkerUnregisterJob::ServiceWorkerUnregisterJob(nsIPrincipal* aPrincipal,
                                                       const nsACString& aScope,
                                                       bool aSendToParent)
    : ServiceWorkerJob(Type::Unregister, aPrincipal, aScope, ""_ns),
      mResult(false),
      mSendToParent(aSendToParent) {}

bool ServiceWorkerUnregisterJob::GetResult() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mResult;
}

ServiceWorkerUnregisterJob::~ServiceWorkerUnregisterJob() = default;

void ServiceWorkerUnregisterJob::AsyncExecute() {
  MOZ_ASSERT(NS_IsMainThread());

  if (Canceled()) {
    Finish(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Push API, section 5: "When a service worker registration is unregistered,
  // any associated push subscription must be deactivated." To ensure the
  // service worker registration isn't cleared as we're unregistering, we
  // unsubscribe first.
  nsCOMPtr<nsIPushService> pushService =
      do_GetService("@mozilla.org/push/Service;1");
  if (NS_WARN_IF(!pushService)) {
    Unregister();
    return;
  }
  nsCOMPtr<nsIUnsubscribeResultCallback> unsubscribeCallback =
      new PushUnsubscribeCallback(this);
  nsresult rv = pushService->Unsubscribe(NS_ConvertUTF8toUTF16(mScope),
                                         mPrincipal, unsubscribeCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unregister();
  }
}

void ServiceWorkerUnregisterJob::Unregister() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (Canceled() || !swm) {
    Finish(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Step 1 of the Unregister algorithm requires checking that the
  // client origin matches the scope's origin.  We perform this in
  // registration->update() method directly since we don't have that
  // client information available here.

  // "Let registration be the result of running [[Get Registration]]
  // algorithm passing scope as the argument."
  RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetRegistration(mPrincipal, mScope);
  if (!registration) {
    // "If registration is null, then, resolve promise with false."
    Finish(NS_OK);
    return;
  }

  // Note, we send the message to remove the registration from disk now. This is
  // necessary to ensure the registration is removed if the controlled
  // clients are closed by shutting down the browser.
  if (mSendToParent) {
    swm->MaybeSendUnregister(mPrincipal, mScope);
  }

  swm->EvictFromBFCache(registration);

  // "Remove scope to registration map[job's scope url]."
  swm->RemoveRegistration(registration);
  MOZ_ASSERT(registration->IsUnregistered());

  // "Resolve promise with true"
  mResult = true;
  InvokeResultCallbacks(NS_OK);

  // "Invoke Try Clear Registration with registration"
  if (!registration->IsControllingClients()) {
    if (registration->IsIdle()) {
      registration->Clear();
    } else {
      registration->ClearWhenIdle();
    }
  }

  Finish(NS_OK);
}

}  // namespace mozilla::dom
