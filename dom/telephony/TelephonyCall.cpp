/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCall.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/TelephonyCallBinding.h"
#include "mozilla/dom/telephony/TelephonyCallback.h"

#include "mozilla/dom/DOMError.h"
#include "nsPrintfCString.h"

#include "Telephony.h"
#include "TelephonyCallGroup.h"

#ifdef CONVERT_STRING_TO_NULLABLE_ENUM
#undef CONVERT_STRING_TO_NULLABLE_ENUM
#endif

#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)      \
{                                                                       \
  _enum.SetNull();                                                      \
                                                                        \
  uint32_t i = 0;                                                       \
  for (const EnumEntry* entry = _enumType##Values::strings;             \
       entry->value;                                                    \
       ++entry, ++i) {                                                  \
    if (_string.EqualsASCII(entry->value)) {                            \
      _enum.SetValue(static_cast<_enumType>(i));                        \
      break;                                                            \
    }                                                                   \
  }                                                                     \
}

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
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
TelephonyCall::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TelephonyCallBinding::Wrap(aCx, this, aGivenProto);
}

void
TelephonyCall::ChangeStateInternal(uint16_t aCallState, bool aFireEvents)
{
  nsRefPtr<TelephonyCall> kungFuDeathGrip(this);

  mCallState = aCallState;
  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_DIALING:
      mState.AssignLiteral("dialing");
      break;
    case nsITelephonyService::CALL_STATE_ALERTING:
      mState.AssignLiteral("alerting");
      break;
    case nsITelephonyService::CALL_STATE_CONNECTED:
      mState.AssignLiteral("connected");
      break;
    case nsITelephonyService::CALL_STATE_HELD:
      mState.AssignLiteral("held");
      break;
    case nsITelephonyService::CALL_STATE_DISCONNECTED:
      mState.AssignLiteral("disconnected");
      break;
    case nsITelephonyService::CALL_STATE_INCOMING:
      mState.AssignLiteral("incoming");
      break;
    default:
      NS_NOTREACHED("Unknown state!");
  }

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
      rv = DispatchCallEvent(mState, this);
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

already_AddRefed<Promise>
TelephonyCall::CreatePromise(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return promise.forget();
}

void
TelephonyCall::NotifyError(const nsAString& aError)
{
  // Set the error string
  NS_ASSERTION(!mError, "Already have an error?");

  mError = new DOMError(GetOwner(), aError);

  nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("error"), this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch error event!");
  }
}

void
TelephonyCall::UpdateDisconnectedReason(const nsAString& aDisconnectedReason)
{
  NS_ASSERTION(Substring(aDisconnectedReason,
                         aDisconnectedReason.Length() - 5).EqualsLiteral("Error"),
               "Disconnected reason should end with 'Error'");

  if (!mDisconnectedReason.IsNull()) {
    return;
  }

  // There is no 'Error' suffix in the corresponding enum. We should skip
  // that part for comparison.
  CONVERT_STRING_TO_NULLABLE_ENUM(
      Substring(aDisconnectedReason, 0, aDisconnectedReason.Length() - 5),
      TelephonyCallDisconnectedReason,
      mDisconnectedReason);

  if (!aDisconnectedReason.EqualsLiteral("NormalCallClearingError")) {
    NotifyError(aDisconnectedReason);
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

already_AddRefed<Promise>
TelephonyCall::Answer(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mCallState != nsITelephonyService::CALL_STATE_INCOMING) {
    NS_WARNING(nsPrintfCString("Answer on non-incoming call is rejected!"
                               " (State: %u)", mCallState).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->AnswerCall(mServiceId, mCallIndex, callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCall::HangUp(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    NS_WARNING(nsPrintfCString("HangUp on previously disconnected call"
                               " is rejected! (State: %u)", mCallState).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mCallState == nsITelephonyService::CALL_STATE_INCOMING ?
    mTelephony->Service()->RejectCall(mServiceId, mCallIndex, callback) :
    mTelephony->Service()->HangUpCall(mServiceId, mCallIndex, callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCall::Hold(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = Hold(callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCall::Resume(ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = Resume(callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

nsresult
TelephonyCall::Hold(nsITelephonyCallback* aCallback)
{
  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING(nsPrintfCString("Hold non-connected call is rejected!"
                               " (State: %u)", mCallState).get());
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mGroup) {
    NS_WARNING("Hold a call in conference is rejected!");
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (!mSwitchable) {
    NS_WARNING("Hold a non-switchable call is rejected!");
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = mTelephony->Service()->HoldCall(mServiceId, mCallIndex, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  if (mSecondId) {
    // No state transition when we switch two numbers within one TelephonyCall
    // object. Otherwise, the state here will be inconsistent with the backend
    // RIL and will never be right.
    return NS_OK;
  }

  return NS_OK;
}

nsresult
TelephonyCall::Resume(nsITelephonyCallback* aCallback)
{
  if (mCallState != nsITelephonyService::CALL_STATE_HELD) {
    NS_WARNING("Resume non-held call is rejected!");
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mGroup) {
    NS_WARNING("Resume a call in conference is rejected!");
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (!mSwitchable) {
    NS_WARNING("Resume a non-switchable call is rejected!");
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = mTelephony->Service()->ResumeCall(mServiceId, mCallIndex, aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}