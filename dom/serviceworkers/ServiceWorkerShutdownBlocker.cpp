/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerShutdownBlocker.h"

#include <utility>

#include "MainThreadUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIWritablePropertyBag2.h"
#include "nsThreadUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ServiceWorkerShutdownBlocker, nsIAsyncShutdownBlocker)

NS_IMETHODIMP ServiceWorkerShutdownBlocker::GetName(nsAString& aNameOut) {
  aNameOut = NS_LITERAL_STRING(
      "ServiceWorkerShutdownBlocker: shutting down Service Workers");
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerShutdownBlocker::BlockShutdown(nsIAsyncShutdownClient* aClient) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mShutdownClient);

  mShutdownClient = aClient;
  MaybeUnblockShutdown();

  return NS_OK;
}

NS_IMETHODIMP ServiceWorkerShutdownBlocker::GetState(nsIPropertyBag** aBagOut) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aBagOut);

  nsCOMPtr<nsIWritablePropertyBag2> propertyBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");

  if (NS_WARN_IF(!propertyBag)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = propertyBag->SetPropertyAsBool(
      NS_LITERAL_STRING("acceptingPromises"), IsAcceptingPromises());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = propertyBag->SetPropertyAsUint32(NS_LITERAL_STRING("pendingPromises"),
                                        GetPendingPromises());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  propertyBag.forget(aBagOut);

  return NS_OK;
}

/* static */ already_AddRefed<ServiceWorkerShutdownBlocker>
ServiceWorkerShutdownBlocker::CreateAndRegisterOn(
    nsIAsyncShutdownClient* aShutdownBarrier) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aShutdownBarrier);

  RefPtr<ServiceWorkerShutdownBlocker> blocker =
      new ServiceWorkerShutdownBlocker();

  nsresult rv = aShutdownBarrier->AddBlocker(
      blocker.get(), NS_LITERAL_STRING(__FILE__), __LINE__,
      NS_LITERAL_STRING("Service Workers shutdown"));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return blocker.forget();
}

void ServiceWorkerShutdownBlocker::WaitOnPromise(
    GenericNonExclusivePromise* aPromise) {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(IsAcceptingPromises());
  MOZ_ASSERT(aPromise);

  ++mState.as<AcceptingPromises>().mPendingPromises;

  RefPtr<ServiceWorkerShutdownBlocker> self = this;

  aPromise->Then(GetCurrentThreadSerialEventTarget(), __func__,
                 [self = std::move(self)](
                     const GenericNonExclusivePromise::ResolveOrRejectValue&) {
                   if (!self->PromiseSettled()) {
                     self->MaybeUnblockShutdown();
                   }
                 });
}

void ServiceWorkerShutdownBlocker::StopAcceptingPromises() {
  AssertIsOnMainThread();
  MOZ_ASSERT(IsAcceptingPromises());

  mState = AsVariant(NotAcceptingPromises(mState.as<AcceptingPromises>()));
}

ServiceWorkerShutdownBlocker::ServiceWorkerShutdownBlocker()
    : mState(VariantType<AcceptingPromises>()) {
  AssertIsOnMainThread();
}

ServiceWorkerShutdownBlocker::~ServiceWorkerShutdownBlocker() {
  MOZ_ASSERT(!IsAcceptingPromises());
  MOZ_ASSERT(!GetPendingPromises());
  MOZ_ASSERT(!mShutdownClient);
}

void ServiceWorkerShutdownBlocker::MaybeUnblockShutdown() {
  AssertIsOnMainThread();

  if (!mShutdownClient || IsAcceptingPromises() || GetPendingPromises()) {
    return;
  }

  mShutdownClient->RemoveBlocker(this);
  mShutdownClient = nullptr;
}

uint32_t ServiceWorkerShutdownBlocker::PromiseSettled() {
  AssertIsOnMainThread();
  MOZ_ASSERT(GetPendingPromises());

  if (IsAcceptingPromises()) {
    return --mState.as<AcceptingPromises>().mPendingPromises;
  }

  return --mState.as<NotAcceptingPromises>().mPendingPromises;
}

bool ServiceWorkerShutdownBlocker::IsAcceptingPromises() const {
  AssertIsOnMainThread();

  return mState.is<AcceptingPromises>();
}

uint32_t ServiceWorkerShutdownBlocker::GetPendingPromises() const {
  AssertIsOnMainThread();

  if (IsAcceptingPromises()) {
    return mState.as<AcceptingPromises>().mPendingPromises;
  }

  return mState.as<NotAcceptingPromises>().mPendingPromises;
}

ServiceWorkerShutdownBlocker::NotAcceptingPromises::NotAcceptingPromises(
    AcceptingPromises aPreviousState)
    : mPendingPromises(aPreviousState.mPendingPromises) {
  AssertIsOnMainThread();
}

}  // namespace dom
}  // namespace mozilla
