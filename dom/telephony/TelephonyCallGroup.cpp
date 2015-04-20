/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCallGroup.h"

#include "CallsList.h"
#include "Telephony.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/CallGroupErrorEvent.h"
#include "mozilla/dom/TelephonyCallGroupBinding.h"
#include "mozilla/dom/telephony/TelephonyCallback.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using mozilla::ErrorResult;

TelephonyCallGroup::TelephonyCallGroup(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner)
  , mCallState(nsITelephonyService::CALL_STATE_UNKNOWN)
{
}

TelephonyCallGroup::~TelephonyCallGroup()
{
}

// static
already_AddRefed<TelephonyCallGroup>
TelephonyCallGroup::Create(Telephony* aTelephony)
{
  NS_ASSERTION(aTelephony, "Null telephony!");

  nsRefPtr<TelephonyCallGroup> group =
    new TelephonyCallGroup(aTelephony->GetOwner());

  group->mTelephony = aTelephony;
  group->mCallsList = new CallsList(aTelephony, group);

  return group.forget();
}

JSObject*
TelephonyCallGroup::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TelephonyCallGroupBinding::Wrap(aCx, this, aGivenProto);
}

void
TelephonyCallGroup::AddCall(TelephonyCall* aCall)
{
  NS_ASSERTION(!mCalls.Contains(aCall), "Already know about this one!");
  mCalls.AppendElement(aCall);
  aCall->ChangeGroup(this);
  NotifyCallsChanged(aCall);
}

void
TelephonyCallGroup::RemoveCall(TelephonyCall* aCall)
{
  NS_ASSERTION(mCalls.Contains(aCall), "Didn't know about this one!");
  mCalls.RemoveElement(aCall);
  aCall->ChangeGroup(nullptr);
  NotifyCallsChanged(aCall);
}

nsresult
TelephonyCallGroup::NotifyError(const nsAString& aName, const nsAString& aMessage)
{
  CallGroupErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mName = aName;
  init.mMessage = aMessage;

  nsRefPtr<CallGroupErrorEvent> event =
    CallGroupErrorEvent::Constructor(this, NS_LITERAL_STRING("error"), init);

  return DispatchTrustedEvent(event);
}

void
TelephonyCallGroup::ChangeState(uint16_t aCallState)
{
  if (mCallState == aCallState) {
    return;
  }
  // Update the internal state.
  mCallState = aCallState;

  // Indicate whether the external state should be changed.
  bool externalStateChanged = true;
  switch (aCallState) {
    // These states are used internally to mark this CallGroup is currently
    // being controlled, and we should block consecutive requests of the same
    // type according to these states.
    case nsITelephonyService::CALL_STATE_HOLDING:
    case nsITelephonyService::CALL_STATE_RESUMING:
      externalStateChanged = false;
      break;
    // These states will be translated into literal strings which are used to
    // show the current status of this CallGroup.
    case nsITelephonyService::CALL_STATE_UNKNOWN:
      mState.AssignLiteral("");
      break;
    case nsITelephonyService::CALL_STATE_CONNECTED:
      mState.AssignLiteral("connected");
      break;
    case nsITelephonyService::CALL_STATE_HELD:
      mState.AssignLiteral("held");
      break;
    default:
      NS_NOTREACHED("Unknown state!");
  }

  if (externalStateChanged) {
    nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("statechange"), nullptr);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch specific event!");
    }
    if (!mState.IsEmpty()) {
      // This can change if the statechange handler called back here... Need to
      // figure out something smarter.
      if (mCallState == aCallState) {
        rv = DispatchCallEvent(mState, nullptr);
        if (NS_FAILED(rv)) {
          NS_WARNING("Failed to dispatch specific event!");
        }
      }
    }
  }

  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall> call = mCalls[index];
    call->ChangeState(aCallState);

    MOZ_ASSERT(call->CallState() == aCallState);
  }
}

nsresult
TelephonyCallGroup::NotifyCallsChanged(TelephonyCall* aCall)
{
  return DispatchCallEvent(NS_LITERAL_STRING("callschanged"), aCall);
}

