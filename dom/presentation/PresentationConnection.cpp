/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/MessageEvent.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "PresentationConnection.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationConnection, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationConnection, DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationConnection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationConnection, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationConnection)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationSessionListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationConnection::PresentationConnection(nsPIDOMWindow* aWindow,
                                               const nsAString& aId,
                                               PresentationConnectionState aState)
  : DOMEventTargetHelper(aWindow)
  , mId(aId)
  , mState(aState)
{
}

/* virtual */ PresentationConnection::~PresentationConnection()
{
}

/* static */ already_AddRefed<PresentationConnection>
PresentationConnection::Create(nsPIDOMWindow* aWindow,
                               const nsAString& aId,
                               PresentationConnectionState aState)
{
  RefPtr<PresentationConnection> connection =
    new PresentationConnection(aWindow, aId, aState);
  return NS_WARN_IF(!connection->Init()) ? nullptr : connection.forget();
}

bool
PresentationConnection::Init()
{
  if (NS_WARN_IF(mId.IsEmpty())) {
    return false;
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->RegisterSessionListener(mId, this);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

void
PresentationConnection::Shutdown()
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterSessionListener(mId);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ void
PresentationConnection::DisconnectFromOwner()
{
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

/* virtual */ JSObject*
PresentationConnection::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return PresentationConnectionBinding::Wrap(aCx, this, aGivenProto);
}

void
PresentationConnection::GetId(nsAString& aId) const
{
  aId = mId;
}

PresentationConnectionState
PresentationConnection::State() const
{
  return mState;
}

void
PresentationConnection::Send(const nsAString& aData,
                          ErrorResult& aRv)
{
  // Sending is not allowed if the session is not connected.
  if (NS_WARN_IF(mState != PresentationConnectionState::Connected)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
    do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  NS_ConvertUTF16toUTF8 msgString(aData);
  rv = stream->SetData(msgString.BeginReading(), msgString.Length());
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  rv = service->SendSessionMessage(mId, stream);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
  }
}

void
PresentationConnection::Close(ErrorResult& aRv)
{
  // It only works when the state is CONNECTED.
  if (NS_WARN_IF(mState != PresentationConnectionState::Connected)) {
    return;
  }

  // TODO Bug 1210340 - Support close semantics.
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
PresentationConnection::Terminate(ErrorResult& aRv)
{
  // It only works when the state is CONNECTED.
  if (NS_WARN_IF(mState != PresentationConnectionState::Connected)) {
    return;
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  NS_WARN_IF(NS_FAILED(service->TerminateSession(mId)));
}

NS_IMETHODIMP
PresentationConnection::NotifyStateChange(const nsAString& aSessionId,
                                       uint16_t aState)
{
  if (!aSessionId.Equals(mId)) {
    return NS_ERROR_INVALID_ARG;
  }

  PresentationConnectionState state;
  switch (aState) {
    case nsIPresentationSessionListener::STATE_CONNECTED:
      state = PresentationConnectionState::Connected;
      break;
    case nsIPresentationSessionListener::STATE_CLOSED:
      state = PresentationConnectionState::Closed;
      break;
    case nsIPresentationSessionListener::STATE_TERMINATED:
      state = PresentationConnectionState::Terminated;
      break;
    default:
      NS_WARNING("Unknown presentation session state.");
      return NS_ERROR_INVALID_ARG;
  }

  if (mState == state) {
    return NS_OK;
  }

  mState = state;

  // Unregister session listener if the session is no longer connected.
  if (mState == PresentationConnectionState::Terminated) {
    nsCOMPtr<nsIPresentationService> service =
      do_GetService(PRESENTATION_SERVICE_CONTRACTID);
    if (NS_WARN_IF(!service)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult rv = service->UnregisterSessionListener(mId);
    if(NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return DispatchStateChangeEvent();
}

NS_IMETHODIMP
PresentationConnection::NotifyMessage(const nsAString& aSessionId,
                                   const nsACString& aData)
{
  if (!aSessionId.Equals(mId)) {
    return NS_ERROR_INVALID_ARG;
  }

  // No message should be expected when the session is not connected.
  if (NS_WARN_IF(mState != PresentationConnectionState::Connected)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Transform the data.
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetOwner())) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsData(cx);
  NS_ConvertUTF8toUTF16 utf16Data(aData);
  if(NS_WARN_IF(!ToJSValue(cx, utf16Data, &jsData))) {
    return NS_ERROR_FAILURE;
  }

  return DispatchMessageEvent(jsData);
}

nsresult
PresentationConnection::DispatchStateChangeEvent()
{
  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("statechange"), false);
  return asyncDispatcher->PostDOMEvent();
}

nsresult
PresentationConnection::DispatchMessageEvent(JS::Handle<JS::Value> aData)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the origin.
  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(global->PrincipalOrNull(), origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<MessageEvent> messageEvent =
    NS_NewDOMMessageEvent(this, nullptr, nullptr);

  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false,
                                      aData,
                                      origin,
                                      EmptyString(), nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  messageEvent->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, static_cast<Event*>(messageEvent));
  return asyncDispatcher->PostDOMEvent();
}
