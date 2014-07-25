/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsPIDOMWindow.h"

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerContainerBinding.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#include "ServiceWorker.h"

namespace mozilla {
namespace dom {
namespace workers {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper,
                                   mInstallingWorker,
                                   mWaitingWorker,
                                   mActiveWorker,
                                   mControllerWorker)

ServiceWorkerContainer::ServiceWorkerContainer(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  SetIsDOMBinding();
  StartListeningForEvents();
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
  StopListeningForEvents();
}

JSObject*
ServiceWorkerContainer::WrapObject(JSContext* aCx)
{
  return ServiceWorkerContainerBinding::Wrap(aCx, this);
}

already_AddRefed<Promise>
ServiceWorkerContainer::Register(const nsAString& aScriptURL,
                                 const RegistrationOptionList& aOptions,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> promise;

  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  aRv = swm->Register(mWindow, aOptions.mScope, aScriptURL, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::Unregister(const nsAString& aScope,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> promise;

  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  aRv = swm->Unregister(mWindow, aScope, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetInstalling()
{
  if (!mInstallingWorker) {
    mInstallingWorker = GetWorkerReference(WhichServiceWorker::INSTALLING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mInstallingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetWaiting()
{
  if (!mWaitingWorker) {
    mWaitingWorker = GetWorkerReference(WhichServiceWorker::WAITING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mWaitingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetActive()
{
  if (!mActiveWorker) {
    mActiveWorker = GetWorkerReference(WhichServiceWorker::ACTIVE_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mActiveWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetController()
{
  if (!mControllerWorker) {
    nsresult rv;
    nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    nsCOMPtr<nsISupports> serviceWorker;
    rv = swm->GetDocumentController(mWindow, getter_AddRefs(serviceWorker));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    mControllerWorker = static_cast<ServiceWorker*>(serviceWorker.get());
  }

  nsRefPtr<ServiceWorker> ref = mControllerWorker;
  return ref.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetAll(ErrorResult& aRv)
{
  // FIXME(nsm): Bug 1002571
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetReady(ErrorResult& aRv)
{
  // FIXME(nsm): Bug 1025077
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  return Promise::Create(global, aRv);
}

// XXXnsm, maybe this can be optimized to only add when a event handler is
// registered.
void
ServiceWorkerContainer::StartListeningForEvents()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->AddContainerEventListener(mWindow->GetDocumentURI(), this);
  }
}

void
ServiceWorkerContainer::StopListeningForEvents()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->RemoveContainerEventListener(mWindow->GetDocumentURI(), this);
  }
}

void
ServiceWorkerContainer::InvalidateWorkerReference(WhichServiceWorker aWhichOnes)
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

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetWorkerReference(WhichServiceWorker aWhichOne)
{
  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
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

  nsRefPtr<ServiceWorker> ref = static_cast<ServiceWorker*>(serviceWorker.get());
  return ref.forget();
}

// Testing only.
already_AddRefed<Promise>
ServiceWorkerContainer::ClearAllServiceWorkerData(ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

// Testing only.
void
ServiceWorkerContainer::GetScopeForUrl(const nsAString& aUrl,
                                       nsString& aScope,
                                       ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  aRv = swm->GetScopeForUrl(aUrl, aScope);
}

// Testing only.
void
ServiceWorkerContainer::GetControllingWorkerScriptURLForPath(
                                                        const nsAString& aPath,
                                                        nsString& aScriptURL,
                                                        ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}
} // namespace workers
} // namespace dom
} // namespace mozilla
