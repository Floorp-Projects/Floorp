/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorker.h"

#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "ServiceWorkerCloneData.h"
#include "ServiceWorkerImpl.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"

#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerPrivate.h"

#ifdef XP_WIN
#undef PostMessage
#endif

using mozilla::ErrorResult;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return DOMPrefs::ServiceWorkersEnabled();
  }

  return IS_INSTANCE_OF(ServiceWorkerGlobalScope, aObj);
}

// static
already_AddRefed<ServiceWorker>
ServiceWorker::Create(nsIGlobalObject* aOwner,
                      const ServiceWorkerDescriptor& aDescriptor)
{
  RefPtr<ServiceWorker> ref;

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return ref.forget();
  }

  RefPtr<ServiceWorkerRegistrationInfo> reg =
    swm->GetRegistration(aDescriptor.PrincipalInfo(), aDescriptor.Scope());
  if (!reg) {
    return ref.forget();
  }

  RefPtr<ServiceWorkerInfo> info = reg->GetByDescriptor(aDescriptor);
  if (!info) {
    return ref.forget();
  }

  RefPtr<ServiceWorker::Inner> inner = new ServiceWorkerImpl(info);
  ref = new ServiceWorker(aOwner, aDescriptor, inner);
  return ref.forget();
}

ServiceWorker::ServiceWorker(nsIGlobalObject* aGlobal,
                             const ServiceWorkerDescriptor& aDescriptor,
                             ServiceWorker::Inner* aInner)
  : DOMEventTargetHelper(aGlobal)
  , mDescriptor(aDescriptor)
  , mInner(aInner)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mInner);

  KeepAliveIfHasListenersFor(NS_LITERAL_STRING("statechange"));

  // The error event handler is required by the spec currently, but is not used
  // anywhere.  Don't keep the object alive in that case.

  // This will update our state too.
  mInner->AddServiceWorker(this);
}

ServiceWorker::~ServiceWorker()
{
  MOZ_ASSERT(NS_IsMainThread());
  mInner->RemoveServiceWorker(this);
}

NS_IMPL_ADDREF_INHERITED(ServiceWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorker)
  NS_INTERFACE_MAP_ENTRY(ServiceWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
ServiceWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(NS_IsMainThread());

  return ServiceWorker_Binding::Wrap(aCx, this, aGivenProto);
}

ServiceWorkerState
ServiceWorker::State() const
{
  return mDescriptor.State();
}

void
ServiceWorker::SetState(ServiceWorkerState aState)
{
  ServiceWorkerState oldState = mDescriptor.State();
  mDescriptor.SetState(aState);
  if (oldState == aState) {
    return;
  }

  DOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("statechange"));

  // Once we have transitioned to the redundant state then no
  // more statechange events will occur.  We can allow the DOM
  // object to GC if script is not holding it alive.
  if (mDescriptor.State() == ServiceWorkerState::Redundant) {
    IgnoreKeepAliveIfHasListenersFor(NS_LITERAL_STRING("statechange"));
  }
}

void
ServiceWorker::GetScriptURL(nsString& aURL) const
{
  CopyUTF8toUTF16(mDescriptor.ScriptURL(), aURL);
}

void
ServiceWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           const Sequence<JSObject*>& aTransferable,
                           ErrorResult& aRv)
{
  if (State() == ServiceWorkerState::Redundant) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsPIDOMWindowInner* window = GetOwner();
  if (NS_WARN_IF(!window || !window->GetExtantDoc())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (storageAllowed != nsContentUtils::StorageAccess::eAllow) {
    ServiceWorkerManager::LocalizeAndReportToAllClients(
      mDescriptor.Scope(), "ServiceWorkerPostMessageStorageError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(mDescriptor.Scope()) });
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  Maybe<ClientInfo> clientInfo = window->GetClientInfo();
  Maybe<ClientState> clientState = window->GetClientState();
  if (NS_WARN_IF(clientInfo.isNothing() || clientState.isNothing())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (aRv.Failed()) {
    return;
  }

  RefPtr<ServiceWorkerCloneData> data = new ServiceWorkerCloneData();
  data->Write(aCx, aMessage, transferable, aRv);
  if (aRv.Failed()) {
    return;
  }

  mInner->PostMessage(std::move(data), clientInfo.ref(), clientState.ref());
}


const ServiceWorkerDescriptor&
ServiceWorker::Descriptor() const
{
  return mDescriptor;
}

void
ServiceWorker::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
}

} // namespace dom
} // namespace mozilla