nsresult
TelephonyCallGroup::DispatchCallEvent(const nsAString& aType,
                                      TelephonyCall* aCall)
{
  CallEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCall = aCall;

  nsRefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

already_AddRefed<Promise>
TelephonyCallGroup::CreatePromise(ErrorResult& aRv)
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

bool
TelephonyCallGroup::CanConference(const TelephonyCall& aCall,
                                  const TelephonyCall* aSecondCall)
{
  if (!aCall.Mergeable()) {
    return false;
  }

  if (!aSecondCall) {
    MOZ_ASSERT(!mCalls.IsEmpty());

    return (mCallState == nsITelephonyService::CALL_STATE_CONNECTED &&
            aCall.CallState() == nsITelephonyService::CALL_STATE_HELD) ||
           (mCallState == nsITelephonyService::CALL_STATE_HELD &&
            aCall.CallState() == nsITelephonyService::CALL_STATE_CONNECTED);
  }

  MOZ_ASSERT(mCallState == nsITelephonyService::CALL_STATE_UNKNOWN);

  if (aCall.ServiceId() != aSecondCall->ServiceId()) {
    return false;
  }

  if (!aSecondCall->Mergeable()) {
    return false;
  }

  return (aCall.CallState() == nsITelephonyService::CALL_STATE_CONNECTED &&
          aSecondCall->CallState() == nsITelephonyService::CALL_STATE_HELD) ||
         (aCall.CallState() == nsITelephonyService::CALL_STATE_HELD &&
          aSecondCall->CallState() == nsITelephonyService::CALL_STATE_CONNECTED);
}

already_AddRefed<TelephonyCall>
TelephonyCallGroup::GetCall(uint32_t aServiceId, uint32_t aCallIndex)
{
  nsRefPtr<TelephonyCall> call;

  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->ServiceId() == aServiceId &&
        tempCall->CallIndex() == aCallIndex) {
      call = tempCall;
      break;
    }
  }

  return call.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TelephonyCallGroup)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TelephonyCallGroup,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TelephonyCallGroup,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTelephony)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TelephonyCallGroup)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TelephonyCallGroup, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TelephonyCallGroup, DOMEventTargetHelper)

// WebIDL
already_AddRefed<CallsList>
TelephonyCallGroup::Calls() const
{
  nsRefPtr<CallsList> list = mCallsList;
  return list.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::Add(TelephonyCall& aCall,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (!CanConference(aCall, nullptr)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->ConferenceCall(aCall.ServiceId(), callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::Add(TelephonyCall& aCall,
                        TelephonyCall& aSecondCall,
                        ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (!CanConference(aCall, &aSecondCall)) {
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->ConferenceCall(aCall.ServiceId(), callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::Remove(TelephonyCall& aCall, ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING("Remove call from a non-connected call group. Ignore!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  uint32_t serviceId = aCall.ServiceId();
  uint32_t callIndex = aCall.CallIndex();

  nsRefPtr<TelephonyCall> call = GetCall(serviceId, callIndex);
  if (!call) {
    NS_WARNING("Didn't have this call. Ignore!");
    promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->SeparateCall(serviceId, callIndex, callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::HangUp(ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->HangUpConference(mCalls[0]->ServiceId(),
                                                callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::Hold(ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING("Holding a non-connected call is rejected!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->HoldConference(mCalls[0]->ServiceId(),
                                              callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  ChangeState(nsITelephonyService::CALL_STATE_HOLDING);
  return promise.forget();
}

already_AddRefed<Promise>
TelephonyCallGroup::Resume(ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsRefPtr<Promise> promise = CreatePromise(aRv);
  if (!promise) {
    return nullptr;
  }

  if (mCallState != nsITelephonyService::CALL_STATE_HELD) {
    NS_WARNING("Resuming a non-held call is rejected!");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->ResumeConference(mCalls[0]->ServiceId(),
                                                callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  ChangeState(nsITelephonyService::CALL_STATE_RESUMING);
  return promise.forget();
}
