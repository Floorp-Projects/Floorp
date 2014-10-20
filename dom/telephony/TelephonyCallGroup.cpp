/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCallGroup.h"
#include "mozilla/dom/TelephonyCallGroupBinding.h"

#include "CallsList.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/CallGroupErrorEvent.h"
#include "Telephony.h"

using namespace mozilla::dom;
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
TelephonyCallGroup::WrapObject(JSContext* aCx)
{
  return TelephonyCallGroupBinding::Wrap(aCx, this);
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

  nsString stateString;
  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_UNKNOWN:
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
    default:
      NS_NOTREACHED("Unknown state!");
  }

  mState = stateString;
  mCallState = aCallState;

  nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("statechange"), nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch specific event!");
  }
  if (!stateString.IsEmpty()) {
    // This can change if the statechange handler called back here... Need to
    // figure out something smarter.
    if (mCallState == aCallState) {
      rv = DispatchCallEvent(stateString, nullptr);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch specific event!");
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

bool
TelephonyCallGroup::CanConference(const TelephonyCall& aCall,
                                  TelephonyCall* aSecondCall)
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

void
TelephonyCallGroup::Add(TelephonyCall& aCall,
                        ErrorResult& aRv)
{
  if (!CanConference(aCall, nullptr)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  aRv = mTelephony->Service()->ConferenceCall(aCall.ServiceId());
}

void
TelephonyCallGroup::Add(TelephonyCall& aCall,
                        TelephonyCall& aSecondCall,
                        ErrorResult& aRv)
{
  if (!CanConference(aCall, &aSecondCall)) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  aRv = mTelephony->Service()->ConferenceCall(aCall.ServiceId());
}

void
TelephonyCallGroup::Remove(TelephonyCall& aCall, ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING("Remove call from a non-connected call group. Ignore!");
    return;
  }

  uint32_t serviceId = aCall.ServiceId();
  uint32_t callIndex = aCall.CallIndex();

  nsRefPtr<TelephonyCall> call;

  call = GetCall(serviceId, callIndex);
  if (call) {
    aRv = mTelephony->Service()->SeparateCall(serviceId, callIndex);
  } else {
    NS_WARNING("Didn't have this call. Ignore!");
  }
}

already_AddRefed<Promise>
TelephonyCallGroup::HangUp(ErrorResult& aRv)
{
  MOZ_ASSERT(!mCalls.IsEmpty());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  aRv = mTelephony->Service()->HangUpConference(mCalls[0]->ServiceId(),
                                                callback);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  return promise.forget();
}

void
TelephonyCallGroup::Hold(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_CONNECTED) {
    NS_WARNING("Hold non-connected call ignored!");
    return;
  }

  MOZ_ASSERT(!mCalls.IsEmpty());

  nsresult rv = mTelephony->Service()->HoldConference(mCalls[0]->ServiceId());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeState(nsITelephonyService::CALL_STATE_HOLDING);
}

void
TelephonyCallGroup::Resume(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyService::CALL_STATE_HELD) {
    NS_WARNING("Resume non-held call ignored!");
    return;
  }

  MOZ_ASSERT(!mCalls.IsEmpty());

  nsresult rv = mTelephony->Service()->ResumeConference(mCalls[0]->ServiceId());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeState(nsITelephonyService::CALL_STATE_RESUMING);
}
