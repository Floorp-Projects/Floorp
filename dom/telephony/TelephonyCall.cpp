/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCall.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/TelephonyCallBinding.h"

#include "mozilla/dom/DOMError.h"

#include "Telephony.h"
#include "TelephonyCallGroup.h"

using namespace mozilla::dom;
using mozilla::ErrorResult;

// static
already_AddRefed<TelephonyCall>
TelephonyCall::Create(Telephony* aTelephony, TelephonyCallId* aId,
                      uint32_t aServiceId, uint32_t aCallIndex,
                      uint16_t aCallState, bool aEmergency, bool aConference,
                      bool aSwitchable, bool aMergeable)
{
  NS_ASSERTION(aTelephony, "Null aTelephony pointer!");
  NS_ASSERTION(aId, "Null aId pointer!");
  NS_ASSERTION(aCallIndex >= 1, "Invalid call index!");

  nsRefPtr<TelephonyCall> call = new TelephonyCall(aTelephony->GetOwner());

  call->mTelephony = aTelephony;
  call->mId = aId;
  call->mServiceId = aServiceId;
  call->mCallIndex = aCallIndex;
  call->mEmergency = aEmergency;
  call->mGroup = aConference ? aTelephony->ConferenceGroup() : nullptr;
  call->mSwitchable = aSwitchable;
  call->mMergeable = aMergeable;
  call->mError = nullptr;

  call->ChangeStateInternal(aCallState, false);

  return call.forget();
}

TelephonyCall::TelephonyCall(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner),
    mLive(false)
{
}

TelephonyCall::~TelephonyCall()
{
}

JSObject*
TelephonyCall::WrapObject(JSContext* aCx)
{
  return TelephonyCallBinding::Wrap(aCx, this);
}

void
TelephonyCall::ChangeStateInternal(uint16_t aCallState, bool aFireEvents)
{
  nsRefPtr<TelephonyCall> kungFuDeathGrip(this);

  nsString stateString;
  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_DIALING:
      stateString.AssignLiteral("dialing");
      break;
    case nsITelephonyService::CALL_STATE_ALERTING:
      stateString.AssignLiteral("alerting");
      break;
    case nsITelephonyService::CALL_STATE_CONNECTING:
      stateString.AssignLiteral("connecting");
      break;
    case nsITelephonyService::CALL_STATE_CONNECTED:
      stateString.AssignLiteral("connected");
      break;
    case nsITelephonyService::CALL_STATE_HOLDING:
      stateString.AssignLiteral("holding");
      break;
    case nsITelephonyService::CALL_STATE_HELD:
      stateString.AssignLiteral("held");
      break;
    case nsITelephonyService::CALL_STATE_RESUMING:
      stateString.AssignLiteral("resuming");
      break;
    case nsITelephonyService::CALL_STATE_DISCONNECTING:
      stateString.AssignLiteral("disconnecting");
      break;
    case nsITelephonyService::CALL_STATE_DISCONNECTED:
      stateString.AssignLiteral("disconnected");
      break;
    case nsITelephonyService::CALL_STATE_INCOMING:
      stateString.AssignLiteral("incoming");
      break;
    default:
      NS_NOTREACHED("Unknown state!");
  }

  mState = stateString;
  mCallState = aCallState;

  if (aCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    NS_ASSERTION(mLive, "Should be live!");
    mLive = false;
    if (mGroup) {
      mGroup->RemoveCall(this);
    } else {
      mTelephony->RemoveCall(this);
    }
  } else if (!mLive) {
    mLive = true;
    if (mGroup) {
      mGroup->AddCall(this);
    } else {
      mTelephony->AddCall(this);
    }
  }

  if (aFireEvents) {
    nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("statechange"), this);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch specific event!");
    }

    // This can change if the statechange handler called back here... Need to
    // figure out something smarter.
    if (mCallState == aCallState) {
      rv = DispatchCallEvent(stateString, this);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch specific event!");
      }
    }
  }
}

