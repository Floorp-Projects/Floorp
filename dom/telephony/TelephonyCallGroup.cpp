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
#include "mozilla/dom/telephony/TelephonyCallback.h"

#include "nsPrintfCString.h"

#ifdef TELEPHONY_GROUP_STATE
#undef TELEPHONY_GROUP_STATE
#endif

#define TELEPHONY_GROUP_STATE(_state) \
  (TelephonyCallGroupStateValues::strings[static_cast<int32_t>(_state)].value)

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using mozilla::ErrorResult;

TelephonyCallGroup::TelephonyCallGroup(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner)
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
  group->mState = TelephonyCallGroupState::_empty;
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
TelephonyCallGroup::ChangeState()
{
  MOZ_ASSERT(mCalls.Length() != 1);
  if (mCalls.Length() == 0) {
    ChangeStateInternal(TelephonyCallGroupState::_empty);
    return;
  }

  TelephonyCallState state = mCalls[0]->State();
  for (uint32_t i = 1; i < mCalls.Length(); i++) {
    if (mCalls[i]->State() != state) {
      MOZ_ASSERT(false, "Various call states are found in a call group!");
      ChangeStateInternal(TelephonyCallGroupState::_empty);
      return;
    }
  }

  TelephonyCallGroupState groupState = TelephonyCallGroupState::_empty;
  switch (state) {
    case TelephonyCallState::Connected:
      groupState = TelephonyCallGroupState::Connected;
      break;
    case TelephonyCallState::Held:
      groupState = TelephonyCallGroupState::Held;
      break;
    default:
      NS_NOTREACHED(nsPrintfCString("Invavild call state for a call group(%s)!",
                                    TELEPHONY_CALL_STATE(state)).get());
  }

  ChangeStateInternal(groupState);
}

void
TelephonyCallGroup::ChangeStateInternal(TelephonyCallGroupState aState)
{
  if (mState == aState) {
    return;
  }
 
  // Update Current State
  mState = aState;
 
  // Dispatch related events
  NotifyStateChanged();
}

nsresult
TelephonyCallGroup::NotifyStateChanged()
{
  // Since |mState| can be changed after statechange handler called back here,
  // we must save current state. Maybe we should figure out something smarter.
  TelephonyCallGroupState prevState = mState;

  nsresult res = DispatchCallEvent(NS_LITERAL_STRING("statechange"), nullptr);
  if (NS_FAILED(res)) {
    NS_WARNING("Failed to dispatch specific event!");
  }

  // Check whether |mState| remains the same after the statechange handler.
  // Besides, If there is no conference call at all, then we dont't have to
  // dispatch the state evnet.
  if (mState == prevState) {
    res = DispatchCallEvent(NS_ConvertASCIItoUTF16(TELEPHONY_GROUP_STATE(mState)),
                            nullptr);
    if (NS_FAILED(res)) {
      NS_WARNING("Failed to dispatch specific event!");
    }
  }

  // Notify each call within to dispatch call state change event
  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    if (NS_FAILED(mCalls[index]->NotifyStateChanged())){
      res = NS_ERROR_FAILURE;
    }
  }

  return res;
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
    return (mState == TelephonyCallGroupState::Connected &&
            aCall.State() == TelephonyCallState::Held) ||
           (mState == TelephonyCallGroupState::Held &&
            aCall.State() == TelephonyCallState::Connected);
  }

  MOZ_ASSERT(mState != TelephonyCallGroupState::_empty);

  if (aCall.ServiceId() != aSecondCall->ServiceId()) {
    return false;
  }

  if (!aSecondCall->Mergeable()) {
    return false;
  }

  return (aCall.State() == TelephonyCallState::Connected &&
          aSecondCall->State() == TelephonyCallState::Held) ||
         (aCall.State() == TelephonyCallState::Held &&
          aSecondCall->State() == TelephonyCallState::Connected);
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

  if (mState != TelephonyCallGroupState::Connected) {
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

  if (mState == TelephonyCallGroupState::_empty) {
    NS_WARNING(nsPrintfCString("We don't have a call group now!"
                               " (State: %s)",
                               TELEPHONY_GROUP_STATE(mState)).get());
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
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

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = Hold(callback);
  if (NS_WARN_IF(aRv.Failed() &&
                 !aRv.ErrorCodeIs(NS_ERROR_DOM_INVALID_STATE_ERR))) {
    return nullptr;
  }

  return promise.forget();
}

nsresult
TelephonyCallGroup::Hold(nsITelephonyCallback* aCallback)
{
  if (mState != TelephonyCallGroupState::Connected) {
    NS_WARNING(nsPrintfCString("Resume non-connected call group is rejected!"
                               " (State: %s)",
                               TELEPHONY_GROUP_STATE(mState)).get());
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = mTelephony->Service()->HoldConference(mCalls[0]->ServiceId(),
                                                      aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<Promise>
TelephonyCallGroup::Resume(ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

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
TelephonyCallGroup::Resume(nsITelephonyCallback* aCallback)
{
  if (mState != TelephonyCallGroupState::Held) {
    NS_WARNING(nsPrintfCString("Resume non-held call group is rejected!"
                               " (State: %s)",
                               TELEPHONY_GROUP_STATE(mState)).get());
    aCallback->NotifyError(NS_LITERAL_STRING("InvalidStateError"));
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv = mTelephony->Service()->ResumeConference(mCalls[0]->ServiceId(),
                                                        aCallback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
