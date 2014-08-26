/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistration.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/Services.h"
#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"
#include "ServiceWorker.h"

#include "nsIObserverService.h"
#include "nsIServiceWorkerManager.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"

using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration,
                                   DOMEventTargetHelper,
                                   mWindow,
                                   mInstallingWorker,
                                   mWaitingWorker,
                                   mActiveWorker)

ServiceWorkerRegistration::ServiceWorkerRegistration(nsPIDOMWindow* aWindow,
                                                     const nsAString& aScope)
  : mWindow(aWindow)
  , mScope(aScope)
  , mInnerID(0)
  , mIsListeningForEvents(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  SetIsDOMBinding();
  StartListeningForEvents();

  mInnerID = aWindow->WindowID();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
  StopListeningForEvents();
}

JSObject*
ServiceWorkerRegistration::WrapObject(JSContext* aCx)
{
  return ServiceWorkerRegistrationBinding::Wrap(aCx, this);
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistration::GetInstalling()
{
  if (!mInstallingWorker) {
    mInstallingWorker = GetWorkerReference(WhichServiceWorker::INSTALLING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mInstallingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistration::GetWaiting()
{
  if (!mWaitingWorker) {
    mWaitingWorker = GetWorkerReference(WhichServiceWorker::WAITING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mWaitingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistration::GetActive()
{
  if (!mActiveWorker) {
    mActiveWorker = GetWorkerReference(WhichServiceWorker::ACTIVE_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mActiveWorker;
  return ret.forget();
}

already_AddRefed<Promise>
ServiceWorkerRegistration::Unregister(ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> promise;

  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm =
    do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  aRv = swm->Unregister(mScope, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistration::GetWorkerReference(WhichServiceWorker aWhichOne)
{
  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm =
    do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> serviceWorker;
  switch(aWhichOne) {
    case WhichServiceWorker::INSTALLING_WORKER:
      rv = swm->GetInstalling(mWindow, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::WAITING_WORKER:
      rv = swm->GetWaiting(mWindow, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::ACTIVE_WORKER:
      rv = swm->GetActive(mWindow, getter_AddRefs(serviceWorker));
      break;
    default:
      MOZ_CRASH("Invalid enum value");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsRefPtr<ServiceWorker> ref =
    static_cast<ServiceWorker*>(serviceWorker.get());
  return ref.forget();
}

void
ServiceWorkerRegistration::InvalidateWorkerReference(WhichServiceWorker aWhichOnes)
{
  if (aWhichOnes & WhichServiceWorker::INSTALLING_WORKER) {
    mInstallingWorker = nullptr;
  }

  if (aWhichOnes & WhichServiceWorker::WAITING_WORKER) {
    mWaitingWorker = nullptr;
  }

  if (aWhichOnes & WhichServiceWorker::ACTIVE_WORKER) {
    mActiveWorker = nullptr;
  }
}

// XXXnsm, maybe this can be optimized to only add when a event handler is
// registered.
void
ServiceWorkerRegistration::StartListeningForEvents()
{
  MOZ_ASSERT(!mIsListeningForEvents);

  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  MOZ_ASSERT(mWindow);

  if (swm) {
    swm->AddRegistrationEventListener(GetDocumentURI(), this);
  }

  mIsListeningForEvents = true;
}

void
ServiceWorkerRegistration::StopListeningForEvents()
{
  if (!mIsListeningForEvents) {
    return;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);

  // StopListeningForEvents is called in the dtor, and it can happen that
  // SnowWhite had already set to null mWindow.
  if (swm && mWindow) {
    swm->RemoveRegistrationEventListener(GetDocumentURI(), this);
  }

  mIsListeningForEvents = false;
}

nsIURI*
ServiceWorkerRegistration::GetDocumentURI() const
{
  MOZ_ASSERT(mWindow);
  return mWindow->GetDocumentURI();
}

NS_IMETHODIMP
ServiceWorkerRegistration::Observe(nsISupports* aSubject, const char* aTopic,
                                   const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed")) {
    return NS_OK;
  }

  if (!mIsListeningForEvents) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (innerID == mInnerID) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
    }

    StopListeningForEvents();
  }

  return NS_OK;
}
} // dom namespace
} // mozilla namespace
