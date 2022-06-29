/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorker.h"

#include "mozilla/dom/Document.h"
#include "nsGlobalWindowInner.h"
#include "nsPIDOMWindow.h"
#include "RemoteServiceWorkerImpl.h"
#include "ServiceWorkerCloneData.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerUtils.h"

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StorageAccess.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla::dom {

static bool IsServiceWorkersTestingEnabledInWindow(JSObject* const aGlobal) {
  if (const nsCOMPtr<nsPIDOMWindowInner> innerWindow =
          Navigator::GetWindowFromGlobal(aGlobal)) {
    if (auto* bc = innerWindow->GetBrowsingContext()) {
      return bc->Top()->ServiceWorkersTestingEnabled();
    }
  }
  return false;
}

bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::dom_serviceWorkers_enabled()) {
    return false;
  }

  if (StaticPrefs::dom_serviceWorkers_hide_in_pbmode_enabled()) {
    if (const nsCOMPtr<nsIGlobalObject> global =
            xpc::CurrentNativeGlobal(aCx)) {
      if (global->GetStorageAccess() == StorageAccess::ePrivateBrowsing) {
        return false;
      }
    }
  }

  // Allow a webextension principal to register a service worker script with
  // a moz-extension url only if 'extensions.service_worker_register.allowed'
  // is true.
  if (!StaticPrefs::extensions_serviceWorkerRegister_allowed()) {
    nsIPrincipal* principal = nsContentUtils::SubjectPrincipal(aCx);
    if (principal && BasePrincipal::Cast(principal)->AddonPolicy()) {
      return false;
    }
  }

  if (IsSecureContextOrObjectIsFromSecureContext(aCx, aGlobal)) {
    return true;
  }

  return StaticPrefs::dom_serviceWorkers_testing_enabled() ||
         IsServiceWorkersTestingEnabledInWindow(aGlobal);
}

bool ServiceWorkerVisible(JSContext* aCx, JSObject* aGlobal) {
  if (NS_IsMainThread()) {
    // We want to expose ServiceWorker interface only when
    // navigator.serviceWorker is available. Currently it may not be available
    // with some reasons:
    // 1. navigator.serviceWorker is not supported in workers. (bug 1131324)
    // 2. `dom.serviceWorkers.hide_in_pbmode.enabled` wants to hide it in
    // private browsing mode.
    return ServiceWorkersEnabled(aCx, aGlobal);
  }

  // We are already in ServiceWorker and interfaces need to be exposed for e.g.
  // globalThis.registration.serviceWorker. Note that navigator.serviceWorker
  // is still not supported. (bug 1131324)
  return IS_INSTANCE_OF(ServiceWorkerGlobalScope, aGlobal);
}

// static
already_AddRefed<ServiceWorker> ServiceWorker::Create(
    nsIGlobalObject* aOwner, const ServiceWorkerDescriptor& aDescriptor) {
  const RefPtr<ServiceWorker::Inner> inner =
      new RemoteServiceWorkerImpl(aDescriptor);
  NS_ENSURE_TRUE(inner, nullptr);

  RefPtr<ServiceWorker> ref = new ServiceWorker(aOwner, aDescriptor, inner);
  return ref.forget();
}

ServiceWorker::ServiceWorker(nsIGlobalObject* aGlobal,
                             const ServiceWorkerDescriptor& aDescriptor,
                             ServiceWorker::Inner* aInner)
    : DOMEventTargetHelper(aGlobal),
      mDescriptor(aDescriptor),
      mInner(aInner),
      mLastNotifiedState(ServiceWorkerState::Installing) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mInner);

  KeepAliveIfHasListenersFor(nsGkAtoms::onstatechange);

  // The error event handler is required by the spec currently, but is not used
  // anywhere.  Don't keep the object alive in that case.

  // This will update our state too.
  mInner->AddServiceWorker(this);

  // Attempt to get an existing binding object for the registration
  // associated with this ServiceWorker.
  RefPtr<ServiceWorkerRegistration> reg =
      aGlobal->GetServiceWorkerRegistration(ServiceWorkerRegistrationDescriptor(
          mDescriptor.RegistrationId(), mDescriptor.RegistrationVersion(),
          mDescriptor.PrincipalInfo(), mDescriptor.Scope(),
          ServiceWorkerUpdateViaCache::Imports));
  if (reg) {
    MaybeAttachToRegistration(reg);
  } else {
    RefPtr<ServiceWorker> self = this;

    mInner->GetRegistration(
        [self = std::move(self)](
            const ServiceWorkerRegistrationDescriptor& aDescriptor) {
          nsIGlobalObject* global = self->GetParentObject();
          NS_ENSURE_TRUE_VOID(global);
          RefPtr<ServiceWorkerRegistration> reg =
              global->GetOrCreateServiceWorkerRegistration(aDescriptor);
          self->MaybeAttachToRegistration(reg);
        },
        [](ErrorResult&& aRv) {
          // do nothing
          aRv.SuppressException();
        });
  }
}

