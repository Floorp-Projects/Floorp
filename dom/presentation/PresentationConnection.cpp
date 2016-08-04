/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationConnection.h"

#include "ControllerConnectionCollection.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/PresentationConnectionClosedEvent.h"
#include "mozilla/ErrorNames.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "PresentationConnectionList.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationConnection, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwningConnectionList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationConnection, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwningConnectionList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationConnection, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationConnection, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationConnection)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationSessionListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationConnection::PresentationConnection(nsPIDOMWindowInner* aWindow,
                                               const nsAString& aId,
                                               const nsAString& aUrl,
                                               const uint8_t aRole,
                                               PresentationConnectionList* aList)
  : DOMEventTargetHelper(aWindow)
  , mId(aId)
  , mUrl(aUrl)
  , mState(PresentationConnectionState::Connecting)
  , mOwningConnectionList(aList)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  mRole = aRole;
}

/* virtual */ PresentationConnection::~PresentationConnection()
{
}

/* static */ already_AddRefed<PresentationConnection>
PresentationConnection::Create(nsPIDOMWindowInner* aWindow,
                               const nsAString& aId,
                               const nsAString& aUrl,
                               const uint8_t aRole,
                               PresentationConnectionList* aList)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  RefPtr<PresentationConnection> connection =
    new PresentationConnection(aWindow, aId, aUrl, aRole, aList);
  if (NS_WARN_IF(!connection->Init())) {
    return nullptr;
  }

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    ControllerConnectionCollection::GetSingleton()->AddConnection(connection,
                                                                  aRole);
  }

  return connection.forget();
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

  nsresult rv = service->RegisterSessionListener(mId, mRole, this);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  rv = AddIntoLoadGroup();
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

  nsresult rv = service->UnregisterSessionListener(mId, mRole);
  NS_WARN_IF(NS_FAILED(rv));

  rv = RemoveFromLoadGroup();
  NS_WARN_IF(NS_FAILED(rv));

  if (mRole == nsIPresentationService::ROLE_CONTROLLER) {
    ControllerConnectionCollection::GetSingleton()->RemoveConnection(this,
                                                                     mRole);
  }
}

/* virtual */ void
PresentationConnection::DisconnectFromOwner()
{
  NS_WARN_IF(NS_FAILED(ProcessConnectionWentAway()));
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

void
PresentationConnection::GetUrl(nsAString& aUrl) const
{
  aUrl = mUrl;
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

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsresult rv = service->SendSessionMessage(mId, mRole, aData);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
  }
}

void
PresentationConnection::Close(ErrorResult& aRv)
{
  // It only works when the state is CONNECTED or CONNECTING.
  if (NS_WARN_IF(mState != PresentationConnectionState::Connected &&
                 mState != PresentationConnectionState::Connecting)) {
    return;
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  NS_WARN_IF(NS_FAILED(
    service->CloseSession(mId,
                          mRole,
                          nsIPresentationService::CLOSED_REASON_CLOSED)));
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

  NS_WARN_IF(NS_FAILED(service->TerminateSession(mId, mRole)));
}

bool
PresentationConnection::Equals(uint64_t aWindowId,
                               const nsAString& aId)
{
  return GetOwner() &&
         aWindowId == GetOwner()->WindowID() &&
         mId.Equals(aId);
}

NS_IMETHODIMP
PresentationConnection::NotifyStateChange(const nsAString& aSessionId,
                                          uint16_t aState,
                                          nsresult aReason)
{
  if (!aSessionId.Equals(mId)) {
    return NS_ERROR_INVALID_ARG;
  }

  PresentationConnectionState state;
  switch (aState) {
    case nsIPresentationSessionListener::STATE_CONNECTING:
      state = PresentationConnectionState::Connecting;
      break;
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

  nsresult rv = ProcessStateChanged(aReason);
  if(NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
  }

  if (mOwningConnectionList) {
    mOwningConnectionList->NotifyStateChange(aSessionId, this);
  }

  return NS_OK;
}

nsresult
PresentationConnection::ProcessStateChanged(nsresult aReason)
{
  switch (mState) {
    case PresentationConnectionState::Connecting:
      return NS_OK;
    case PresentationConnectionState::Connected: {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, NS_LITERAL_STRING("connect"), false);
      return asyncDispatcher->PostDOMEvent();
    }
    case PresentationConnectionState::Closed: {
      PresentationConnectionClosedReason reason =
        PresentationConnectionClosedReason::Closed;

      nsString errorMsg;
      if (NS_FAILED(aReason)) {
        reason = PresentationConnectionClosedReason::Error;
        nsCString name, message;

        // If aReason is not a DOM error, use error name as message.
        if (NS_FAILED(NS_GetNameAndMessageForDOMNSResult(aReason,
                                                         name,
                                                         message))) {
          mozilla::GetErrorName(aReason, message);
          message.InsertLiteral("Internal error: ", 0);
        }
        CopyUTF8toUTF16(message, errorMsg);
      }

      NS_WARN_IF(NS_FAILED(DispatchConnectionClosedEvent(reason, errorMsg)));

      return RemoveFromLoadGroup();
    }
    case PresentationConnectionState::Terminated: {
      // Ensure onterminate event is fired.
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, NS_LITERAL_STRING("terminate"), false);
      NS_WARN_IF(NS_FAILED(asyncDispatcher->PostDOMEvent()));

      nsCOMPtr<nsIPresentationService> service =
        do_GetService(PRESENTATION_SERVICE_CONTRACTID);
      if (NS_WARN_IF(!service)) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      nsresult rv = service->UnregisterSessionListener(mId, mRole);
      if(NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return RemoveFromLoadGroup();
    }
    default:
      MOZ_CRASH("Unknown presentation session state.");
      return NS_ERROR_INVALID_ARG;
  }
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

NS_IMETHODIMP
PresentationConnection::NotifyReplaced()
{
  return NotifyStateChange(mId,
                           nsIPresentationSessionListener::STATE_CLOSED,
                           NS_OK);
}

nsresult
PresentationConnection::DispatchConnectionClosedEvent(
  PresentationConnectionClosedReason aReason,
  const nsAString& aMessage)
{
  if (mState != PresentationConnectionState::Closed) {
    MOZ_ASSERT(false, "The connection state should be closed.");
    return NS_ERROR_FAILURE;
  }

  PresentationConnectionClosedEventInit init;
  init.mReason = aReason;
  init.mMessage = aMessage;

  RefPtr<PresentationConnectionClosedEvent> closedEvent =
    PresentationConnectionClosedEvent::Constructor(this,
                                                   NS_LITERAL_STRING("close"),
                                                   init);
  closedEvent->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, static_cast<Event*>(closedEvent));
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

  messageEvent->InitMessageEvent(nullptr,
                                 NS_LITERAL_STRING("message"),
                                 false, false, aData, origin,
                                 EmptyString(), nullptr, nullptr);
  messageEvent->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, static_cast<Event*>(messageEvent));
  return asyncDispatcher->PostDOMEvent();
}

