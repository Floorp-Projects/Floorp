/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "PresentationSession.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationSession)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationSession, DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSession, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationSession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationSession::PresentationSession(nsPIDOMWindow* aWindow,
                                         const nsAString& aId,
                                         PresentationSessionState aState)
  : DOMEventTargetHelper(aWindow)
  , mId(aId)
  , mState(aState)
{
}

/* virtual */ PresentationSession::~PresentationSession()
{
}

/* static */ already_AddRefed<PresentationSession>
PresentationSession::Create(nsPIDOMWindow* aWindow,
                            const nsAString& aId,
                            PresentationSessionState aState)
{
  nsRefPtr<PresentationSession> session =
    new PresentationSession(aWindow, aId, aState);
  return NS_WARN_IF(!session->Init()) ? nullptr : session.forget();
}

bool
PresentationSession::Init()
{
  if (NS_WARN_IF(mId.IsEmpty())) {
    return false;
  }

  // TODO: Register listener for session state changes.

  return true;
}

void
PresentationSession::Shutdown()
{
  // TODO: Unregister listener for session state changes.
}

/* virtual */ JSObject*
PresentationSession::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return PresentationSessionBinding::Wrap(aCx, this, aGivenProto);
}

void
PresentationSession::GetId(nsAString& aId) const
{
  aId = mId;
}

PresentationSessionState
PresentationSession::State() const
{
  // TODO: Dispatch event when the value of |mState| is changed.
  return mState;
}

void
PresentationSession::Send(const nsAString& aData,
                          ErrorResult& aRv)
{
  // Sending is not allowed if the session is not connected.
  if (NS_WARN_IF(mState != PresentationSessionState::Connected)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
    do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  NS_ConvertUTF16toUTF8 msgString(aData);
  rv = stream->SetData(msgString.BeginReading(), msgString.Length());
  if(NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  // TODO: Send the message to the stream.
}

void
PresentationSession::Close()
{
  // Closing does nothing if the session is already terminated.
  if (NS_WARN_IF(mState == PresentationSessionState::Terminated)) {
    return;
  }

  // TODO: Terminate the socket.
}
