/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/PresentationReceiverBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "PresentationReceiver.h"
#include "PresentationConnection.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationReceiver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationReceiver, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnections)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingGetConnectionPromises)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationReceiver, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConnections)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingGetConnectionPromises)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationReceiver, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationReceiver, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationRespondingListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<PresentationReceiver>
PresentationReceiver::Create(nsPIDOMWindowInner* aWindow)
{
  RefPtr<PresentationReceiver> receiver = new PresentationReceiver(aWindow);
  return NS_WARN_IF(!receiver->Init()) ? nullptr : receiver.forget();
}

PresentationReceiver::PresentationReceiver(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

PresentationReceiver::~PresentationReceiver()
{
  Shutdown();
}

bool
PresentationReceiver::Init()
{
  if (NS_WARN_IF(!GetOwner())) {
    return false;
  }
  mWindowId = GetOwner()->WindowID();

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return false;
  }

  // Register listener for incoming sessions.
  nsresult rv = service->RegisterRespondingListener(mWindowId, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

void PresentationReceiver::Shutdown()
{
  mConnections.Clear();
  mPendingGetConnectionPromises.Clear();

  // Unregister listener for incoming sessions.
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterRespondingListener(mWindowId);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ void
PresentationReceiver::DisconnectFromOwner()
{
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

/* virtual */ JSObject*
PresentationReceiver::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return PresentationReceiverBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
PresentationReceiver::GetConnection(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // If there's no existing connection, leave the promise pending until a
  // connecting request arrives from the controlling browsing context (sender).
  // http://w3c.github.io/presentation-api/#dom-presentation-getconnection
  if (!mConnections.IsEmpty()) {
    promise->MaybeResolve(mConnections[0]);
  } else {
    mPendingGetConnectionPromises.AppendElement(promise);
  }

  return promise.forget();
}

already_AddRefed<Promise>
PresentationReceiver::GetConnections(ErrorResult& aRv) const
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  promise->MaybeResolve(mConnections);
  return promise.forget();
}

NS_IMETHODIMP
PresentationReceiver::NotifySessionConnect(uint64_t aWindowId,
                                           const nsAString& aSessionId)
{
  if (NS_WARN_IF(!GetOwner())) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(aWindowId != GetOwner()->WindowID())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<PresentationConnection> connection =
    PresentationConnection::Create(GetOwner(), aSessionId,
                                   nsIPresentationService::ROLE_RECEIVER,
                                   PresentationConnectionState::Closed);
  if (NS_WARN_IF(!connection)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mConnections.AppendElement(connection);

  // Resolve pending |GetConnection| promises if any.
  if (!mPendingGetConnectionPromises.IsEmpty()) {
    for(uint32_t i = 0; i < mPendingGetConnectionPromises.Length(); i++) {
      mPendingGetConnectionPromises[i]->MaybeResolve(connection);
    }
    mPendingGetConnectionPromises.Clear();
  }

  return DispatchConnectionAvailableEvent();
}

nsresult
PresentationReceiver::DispatchConnectionAvailableEvent()
{
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("connectionavailable"), false);
  return asyncDispatcher->PostDOMEvent();
}
