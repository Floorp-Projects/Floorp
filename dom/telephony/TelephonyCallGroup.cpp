/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCallGroup.h"
#include "mozilla/dom/TelephonyCallGroupBinding.h"

#include "CallEvent.h"
#include "CallsList.h"
#include "Telephony.h"

USING_TELEPHONY_NAMESPACE
using namespace mozilla::dom;

TelephonyCallGroup::TelephonyCallGroup()
: mCallState(nsITelephonyProvider::CALL_STATE_UNKNOWN)
{
  SetIsDOMBinding();
}

TelephonyCallGroup::~TelephonyCallGroup()
{
}

// static
already_AddRefed<TelephonyCallGroup>
TelephonyCallGroup::Create(Telephony* aTelephony)
{
  NS_ASSERTION(aTelephony, "Null telephony!");

  nsRefPtr<TelephonyCallGroup> group = new TelephonyCallGroup();

  group->BindToOwner(aTelephony->GetOwner());

  group->mTelephony = aTelephony;
  group->mCallsList = new CallsList(aTelephony, group);

  return group.forget();
}

JSObject*
TelephonyCallGroup::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TelephonyCallGroupBinding::Wrap(aCx, aScope, this);
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

void
TelephonyCallGroup::ChangeState(uint16_t aCallState)
{
  if (mCallState == aCallState) {
    return;
  }

  nsString stateString;
  switch (aCallState) {
    case nsITelephonyProvider::CALL_STATE_UNKNOWN:
      break;
    case nsITelephonyProvider::CALL_STATE_CONNECTED:
      stateString.AssignLiteral("connected");
      break;
    case nsITelephonyProvider::CALL_STATE_HOLDING:
      stateString.AssignLiteral("holding");
      break;
    case nsITelephonyProvider::CALL_STATE_HELD:
      stateString.AssignLiteral("held");
      break;
    case nsITelephonyProvider::CALL_STATE_RESUMING:
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
  nsRefPtr<CallEvent> event = CallEvent::Create(this, aType, aCall, false, false);
  return DispatchTrustedEvent(event);
}

bool
TelephonyCallGroup::CanConference(const TelephonyCall& aCall,
                                  TelephonyCall* aSecondCall)
{
  if (!aSecondCall) {
    MOZ_ASSERT(!mCalls.IsEmpty());

    return (mCallState == nsITelephonyProvider::CALL_STATE_CONNECTED &&
            aCall.CallState() == nsITelephonyProvider::CALL_STATE_HELD) ||
           (mCallState == nsITelephonyProvider::CALL_STATE_HELD &&
            aCall.CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED);
  }

  MOZ_ASSERT(mCallState == nsITelephonyProvider::CALL_STATE_UNKNOWN);

  return (aCall.CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED &&
          aSecondCall->CallState() == nsITelephonyProvider::CALL_STATE_HELD) ||
         (aCall.CallState() == nsITelephonyProvider::CALL_STATE_HELD &&
          aSecondCall->CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED);
}

already_AddRefed<TelephonyCall>
TelephonyCallGroup::GetCall(uint32_t aCallIndex)
{
  nsRefPtr<TelephonyCall> call;

  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->CallIndex() == aCallIndex) {
      call = tempCall;
      break;
    }
  }

  return call.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TelephonyCallGroup)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TelephonyCallGroup,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TelephonyCallGroup,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTelephony)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TelephonyCallGroup)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TelephonyCallGroup, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TelephonyCallGroup, nsDOMEventTargetHelper)

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

  aRv = mTelephony->Provider()->ConferenceCall();
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

  aRv = mTelephony->Provider()->ConferenceCall();
}

void
TelephonyCallGroup::Remove(TelephonyCall& aCall, ErrorResult& aRv)
{
  if (mCallState != nsITelephonyProvider::CALL_STATE_CONNECTED) {
    NS_WARNING("Remove call from a non-connected call group. Ignore!");
    return;
  }

  uint32_t callIndex = aCall.CallIndex();
  bool hasCallToRemove = false;
  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& call = mCalls[index];
    if (call->CallIndex() == callIndex) {
      hasCallToRemove = true;
      break;
    }
  }

  if (hasCallToRemove) {
    aRv = mTelephony->Provider()->SeparateCall(callIndex);
  } else {
    NS_WARNING("Didn't have this call. Ignore!");
  }
}

void
TelephonyCallGroup::Hold(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyProvider::CALL_STATE_CONNECTED) {
    NS_WARNING("Hold non-connected call ignored!");
    return;
  }

  nsresult rv = mTelephony->Provider()->HoldConference();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeState(nsITelephonyProvider::CALL_STATE_HOLDING);
}

void
TelephonyCallGroup::Resume(ErrorResult& aRv)
{
  if (mCallState != nsITelephonyProvider::CALL_STATE_HELD) {
    NS_WARNING("Resume non-held call ignored!");
    return;
  }

  nsresult rv = mTelephony->Provider()->ResumeConference();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  ChangeState(nsITelephonyProvider::CALL_STATE_RESUMING);
}
