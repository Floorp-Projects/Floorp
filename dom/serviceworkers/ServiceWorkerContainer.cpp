/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Services.h"

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerContainerBinding.h"

#include "ServiceWorker.h"
#include "ServiceWorkerContainerImpl.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerContainer)
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
  nsCOMPtr<nsPIDOMWindowInner> window = Navigator::GetWindowFromGlobal(global);
  if (!window) {
    return false;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc || nsContentUtils::IsInPrivateBrowsing(doc)) {
    return false;
  }

  return DOMPrefs::ServiceWorkersEnabled();
}

// static
already_AddRefed<ServiceWorkerContainer>
ServiceWorkerContainer::Create(nsIGlobalObject* aGlobal)
{
  RefPtr<Inner> inner = new ServiceWorkerContainerImpl();
  RefPtr<ServiceWorkerContainer> ref =
    new ServiceWorkerContainer(aGlobal, inner.forget());
  return ref.forget();
}

ServiceWorkerContainer::ServiceWorkerContainer(nsIGlobalObject* aGlobal,
                                               already_AddRefed<ServiceWorkerContainer::Inner> aInner)
  : DOMEventTargetHelper(aGlobal)
  , mInner(aInner)
{
  Maybe<ServiceWorkerDescriptor> controller = aGlobal->GetController();
  if (controller.isSome()) {
    mControllerWorker = aGlobal->GetOrCreateServiceWorker(controller.ref());
  }
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
  mReadyPromiseHolder.DisconnectIfExists();
}

void
ServiceWorkerContainer::DisconnectFromOwner()
{
  mControllerWorker = nullptr;
  mReadyPromiseHolder.DisconnectIfExists();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void
ServiceWorkerContainer::ControllerChanged(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = GetParentObject();
  if (!go) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mControllerWorker = go->GetOrCreateServiceWorker(go->GetController().ref());
  aRv = DispatchTrustedEvent(NS_LITERAL_STRING("controllerchange"));
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
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (window) {
    baseURI = window->GetDocBaseURI();
  } else {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aScriptURL);
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
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(defaultScope, wSpec);
      return nullptr;
    }
  } else {
    // Step 5. Parse against entry settings object's base URL.
    rv = NS_NewURI(getter_AddRefs(scopeURI), aOptions.mScope.Value(),
                   nullptr, baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsIURI* uri = baseURI ? baseURI : scriptURI;
      nsAutoCString spec;
      uri->GetSpec(spec);
      NS_ConvertUTF8toUTF16 wSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(aOptions.mScope.Value(), wSpec);
      return nullptr;
    }

    aRv = CheckForSlashEscapedCharsInPath(scopeURI);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  // The spec says that the "client" passed to Register() must be the global
  // where the ServiceWorkerContainer was retrieved from.
  aRv = swm->Register(GetOwner(), scopeURI, scriptURI,
                      static_cast<uint16_t>(aOptions.mUpdateViaCache),
                      getter_AddRefs(promise));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> ret = static_cast<Promise*>(promise.get());
  MOZ_ASSERT(ret);
  return ret.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerContainer::GetController()
{
  RefPtr<ServiceWorker> ref = mControllerWorker;
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
ServiceWorkerContainer::GetRegistration(const nsAString& aURL,
                                        ErrorResult& aRv)
{
  nsIGlobalObject* global = GetGlobalIfValid(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  if (!window) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // It would be nice not to require a window here, but right
  // now we don't have a great way to get the base URL just
  // from the nsIGlobalObject.
  Maybe<ClientInfo> clientInfo = window->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
  if (!baseURI) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, baseURI);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCString spec;
  aRv = uri->GetSpec(spec);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Promise> outer = Promise::Create(window->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  mInner->GetRegistration(clientInfo.ref(), spec)->Then(
    window->EventTargetFor(TaskCategory::Other), __func__,
    [self, outer] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      RefPtr<ServiceWorkerRegistration> reg =
        global->GetOrCreateServiceWorkerRegistration(aDescriptor);
      outer->MaybeResolve(reg);
    }, [self, outer] (nsresult aRv) {
      ErrorResult rv;
      Unused << self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      if (NS_SUCCEEDED(aRv)) {
        outer->MaybeResolveWithUndefined();
        return;
      }
      outer->MaybeReject(aRv);
    });

  return outer.forget();
}

Promise*
ServiceWorkerContainer::GetReady(ErrorResult& aRv)
{
  if (mReadyPromise) {
    return mReadyPromise;
  }

  nsIGlobalObject* global = GetGlobalIfValid(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_DIAGNOSTIC_ASSERT(global);

  Maybe<ClientInfo> clientInfo(global->GetClientInfo());
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mReadyPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;
  RefPtr<Promise> outer = mReadyPromise;

  mInner->GetReady(clientInfo.ref())->Then(
    global->EventTargetFor(TaskCategory::Other), __func__,
    [self, outer] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      self->mReadyPromiseHolder.Complete();
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      RefPtr<ServiceWorkerRegistration> reg =
        global->GetOrCreateServiceWorkerRegistration(aDescriptor);
      NS_ENSURE_TRUE_VOID(reg);
      outer->MaybeResolve(reg);
    }, [self, outer] (nsresult aRv) {
      self->mReadyPromiseHolder.Complete();
      outer->MaybeReject(aRv);
    })->Track(mReadyPromiseHolder);

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

  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
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

nsIGlobalObject*
ServiceWorkerContainer::GetGlobalIfValid(ErrorResult& aRv) const
{
  // For now we require a window since ServiceWorkerContainer is
  // not exposed on worker globals yet.  The main thing we need
  // to fix here to support that is the storage access check via
  // the nsIGlobalObject.
  nsPIDOMWindowInner* window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Don't allow a service worker to access service worker registrations
  // from a window with storage disabled.  If these windows can access
  // the registration it increases the chance they can bypass the storage
  // block via postMessage(), etc.
  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (NS_WARN_IF(storageAllowed != nsContentUtils::StorageAccess::eAllow)) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Service Workers"), doc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "ServiceWorkerGetRegistrationStorageError");
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }
  MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(window->GetExtantDoc()->NodePrincipal()));

  return window->AsGlobal();
}

} // namespace dom
} // namespace mozilla
