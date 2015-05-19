/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  RemoveReadyPromise();
}

void
ServiceWorkerContainer::DisconnectFromOwner()
{
  RemoveReadyPromise();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void
ServiceWorkerContainer::ControllerChanged(ErrorResult& aRv)
{
  mControllerWorker = nullptr;
  aRv = DispatchTrustedEvent(NS_LITERAL_STRING("controllerchange"));
}

void
ServiceWorkerContainer::RemoveReadyPromise()
{
  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (window) {
    nsCOMPtr<nsIServiceWorkerManager> swm =
      mozilla::services::GetServiceWorkerManager();
    if (!swm) {
      // If the browser is shutting down, we don't need to remove the promise.
      return;
    }

    swm->RemoveReadyPromise(window);
  }
}

JSObject*
ServiceWorkerContainer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ServiceWorkerContainerBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
ServiceWorkerContainer::Register(const nsAString& aScriptURL,
                                 const RegistrationOptions& aOptions,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsISupports> promise;

  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  MOZ_ASSERT(window);

  nsresult rv;
  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr,
                 window->GetDocBaseURI());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError(MSG_INVALID_URL, &aScriptURL);
    return nullptr;
  }

  // In ServiceWorkerContainer.register() the scope argument is parsed against
  // different base URLs depending on whether it was passed or not.
  nsCOMPtr<nsIURI> scopeURI;

  // Step 4. If none passed, parse against script's URL
  if (!aOptions.mScope.WasPassed()) {
    nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), NS_LITERAL_CSTRING("./"),
                            nullptr, scriptURI);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));
  } else {
    // Step 5. Parse against entry settings object's base URL.
    nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aOptions.mScope.Value(),
                            nullptr, window->GetDocBaseURI());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError(MSG_INVALID_URL, &aOptions.mScope.Value());
      return nullptr;
    }
  }

  aRv = swm->Register(window, scopeURI, scriptURI, getter_AddRefs(promise));
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

} // namespace dom
} // namespace mozilla