nsresult
TelephonyCall::DispatchCallEvent(const nsAString& aType,
                                 TelephonyCall* aCall)
{
  MOZ_ASSERT(aCall);

  CallEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCall = aCall;

  nsRefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);

  return DispatchTrustedEvent(event);
}

void
TelephonyCall::NotifyError(const nsAString& aError)
{
  // Set the error string
  NS_ASSERTION(!mError, "Already have an error?");

  mError = new DOMError(GetOwner(), aError);

  // Do the state transitions
  ChangeStateInternal(nsITelephonyService::CALL_STATE_DISCONNECTED, true);

  nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("error"), this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch error event!");
  }
}

void
TelephonyCall::ChangeGroup(TelephonyCallGroup* aGroup)
{
  mGroup = aGroup;

  nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("groupchange"), this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch error event!");
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TelephonyCall,
                                   DOMEventTargetHelper,
                                   mTelephony,
                                   mError,
                                   mGroup,
                                   mId,
                                   mSecondId);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TelephonyCall)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TelephonyCall, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TelephonyCall, DOMEventTargetHelper)

// TelephonyCall WebIDL

already_AddRefed<TelephonyCallId>
TelephonyCall::Id() const
{
  nsRefPtr<TelephonyCallId> id = mId;
  return id.forget();
}

already_AddRefed<TelephonyCallId>
TelephonyCall::GetSecondId() const
{
  nsRefPtr<TelephonyCallId> id = mSecondId;
  return id.forget();
}

already_AddRefed<DOMError>
TelephonyCall::GetError() const
{
  nsRefPtr<DOMError> error = mError;
  return error.forget();
}

already_AddRefed<TelephonyCallGroup>
TelephonyCall::GetGroup() const
{
  nsRefPtr<TelephonyCallGroup> group = mGroup;
  return group.forget();
}

void
TelephonyCall::Answer(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_INCOMING) {
    NS_WARNING("Answer on non-incoming call ignored!");
    return;
  }

  nsresult rv = mTelephony->Service()->AnswerCall(mServiceId, mCallIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeStateInternal(nsITelephonyService::CALL_STATE_CONNECTING, true);
}

void
TelephonyCall::HangUp(ErrorResult& aRv)
{
  if (mCallState == nsITelephonyService::CALL_STATE_DISCONNECTING ||
      mCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    NS_WARNING("HangUp on previously disconnected call ignored!");
    return;
  }

  nsresult rv = mCallState == nsITelephonyService::CALL_STATE_INCOMING ?
                mTelephony->Service()->RejectCall(mServiceId, mCallIndex) :
                mTelephony->Service()->HangUp(mServiceId, mCallIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeStateInternal(nsITelephonyService::CALL_STATE_DISCONNECTING, true);
}

void
TelephonyCall::Hold(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING("Hold non-connected call ignored!");
    return;
  }

  if (mGroup) {
    NS_WARNING("Hold a call in conference ignored!");
    return;
  }

  if (!mSwitchable) {
    NS_WARNING("Hold a non-switchable call ignored!");
    return;
  }

  nsresult rv = mTelephony->Service()->HoldCall(mServiceId, mCallIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (mSecondId) {
    // No state transition when we switch two numbers within one TelephonyCall
    // object. Otherwise, the state here will be inconsistent with the backend
    // RIL and will never be right.
    return;
  }

  ChangeStateInternal(nsITelephonyService::CALL_STATE_HOLDING, true);
}

void
TelephonyCall::Resume(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_HELD) {
    NS_WARNING("Resume non-held call ignored!");
    return;
  }

  if (mGroup) {
    NS_WARNING("Resume a call in conference ignored!");
    return;
  }

  if (!mSwitchable) {
    NS_WARNING("Resume a non-switchable call ignored!");
    return;
  }

  nsresult rv = mTelephony->Service()->ResumeCall(mServiceId, mCallIndex);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeStateInternal(nsITelephonyService::CALL_STATE_RESUMING, true);
}
