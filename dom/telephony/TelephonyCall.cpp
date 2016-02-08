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

#ifdef TELEPHONY_CALL_STATE
#undef TELEPHONY_CALL_STATE
#endif

#define TELEPHONY_CALL_STATE(_state) \
  (TelephonyCallStateValues::strings[static_cast<int32_t>(_state)].value)

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using mozilla::ErrorResult;

// static
TelephonyCallState
TelephonyCall::ConvertToTelephonyCallState(uint32_t aCallState)
{
  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_DIALING:
      return TelephonyCallState::Dialing;
    case nsITelephonyService::CALL_STATE_ALERTING:
      return TelephonyCallState::Alerting;
    case nsITelephonyService::CALL_STATE_CONNECTED:
      return TelephonyCallState::Connected;
    case nsITelephonyService::CALL_STATE_HELD:
      return TelephonyCallState::Held;
    case nsITelephonyService::CALL_STATE_DISCONNECTED:
      return TelephonyCallState::Disconnected;
    case nsITelephonyService::CALL_STATE_INCOMING:
      return TelephonyCallState::Incoming;
  }

  NS_NOTREACHED("Unknown state!");
  return TelephonyCallState::Disconnected;
}

// static
already_AddRefed<TelephonyCall>
TelephonyCall::Create(Telephony* aTelephony,
                      TelephonyCallId* aId,
                      uint32_t aServiceId,
                      uint32_t aCallIndex,
                      TelephonyCallState aState,
                      bool aEmergency,
                      bool aConference,
                      bool aSwitchable,
                      bool aMergeable)
{
  NS_ASSERTION(aTelephony, "Null aTelephony pointer!");
  NS_ASSERTION(aId, "Null aId pointer!");
  NS_ASSERTION(aCallIndex >= 1, "Invalid call index!");

  RefPtr<TelephonyCall> call = new TelephonyCall(aTelephony->GetOwner());

  call->mTelephony = aTelephony;
  call->mId = aId;
  call->mServiceId = aServiceId;
  call->mCallIndex = aCallIndex;
  call->mEmergency = aEmergency;
  call->mGroup = aConference ? aTelephony->ConferenceGroup() : nullptr;
  call->mSwitchable = aSwitchable;
  call->mMergeable = aMergeable;
  call->mError = nullptr;

  call->ChangeStateInternal(aState, false);
  return call.forget();
}

TelephonyCall::TelephonyCall(nsPIDOMWindowInner* aOwner)
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
TelephonyCall::ChangeStateInternal(TelephonyCallState aState, bool aFireEvents)
{
  RefPtr<TelephonyCall> kungFuDeathGrip(this);

  // Update current state
  mState = aState;

  // Handle disconnected calls
  if (mState == TelephonyCallState::Disconnected) {
    NS_ASSERTION(mLive, "Should be live!");
    mLive = false;
    if (mGroup) {
      mGroup->RemoveCall(this);
    } else {
      mTelephony->RemoveCall(this);
    }
  } else if (!mLive) { // Handle newly added calls
    mLive = true;
    if (mGroup) {
      mGroup->AddCall(this);
    } else {
      mTelephony->AddCall(this);
    }
  }

  // Dispatch call state changed and call state event
  if (aFireEvents) {
    NotifyStateChanged();
  }
}

nsresult
TelephonyCall::NotifyStateChanged()
{
  // Since |mState| can be changed after statechange handler called back here,
  // we must save current state. Maybe we should figure out something smarter.
  TelephonyCallState prevState = mState;

  nsresult res = DispatchCallEvent(NS_LITERAL_STRING("statechange"), this);
  if (NS_FAILED(res)) {
    NS_WARNING("Failed to dispatch specific event!");
  }

  // Check whether |mState| remains the same after the statechange handler.
  if (mState != prevState) {
    NS_WARNING("Call State has changed by statechange handler!");
    return res;
  }

  res = DispatchCallEvent(NS_ConvertASCIItoUTF16(TELEPHONY_CALL_STATE(mState)),
                          this);
  if (NS_FAILED(res)) {
    NS_WARNING("Failed to dispatch a specific event!");
  }

  return res;
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

  RefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
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
  RefPtr<TelephonyCallId> id = mId;
  return id.forget();
}

already_AddRefed<TelephonyCallId>
TelephonyCall::GetSecondId() const
{
  RefPtr<TelephonyCallId> id = mSecondId;
  return id.forget();
}

already_AddRefed<DOMError>
TelephonyCall::GetError() const
{
  RefPtr<DOMError> error = mError;
  return error.forget();
}

already_AddRefed<TelephonyCallGroup>
TelephonyCall::GetGroup() const
{
  RefPtr<TelephonyCallGroup> group = mGroup;
  return group.forget();
}

already_AddRefed<Promise>
TelephonyCall::Answer(ErrorResult& aRv)
{
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mState != TelephonyCallState::Incoming) {
    NS_WARNING(nsPrintfCString("Answer on non-incoming call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState)).get());
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
  RefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mState == TelephonyCallState::Disconnected) {
    NS_WARNING(nsPrintfCString("HangUp on a disconnected call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState)).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mState == TelephonyCallState::Incoming ?
    mTelephony->Service()->RejectCall(mServiceId, mCallIndex, callback) :
    mTelephony->Service()->HangUpCall(mServiceId, mCallIndex, callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCall::Hold(ErrorResult& aRv)
{
  RefPtr<Promise> promise = CreatePromise(aRv);
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

nsresult
TelephonyCall::Hold(nsITelephonyCallback* aCallback)
{
  if (mState != TelephonyCallState::Connected) {
    NS_WARNING(nsPrintfCString("Hold non-connected call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState)).get());
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

already_AddRefed<Promise>
TelephonyCall::Resume(ErrorResult& aRv)
{
  RefPtr<Promise> promise = CreatePromise(aRv);
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
TelephonyCall::Resume(nsITelephonyCallback* aCallback)
{
  if (mState != TelephonyCallState::Held) {
    NS_WARNING(nsPrintfCString("Resume non-held call is rejected!"
                               " (State: %s)",
                               TELEPHONY_CALL_STATE(mState)).get());
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
