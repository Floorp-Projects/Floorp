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
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "ServiceWorker.h"

#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"

using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistration, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistration,
                                   DOMEventTargetHelper,
                                   mInstallingWorker,
                                   mWaitingWorker,
                                   mActiveWorker)

ServiceWorkerRegistration::ServiceWorkerRegistration(nsPIDOMWindow* aWindow,
                                                     const nsAString& aScope)
  : DOMEventTargetHelper(aWindow)
  , mScope(aScope)
  , mListeningForEvents(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  StartListeningForEvents();
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
  StopListeningForEvents();
}

void
ServiceWorkerRegistration::DisconnectFromOwner()
{
  StopListeningForEvents();
  DOMEventTargetHelper::DisconnectFromOwner();
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

namespace {

class UnregisterCallback MOZ_FINAL : public nsIServiceWorkerUnregisterCallback
{
  nsRefPtr<Promise> mPromise;

public:
  NS_DECL_ISUPPORTS

  UnregisterCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  NS_IMETHODIMP
  UnregisterSucceeded(bool aState)
  {
    AssertIsOnMainThread();
    mPromise->MaybeResolve(aState);
    return NS_OK;
  }

  NS_IMETHODIMP
  UnregisterFailed()
  {
    AssertIsOnMainThread();

    AutoJSAPI api;
    api.Init(mPromise->GetParentObject());
    mPromise->MaybeReject(api.cx(), JS::UndefinedHandleValue);
    return NS_OK;
  }

private:
  ~UnregisterCallback()
  { }
};

NS_IMPL_ISUPPORTS(UnregisterCallback, nsIServiceWorkerUnregisterCallback)

} // anonymous namespace

already_AddRefed<Promise>
ServiceWorkerRegistration::Unregister(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(GetOwner());
  if (!go) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Although the spec says that the same-origin checks should also be done
  // asynchronously, we do them in sync because the Promise created by the
  // WebIDL infrastructure due to a returned error will be resolved
  // asynchronously. We aren't making any internal state changes in these
  // checks, so ordering of multiple calls is not affected.
  nsCOMPtr<nsIDocument> document = GetOwner()->GetExtantDoc();
  if (!document) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIURI> scopeURI;
  nsCOMPtr<nsIURI> baseURI = document->GetBaseURI();
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), mScope, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> documentPrincipal = document->NodePrincipal();
  rv = documentPrincipal->CheckMayLoad(scopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsAutoCString uriSpec;
  aRv = scopeURI->GetSpec(uriSpec);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm =
    do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<UnregisterCallback> cb = new UnregisterCallback(promise);

  NS_ConvertUTF8toUTF16 scope(uriSpec);
  aRv = swm->Unregister(cb, scope);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistration::GetWorkerReference(WhichServiceWorker aWhichOne)
{
  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIServiceWorkerManager> swm =
    do_GetService(SERVICEWORKERMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> serviceWorker;
  switch(aWhichOne) {
    case WhichServiceWorker::INSTALLING_WORKER:
      rv = swm->GetInstalling(window, mScope, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::WAITING_WORKER:
      rv = swm->GetWaiting(window, mScope, getter_AddRefs(serviceWorker));
      break;
    case WhichServiceWorker::ACTIVE_WORKER:
      rv = swm->GetActive(window, mScope, getter_AddRefs(serviceWorker));
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
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->AddRegistrationEventListener(GetDocumentURI(), this);
    mListeningForEvents = true;
  }
}

void
ServiceWorkerRegistration::StopListeningForEvents()
{
  if (!mListeningForEvents) {
    return;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->RemoveRegistrationEventListener(GetDocumentURI(), this);
    mListeningForEvents = false;
  }
}

nsIURI*
ServiceWorkerRegistration::GetDocumentURI() const
{
  MOZ_ASSERT(GetOwner());
  return GetOwner()->GetDocumentURI();
}

} // dom namespace
} // mozilla namespace
