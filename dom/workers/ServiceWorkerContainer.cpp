/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/Navigator.h"
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

/* static */ bool
ServiceWorkerContainer::IsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());

  JS::Rooted<JSObject*> global(aCx, aGlobal);
  nsCOMPtr<nsPIDOMWindow> window = Navigator::GetWindowFromGlobal(global);
  if (!window) {
    return false;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc || nsContentUtils::IsInPrivateBrowsing(doc)) {
    return false;
  }

  return Preferences::GetBool("dom.serviceWorkers.enabled", false);
}

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

static nsresult
CheckForSlashEscapedCharsInPath(nsIURI* aURI)
{
  MOZ_ASSERT(aURI);

  // A URL that can't be downcast to a standard URL is an invalid URL and should
  // be treated as such and fail with SecurityError.
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (NS_WARN_IF(!url)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAutoCString path;
  nsresult rv = url->GetFilePath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ToLowerCase(path);
  if (path.Find("%2f") != kNotFound ||
      path.Find("%5c") != kNotFound) {
    return NS_ERROR_DOM_TYPE_ERR;
  }

  return NS_OK;
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

  nsCOMPtr<nsIURI> baseURI;

  nsIDocument* doc = GetEntryDocument();
  if (doc) {
    baseURI = doc->GetBaseURI();
  } else {
    // XXXnsm. One of our devtools browser test calls register() from a content
    // script where there is no valid entry document. Use the window to resolve
    // the uri in that case.
    nsCOMPtr<nsPIDOMWindow> window = GetOwner();
    nsCOMPtr<nsPIDOMWindow> outerWindow;
    if (window && (outerWindow = window->GetOuterWindow()) &&
        outerWindow->GetServiceWorkersTestingEnabled()) {
      baseURI = window->GetDocBaseURI();
    }
  }

  nsresult rv;
  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(&aScriptURL);
    return nullptr;
  }

  aRv = CheckForSlashEscapedCharsInPath(scriptURI);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // In ServiceWorkerContainer.register() the scope argument is parsed against
  // different base URLs depending on whether it was passed or not.
  nsCOMPtr<nsIURI> scopeURI;

  // Step 4. If none passed, parse against script's URL
  if (!aOptions.mScope.WasPassed()) {
    NS_NAMED_LITERAL_STRING(defaultScope, "./");
    rv = NS_NewURI(getter_AddRefs(scopeURI), defaultScope,
                   nullptr, scriptURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString spec;
      scriptURI->GetSpec(spec);
      NS_ConvertUTF8toUTF16 wSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(&defaultScope, &wSpec);
      return nullptr;
    }
  } else {
    // Step 5. Parse against entry settings object's base URL.
    rv = NS_NewURI(getter_AddRefs(scopeURI), aOptions.mScope.Value(),
                   nullptr, baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString spec;
      baseURI->GetSpec(spec);
      NS_ConvertUTF8toUTF16 wSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(&aOptions.mScope.Value(), &wSpec);
      return nullptr;
    }

    aRv = CheckForSlashEscapedCharsInPath(scopeURI);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  // The spec says that the "client" passed to Register() must be the global
  // where the ServiceWorkerContainer was retrieved from.
  aRv = swm->Register(GetOwner(), scopeURI, scriptURI, getter_AddRefs(promise));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> ret = static_cast<Promise*>(promise.get());
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

    // TODO: What should we do here if the ServiceWorker script fails to load?
    //       In theory the DOM ServiceWorker object can exist without the worker
    //       thread running, but it seems our design does not expect that.
    nsCOMPtr<nsISupports> serviceWorker;
    rv = swm->GetDocumentController(GetOwner(),
                                    getter_AddRefs(serviceWorker));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    mControllerWorker =
      static_cast<workers::ServiceWorker*>(serviceWorker.get());
  }

  RefPtr<workers::ServiceWorker> ref = mControllerWorker;
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

  RefPtr<Promise> ret = static_cast<Promise*>(promise.get());
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

  RefPtr<Promise> ret = static_cast<Promise*>(promise.get());
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

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aRv = swm->GetScopeForUrl(doc->NodePrincipal(),
                            aUrl, aScope);
}

} // namespace dom
} // namespace mozilla