ServiceWorker::~ServiceWorker() {
  MOZ_ASSERT(NS_IsMainThread());
  mInner->RemoveServiceWorker(this);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorker, DOMEventTargetHelper,
                                   mRegistration);

NS_IMPL_ADDREF_INHERITED(ServiceWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorker)
  NS_INTERFACE_MAP_ENTRY(ServiceWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* ServiceWorker::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(NS_IsMainThread());

  return ServiceWorker_Binding::Wrap(aCx, this, aGivenProto);
}

ServiceWorkerState ServiceWorker::State() const { return mDescriptor.State(); }

void ServiceWorker::SetState(ServiceWorkerState aState) {
  NS_ENSURE_TRUE_VOID(aState >= mDescriptor.State());
  mDescriptor.SetState(aState);
}

void ServiceWorker::MaybeDispatchStateChangeEvent() {
  if (mDescriptor.State() <= mLastNotifiedState || !GetParentObject()) {
    return;
  }
  mLastNotifiedState = mDescriptor.State();

  DOMEventTargetHelper::DispatchTrustedEvent(u"statechange"_ns);

  // Once we have transitioned to the redundant state then no
  // more statechange events will occur.  We can allow the DOM
  // object to GC if script is not holding it alive.
  if (mLastNotifiedState == ServiceWorkerState::Redundant) {
    IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onstatechange);
  }
}

void ServiceWorker::GetScriptURL(nsString& aURL) const {
  CopyUTF8toUTF16(mDescriptor.ScriptURL(), aURL);
}

void ServiceWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                                const Sequence<JSObject*>& aTransferable,
                                ErrorResult& aRv) {
  if (State() == ServiceWorkerState::Redundant) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsPIDOMWindowInner* window = GetOwner();
  if (NS_WARN_IF(!window || !window->GetExtantDoc())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  auto storageAllowed = StorageAllowedForWindow(window);
  if (storageAllowed != StorageAccess::eAllow &&
      (!StaticPrefs::privacy_partition_serviceWorkers() ||
       !StoragePartitioningEnabled(
           storageAllowed, window->GetExtantDoc()->CookieJarSettings()))) {
    ServiceWorkerManager::LocalizeAndReportToAllClients(
        mDescriptor.Scope(), "ServiceWorkerPostMessageStorageError",
        nsTArray<nsString>{NS_ConvertUTF8toUTF16(mDescriptor.Scope())});
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

  // Window-to-SW messages do not allow memory sharing since they are not in the
  // same agent cluster group, but we do not want to throw an error during the
  // serialization. Because of this, ServiceWorkerCloneData will propagate an
  // error message data if the SameProcess serialization is required. So that
  // the receiver (service worker) knows that it needs to throw while
  // deserialization and sharing memory objects are not propagated to the other
  // process.
  JS::CloneDataPolicy clonePolicy;
  if (nsGlobalWindowInner::Cast(window)->IsSharedMemoryAllowed()) {
    clonePolicy.allowSharedMemoryObjects();
  }

  RefPtr<ServiceWorkerCloneData> data = new ServiceWorkerCloneData();
  data->Write(aCx, aMessage, transferable, clonePolicy, aRv);
  if (aRv.Failed()) {
    return;
  }

  // The value of CloneScope() is set while StructuredCloneData::Write(). If the
  // aValue contiains a shared memory object, then the scope will be restricted
  // and thus return SameProcess. If not, it will return DifferentProcess.
  //
  // When we postMessage a shared memory object from a window to a service
  // worker, the object must be sent from a cross-origin isolated process to
  // another one. So, we mark mark this data as an error message data if the
  // scope is limited to same process.
  if (data->CloneScope() ==
      StructuredCloneHolder::StructuredCloneScope::SameProcess) {
    data->SetAsErrorMessageData();
  }

  mInner->PostMessage(std::move(data), clientInfo.ref(), clientState.ref());
}

void ServiceWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                                const StructuredSerializeOptions& aOptions,
                                ErrorResult& aRv) {
  PostMessage(aCx, aMessage, aOptions.mTransfer, aRv);
}

const ServiceWorkerDescriptor& ServiceWorker::Descriptor() const {
  return mDescriptor;
}

void ServiceWorker::DisconnectFromOwner() {
  DOMEventTargetHelper::DisconnectFromOwner();
}

void ServiceWorker::MaybeAttachToRegistration(
    ServiceWorkerRegistration* aRegistration) {
  MOZ_DIAGNOSTIC_ASSERT(aRegistration);
  MOZ_DIAGNOSTIC_ASSERT(!mRegistration);

  // If the registration no longer actually references this ServiceWorker
  // then we must be in the redundant state.
  if (!aRegistration->Descriptor().HasWorker(mDescriptor)) {
    SetState(ServiceWorkerState::Redundant);
    MaybeDispatchStateChangeEvent();
    return;
  }

  mRegistration = aRegistration;
}

}  // namespace mozilla::dom
