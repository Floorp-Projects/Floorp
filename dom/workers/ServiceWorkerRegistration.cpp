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

#include "Workers.h"

#ifndef MOZ_SIMPLEPUSH
#include "mozilla/dom/PushManagerBinding.h"
#endif

using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

bool
ServiceWorkerRegistrationVisible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.serviceWorkers.enabled", false);
  }

  // Otherwise check the pref via the work private helper
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->ServiceWorkersEnabled();
}

NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistrationBase, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistrationBase, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationBase)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationBase,
                                   DOMEventTargetHelper, mCCDummy);

ServiceWorkerRegistrationBase::ServiceWorkerRegistrationBase(nsPIDOMWindow* aWindow,
                                                             const nsAString& aScope)
  : DOMEventTargetHelper(aWindow)
  , mScope(aScope)
  , mListeningForEvents(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  StartListeningForEvents();
}

ServiceWorkerRegistrationBase::~ServiceWorkerRegistrationBase()
{
  StopListeningForEvents();
}

void
ServiceWorkerRegistrationBase::DisconnectFromOwner()
{
  StopListeningForEvents();
  DOMEventTargetHelper::DisconnectFromOwner();
}

// XXXnsm, maybe this can be optimized to only add when a event handler is
// registered.
void
ServiceWorkerRegistrationBase::StartListeningForEvents()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->AddRegistrationEventListener(mScope, this);
    mListeningForEvents = true;
  }
}

void
ServiceWorkerRegistrationBase::StopListeningForEvents()
{
  if (!mListeningForEvents) {
    return;
  }

  nsCOMPtr<nsIServiceWorkerManager> swm = do_GetService(SERVICEWORKERMANAGER_CONTRACTID);
  if (swm) {
    swm->RemoveRegistrationEventListener(mScope, this);
    mListeningForEvents = false;
  }
}

////////////////////////////////////////////////////
// Main Thread implementation
NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistrationMainThread, ServiceWorkerRegistrationBase)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistrationMainThread, ServiceWorkerRegistrationBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationMainThread)
NS_INTERFACE_MAP_END_INHERITING(ServiceWorkerRegistrationBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationMainThread, ServiceWorkerRegistrationBase,
#ifndef MOZ_SIMPLEPUSH
                                   mPushManager,
#endif
                                   mInstallingWorker, mWaitingWorker, mActiveWorker);

ServiceWorkerRegistrationMainThread::ServiceWorkerRegistrationMainThread(nsPIDOMWindow* aWindow,
                                                                         const nsAString& aScope)
  : ServiceWorkerRegistrationBase(aWindow, aScope)
{
}

ServiceWorkerRegistrationMainThread::~ServiceWorkerRegistrationMainThread()
{
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationMainThread::GetWorkerReference(WhichServiceWorker aWhichOne)
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

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv) || rv == NS_ERROR_DOM_NOT_FOUND_ERR,
                   "Unexpected error getting service worker instance from ServiceWorkerManager");
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsRefPtr<ServiceWorker> ref =
    static_cast<ServiceWorker*>(serviceWorker.get());
  return ref.forget();
}

