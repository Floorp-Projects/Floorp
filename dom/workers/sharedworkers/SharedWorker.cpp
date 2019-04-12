/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorker.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/SharedWorkerBinding.h"
#include "mozilla/dom/SharedWorkerChild.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/WorkerLoadInfo.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsPIDOMWindow.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {

nsresult PopulateContentSecurityPolicyArray(
    nsIPrincipal* aPrincipal, nsTArray<ContentSecurityPolicy>& policies,
    nsTArray<ContentSecurityPolicy>& preloadPolicies) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(policies.IsEmpty());
  MOZ_ASSERT(preloadPolicies.IsEmpty());

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = BasePrincipal::Cast(aPrincipal)->GetCsp(getter_AddRefs(csp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (csp) {
    rv = PopulateContentSecurityPolicies(csp, policies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = BasePrincipal::Cast(aPrincipal)->GetPreloadCsp(getter_AddRefs(csp));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (csp) {
    rv = PopulateContentSecurityPolicies(csp, preloadPolicies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

}  // namespace

SharedWorker::SharedWorker(nsPIDOMWindowInner* aWindow,
                           SharedWorkerChild* aActor, MessagePort* aMessagePort)
    : DOMEventTargetHelper(aWindow),
      mWindow(aWindow),
      mActor(aActor),
      mMessagePort(aMessagePort),
      mFrozen(false) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aMessagePort);
}

SharedWorker::~SharedWorker() {
  AssertIsOnMainThread();
  Close();
}

// static
already_AddRefed<SharedWorker> SharedWorker::Constructor(
    const GlobalObject& aGlobal, const nsAString& aScriptURL,
    const StringOrWorkerOptions& aOptions, ErrorResult& aRv) {
  AssertIsOnMainThread();

  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);

  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (storageAllowed == nsContentUtils::StorageAccess::eDeny) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (storageAllowed == nsContentUtils::StorageAccess::ePartitionedOrDeny &&
      !StaticPrefs::privacy_storagePrincipal_enabledForTrackers()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Assert that the principal private browsing state matches the
  // StorageAccess value.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (storageAllowed == nsContentUtils::StorageAccess::ePrivateBrowsing) {
    nsCOMPtr<Document> doc = window->GetExtantDoc();
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
    uint32_t privateBrowsingId = 0;
    if (principal) {
      MOZ_ALWAYS_SUCCEEDS(principal->GetPrivateBrowsingId(&privateBrowsingId));
    }
    MOZ_DIAGNOSTIC_ASSERT(privateBrowsingId != 0);
  }
#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  nsAutoString name;
  if (aOptions.IsString()) {
    name = aOptions.GetAsString();
  } else {
    MOZ_ASSERT(aOptions.IsWorkerOptions());
    name = aOptions.GetAsWorkerOptions().mName;
  }

  JSContext* cx = aGlobal.Context();

  WorkerLoadInfo loadInfo;
  aRv = WorkerPrivate::GetLoadInfo(cx, window, nullptr, aScriptURL, false,
                                   WorkerPrivate::OverrideLoadGroup,
                                   WorkerTypeShared, &loadInfo);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  PrincipalInfo principalInfo;
  aRv = PrincipalToPrincipalInfo(loadInfo.mPrincipal, &principalInfo);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsTArray<ContentSecurityPolicy> principalCSP;
  nsTArray<ContentSecurityPolicy> principalPreloadCSP;
  aRv = PopulateContentSecurityPolicyArray(loadInfo.mPrincipal, principalCSP,
                                           principalPreloadCSP);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  PrincipalInfo loadingPrincipalInfo;
  aRv = PrincipalToPrincipalInfo(loadInfo.mLoadingPrincipal,
                                 &loadingPrincipalInfo);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsTArray<ContentSecurityPolicy> loadingPrincipalCSP;
  nsTArray<ContentSecurityPolicy> loadingPrincipalPreloadCSP;
  aRv = PopulateContentSecurityPolicyArray(loadInfo.mLoadingPrincipal,
                                           loadingPrincipalCSP,
                                           loadingPrincipalPreloadCSP);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Here, the StoragePrincipal is always equal to the SharedWorker's principal
  // because the channel is not opened yet, and, because of this, it's not
  // classified. We need to force the correct originAttributes.
  if (storageAllowed == nsContentUtils::StorageAccess::ePartitionedOrDeny) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(window);
    if (!sop) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsIPrincipal* windowPrincipal = sop->GetPrincipal();
    if (!windowPrincipal) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsIPrincipal* windowStoragePrincipal = sop->GetEffectiveStoragePrincipal();
    if (!windowStoragePrincipal) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    if (!windowPrincipal->Equals(windowStoragePrincipal)) {
      loadInfo.mStoragePrincipal =
          BasePrincipal::Cast(loadInfo.mPrincipal)
              ->CloneForcingOriginAttributes(
                  BasePrincipal::Cast(windowStoragePrincipal)
                      ->OriginAttributesRef());
    }
  }

  PrincipalInfo storagePrincipalInfo;
  if (loadInfo.mPrincipal->Equals(loadInfo.mStoragePrincipal)) {
    storagePrincipalInfo = principalInfo;
  } else {
    aRv = PrincipalToPrincipalInfo(loadInfo.mStoragePrincipal,
                                   &storagePrincipalInfo);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  nsTArray<ContentSecurityPolicy> storagePrincipalCSP;
  nsTArray<ContentSecurityPolicy> storagePrincipalPreloadCSP;
  aRv = PopulateContentSecurityPolicyArray(loadInfo.mStoragePrincipal,
                                           storagePrincipalCSP,
                                           storagePrincipalPreloadCSP);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // We don't actually care about this MessageChannel, but we use it to 'steal'
  // its 2 connected ports.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(window);
  RefPtr<MessageChannel> channel = MessageChannel::Constructor(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MessagePortIdentifier portIdentifier;
  channel->Port1()->CloneAndDisentangle(portIdentifier);

  URIParams resolvedScriptURL;
  SerializeURI(loadInfo.mResolvedScriptURI, resolvedScriptURL);

  URIParams baseURL;
  SerializeURI(loadInfo.mBaseURI, baseURL);

  // Register this component to PBackground.
  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();

  bool isSecureContext = JS::GetIsSecureContext(js::GetContextRealm(cx));

  Maybe<IPCClientInfo> ipcClientInfo;
  Maybe<ClientInfo> clientInfo = window->GetClientInfo();
  if (clientInfo.isSome()) {
    ipcClientInfo.emplace(clientInfo.value().ToIPC());
  }

  RemoteWorkerData remoteWorkerData(
      nsString(aScriptURL), baseURL, resolvedScriptURL, name,
      loadingPrincipalInfo, loadingPrincipalCSP, loadingPrincipalPreloadCSP,
      principalInfo, principalCSP, principalPreloadCSP, storagePrincipalInfo,
      storagePrincipalCSP, storagePrincipalPreloadCSP, loadInfo.mDomain,
      isSecureContext, ipcClientInfo, storageAllowed, true /* sharedWorker */);

  PSharedWorkerChild* pActor = actorChild->SendPSharedWorkerConstructor(
      remoteWorkerData, loadInfo.mWindowID, portIdentifier);

  RefPtr<SharedWorkerChild> actor = static_cast<SharedWorkerChild*>(pActor);
  MOZ_ASSERT(actor);

  RefPtr<SharedWorker> sharedWorker =
      new SharedWorker(window, actor, channel->Port2());

  // Let's inform the window about this SharedWorker.
  nsGlobalWindowInner::Cast(window)->StoreSharedWorker(sharedWorker);
  actor->SetParent(sharedWorker);

  return sharedWorker.forget();
}

MessagePort* SharedWorker::Port() {
  AssertIsOnMainThread();
  return mMessagePort;
}

void SharedWorker::Freeze() {
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsFrozen());

  if (mFrozen) {
    return;
  }

  mFrozen = true;

  if (mActor) {
    mActor->SendFreeze();
  }
}

void SharedWorker::Thaw() {
  AssertIsOnMainThread();
  MOZ_ASSERT(IsFrozen());

  if (!mFrozen) {
    return;
  }

  mFrozen = false;

  if (mActor) {
    mActor->SendThaw();
  }

  if (!mFrozenEvents.IsEmpty()) {
    nsTArray<RefPtr<Event>> events;
    mFrozenEvents.SwapElements(events);

    for (uint32_t index = 0; index < events.Length(); index++) {
      RefPtr<Event>& event = events[index];
      MOZ_ASSERT(event);

      RefPtr<EventTarget> target = event->GetTarget();
      ErrorResult rv;
      target->DispatchEvent(*event, rv);
      if (rv.Failed()) {
        NS_WARNING("Failed to dispatch event!");
      }
    }
  }
}

void SharedWorker::QueueEvent(Event* aEvent) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(IsFrozen());

  mFrozenEvents.AppendElement(aEvent);
}

void SharedWorker::Close() {
  AssertIsOnMainThread();

  if (mWindow) {
    nsGlobalWindowInner::Cast(mWindow)->ForgetSharedWorker(this);
    mWindow = nullptr;
  }

  if (mActor) {
    mActor->SendClose();
    mActor->SetParent(nullptr);
    mActor = nullptr;
  }

  if (mMessagePort) {
    mMessagePort->Close();
  }
}

void SharedWorker::Suspend() {
  if (mActor) {
    mActor->SendSuspend();
  }
}

void SharedWorker::Resume() {
  if (mActor) {
    mActor->SendResume();
  }
}

void SharedWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                               const Sequence<JSObject*>& aTransferable,
                               ErrorResult& aRv) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mMessagePort);

  mMessagePort->PostMessage(aCx, aMessage, aTransferable, aRv);
}

NS_IMPL_ADDREF_INHERITED(SharedWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SharedWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SharedWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(SharedWorker)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SharedWorker,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SharedWorker,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject* SharedWorker::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnMainThread();

  return SharedWorker_Binding::Wrap(aCx, this, aGivenProto);
}

void SharedWorker::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  AssertIsOnMainThread();

  if (IsFrozen()) {
    RefPtr<Event> event = aVisitor.mDOMEvent;
    if (!event) {
      event = EventDispatcher::CreateEvent(aVisitor.mEvent->mOriginalTarget,
                                           aVisitor.mPresContext,
                                           aVisitor.mEvent, EmptyString());
    }

    QueueEvent(event);

    aVisitor.mCanHandle = false;
    aVisitor.SetParentTarget(nullptr, false);
    return;
  }

  DOMEventTargetHelper::GetEventTargetParent(aVisitor);
}

void SharedWorker::ErrorPropagation(nsresult aError) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<AsyncEventDispatcher> errorEvent = new AsyncEventDispatcher(
      this, NS_LITERAL_STRING("error"), CanBubble::eNo);
  errorEvent->PostDOMEvent();

  Close();
}
