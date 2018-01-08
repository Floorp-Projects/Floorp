/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorker.h"

#include "nsIDocument.h"
#include "nsPIDOMWindow.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"
#include "WorkerPrivate.h"

#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"

#ifdef XP_WIN
#undef PostMessage
#endif

using mozilla::ErrorResult;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {
namespace workers {

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return DOMPrefs::ServiceWorkersEnabled();
  }

  return IS_INSTANCE_OF(ServiceWorkerGlobalScope, aObj);
}

ServiceWorker::ServiceWorker(nsPIDOMWindowInner* aWindow,
                             ServiceWorkerInfo* aInfo)
  : DOMEventTargetHelper(aWindow),
    mInfo(aInfo)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aInfo);

  // This will update our state too.
  mInfo->AppendWorker(this);
}

ServiceWorker::~ServiceWorker()
{
  AssertIsOnMainThread();
  mInfo->RemoveWorker(this);
}

NS_IMPL_ADDREF_INHERITED(ServiceWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
ServiceWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnMainThread();

  return ServiceWorkerBinding::Wrap(aCx, this, aGivenProto);
}

void
ServiceWorker::GetScriptURL(nsString& aURL) const
{
  CopyUTF8toUTF16(mInfo->ScriptSpec(), aURL);
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

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
  if (!window || !window->GetExtantDoc()) {
    NS_WARNING("Trying to call post message from an invalid dom object.");
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (storageAllowed != nsContentUtils::StorageAccess::eAllow) {
    ServiceWorkerManager::LocalizeAndReportToAllClients(
      mInfo->Scope(), "ServiceWorkerPostMessageStorageError",
      nsTArray<nsString> { NS_ConvertUTF8toUTF16(mInfo->Scope()) });
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  Maybe<ClientInfo> clientInfo = window->GetClientInfo();
  Maybe<ClientState> clientState = window->GetClientState();
  if (clientInfo.isNothing() || clientState.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  ServiceWorkerPrivate* workerPrivate = mInfo->WorkerPrivate();
  aRv = workerPrivate->SendMessageEvent(aCx, aMessage, aTransferable,
                                        ClientInfoAndState(clientInfo.ref().ToIPC(),
                                                           clientState.ref().ToIPC()));
}

} // namespace workers
} // namespace dom
} // namespace mozilla
