/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Services.h"

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerContainerBinding.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

#include "ServiceWorker.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper,
                                   mControllerWorker, mReadyPromise)

ServiceWorkerContainer::ServiceWorkerContainer(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
}

void
ServiceWorkerContainer::DisconnectFromOwner()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  MOZ_ASSERT(swm);

  swm->RemoveReadyPromise(GetOwner());

  DOMEventTargetHelper::DisconnectFromOwner();
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

  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  aRv = swm->Register(aOptions.mScope, aScriptURL, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerContainer::GetController()
{
  if (!mControllerWorker) {
    nsresult rv;
    nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
    if (!swm) {
      return nullptr;
    }

    nsCOMPtr<nsISupports> serviceWorker;
    rv = swm->GetDocumentController(GetOwner(), getter_AddRefs(serviceWorker));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    mControllerWorker =
      static_cast<workers::ServiceWorker*>(serviceWorker.get());
  }

  nsRefPtr<workers::ServiceWorker> ref = mControllerWorker;
  return ref.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetRegistrations(ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsISupports> promise;
  aRv = swm->GetRegistrations(GetOwner(), getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetRegistration(const nsAString& aDocumentURL,
                                        ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsISupports> promise;
  aRv = swm->GetRegistration(GetOwner(), aDocumentURL, getter_AddRefs(promise));
  if (aRv.Failed()) {
    return nullptr;
  }

  nsRefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

Promise*
ServiceWorkerContainer::GetReady(ErrorResult& aRv)
{
  if (mReadyPromise) {
    return mReadyPromise;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISupports> promise;
  aRv = swm->GetReadyPromise(GetOwner(), getter_AddRefs(promise));

  mReadyPromise = static_cast<Promise*>(promise.get());
  return mReadyPromise;
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
  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    aRv.Throw(NS_ERROR_FAILURE);
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
} // namespace dom
} // namespace mozilla