nsresult
PresentationConnection::ProcessConnectionWentAway()
{
  if (mState != PresentationConnectionState::Connected &&
      mState != PresentationConnectionState::Connecting) {
    // If the state is not connected or connecting, do not need to
    // close the session.
    return NS_OK;
  }

  mState = PresentationConnectionState::Terminated;

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return service->CloseSession(
    mId, mRole, nsIPresentationService::CLOSED_REASON_WENTAWAY);
}

NS_IMETHODIMP
PresentationConnection::GetName(nsACString &aResult)
{
  aResult.AssignLiteral("about:presentation-connection");
  return NS_OK;
}

NS_IMETHODIMP
PresentationConnection::IsPending(bool* aRetval)
{
  *aRetval = true;
  return NS_OK;
}

NS_IMETHODIMP
PresentationConnection::GetStatus(nsresult* aStatus)
{
  *aStatus = NS_OK;
  return NS_OK;
}

NS_IMETHODIMP
PresentationConnection::Cancel(nsresult aStatus)
{
  nsCOMPtr<nsIRunnable> event =
    NewRunnableMethod(this, &PresentationConnection::ProcessConnectionWentAway);
  return NS_DispatchToCurrentThread(event);
}
NS_IMETHODIMP
PresentationConnection::Suspend(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
PresentationConnection::Resume(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PresentationConnection::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  *aLoadGroup = nullptr;

  nsCOMPtr<nsIDocument> doc = GetOwner() ? GetOwner()->GetExtantDoc() : nullptr;
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  *aLoadGroup = doc->GetDocumentLoadGroup().take();
  return NS_OK;
}

NS_IMETHODIMP
PresentationConnection::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
PresentationConnection::GetLoadFlags(nsLoadFlags* aLoadFlags)
{
  *aLoadFlags = nsIRequest::LOAD_BACKGROUND;
  return NS_OK;
}

NS_IMETHODIMP
PresentationConnection::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_OK;
}

nsresult
PresentationConnection::AddIntoLoadGroup()
{
  // Avoid adding to loadgroup multiple times
  if (mWeakLoadGroup) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetLoadGroup(getter_AddRefs(loadGroup));
  if(NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = loadGroup->AddRequest(this, nullptr);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mWeakLoadGroup = do_GetWeakReference(loadGroup);
  return NS_OK;
}

nsresult
PresentationConnection::RemoveFromLoadGroup()
{
  if (!mWeakLoadGroup) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = do_QueryReferent(mWeakLoadGroup);
  if (loadGroup) {
    mWeakLoadGroup = nullptr;
    return loadGroup->RemoveRequest(this, nullptr, NS_OK);
  }

  return NS_OK;
}