JSObject*
ServiceWorkerRegistrationMainThread::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnMainThread();
  return ServiceWorkerRegistrationBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationMainThread::GetInstalling()
{
  AssertIsOnMainThread();
  if (!mInstallingWorker) {
    mInstallingWorker = GetWorkerReference(WhichServiceWorker::INSTALLING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mInstallingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationMainThread::GetWaiting()
{
  AssertIsOnMainThread();
  if (!mWaitingWorker) {
    mWaitingWorker = GetWorkerReference(WhichServiceWorker::WAITING_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mWaitingWorker;
  return ret.forget();
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationMainThread::GetActive()
{
  AssertIsOnMainThread();
  if (!mActiveWorker) {
    mActiveWorker = GetWorkerReference(WhichServiceWorker::ACTIVE_WORKER);
  }

  nsRefPtr<ServiceWorker> ret = mActiveWorker;
  return ret.forget();
}

void
ServiceWorkerRegistrationMainThread::InvalidateWorkerReference(WhichServiceWorker aWhichOnes)
{
  AssertIsOnMainThread();
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

namespace {

class UnregisterCallback final : public nsIServiceWorkerUnregisterCallback
{
  nsRefPtr<Promise> mPromise;

public:
  NS_DECL_ISUPPORTS

  explicit UnregisterCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  NS_IMETHODIMP
  UnregisterSucceeded(bool aState) override
  {
    AssertIsOnMainThread();
    mPromise->MaybeResolve(aState);
    return NS_OK;
  }

  NS_IMETHODIMP
  UnregisterFailed() override
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

void
ServiceWorkerRegistrationMainThread::Update()
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIServiceWorkerManager> swm =
    mozilla::services::GetServiceWorkerManager();
  MOZ_ASSERT(swm);
  // The spec defines ServiceWorkerRegistrationBase.update() exactly as Soft Update.
  swm->SoftUpdate(mScope);
}

already_AddRefed<Promise>
ServiceWorkerRegistrationMainThread::Unregister(ErrorResult& aRv)
{
  AssertIsOnMainThread();
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
  // "If the origin of scope is not client's origin..."
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
  aRv = scopeURI->GetSpecIgnoringRef(uriSpec);
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
  aRv = swm->Unregister(documentPrincipal, cb, scope);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<PushManager>
ServiceWorkerRegistrationMainThread::GetPushManager(ErrorResult& aRv)
{
  AssertIsOnMainThread();

#ifdef MOZ_SIMPLEPUSH
  return nullptr;
#else

  if (!mPushManager) {
    nsCOMPtr<nsIGlobalObject> globalObject = do_QueryInterface(GetOwner());

    if (!globalObject) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(globalObject))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    JSContext* cx = jsapi.cx();

    JS::RootedObject globalJs(cx, globalObject->GetGlobalJSObject());
    GlobalObject global(cx, globalJs);

    // TODO: bug 1148117.  This will fail when swr is exposed on workers
    JS::Rooted<JSObject*> jsImplObj(cx);
    nsCOMPtr<nsIGlobalObject> unused = ConstructJSImplementation(cx, "@mozilla.org/push/PushManager;1",
                              global, &jsImplObj, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    mPushManager = new PushManager(jsImplObj, globalObject);

    mPushManager->SetScope(mScope, aRv);
    if (aRv.Failed()) {
      mPushManager = nullptr;
      return nullptr;
    }
  }

  nsRefPtr<PushManager> ret = mPushManager;
  return ret.forget();

  #endif /* ! MOZ_SIMPLEPUSH */
}

////////////////////////////////////////////////////
// Worker Thread implementation
NS_IMPL_ADDREF_INHERITED(ServiceWorkerRegistrationWorkerThread, ServiceWorkerRegistrationBase)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerRegistrationWorkerThread, ServiceWorkerRegistrationBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationWorkerThread)
NS_INTERFACE_MAP_END_INHERITING(ServiceWorkerRegistrationBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                   ServiceWorkerRegistrationBase, mCCDummyWorkerThread);

JSObject*
ServiceWorkerRegistrationWorkerThread::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnMainThread();
  return ServiceWorkerRegistrationBinding_workers::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetInstalling()
{
  MOZ_CRASH("FIXME");
  return nullptr;
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetWaiting()
{
  MOZ_CRASH("FIXME");
  return nullptr;
}

already_AddRefed<workers::ServiceWorker>
ServiceWorkerRegistrationWorkerThread::GetActive()
{
  MOZ_CRASH("FIXME");
  return nullptr;
}

void
ServiceWorkerRegistrationWorkerThread::InvalidateWorkerReference(WhichServiceWorker aWhichOnes)
{
  MOZ_CRASH("FIXME");
}

} // dom namespace
} // mozilla namespace
