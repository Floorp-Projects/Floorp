/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TelephonyCall.h"

#include "nsDOMClassInfo.h"

#include "CallEvent.h"
#include "Telephony.h"

USING_TELEPHONY_NAMESPACE

// static
already_AddRefed<TelephonyCall>
TelephonyCall::Create(Telephony* aTelephony, const nsAString& aNumber,
                      PRUint16 aCallState, PRUint32 aCallIndex)
{
  NS_ASSERTION(aTelephony, "Null pointer!");
  NS_ASSERTION(!aNumber.IsEmpty(), "Empty number!");
  NS_ASSERTION(aCallIndex >= 1, "Invalid call index!");

  nsRefPtr<TelephonyCall> call = new TelephonyCall();

  call->BindToOwner(aTelephony->GetOwner());

  call->mTelephony = aTelephony;
  call->mNumber = aNumber;
  call->mCallIndex = aCallIndex;

  call->ChangeStateInternal(aCallState, false);

  return call.forget();
}

void
TelephonyCall::ChangeStateInternal(PRUint16 aCallState, bool aFireEvents)
{
  nsRefPtr<TelephonyCall> kungFuDeathGrip(this);

  nsString stateString;
  switch (aCallState) {
    case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
      stateString.AssignLiteral("dialing");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
      stateString.AssignLiteral("ringing");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_BUSY:
      stateString.AssignLiteral("busy");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTING:
      stateString.AssignLiteral("connecting");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
      stateString.AssignLiteral("connected");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_HOLDING:
      stateString.AssignLiteral("holding");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_HELD:
      stateString.AssignLiteral("held");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_RESUMING:
      stateString.AssignLiteral("resuming");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTING:
      stateString.AssignLiteral("disconnecting");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
      stateString.AssignLiteral("disconnected");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
      stateString.AssignLiteral("incoming");
      break;
    default:
      NS_NOTREACHED("Unknown state!");
  }

  mState = stateString;
  mCallState = aCallState;

  if (aCallState == nsIRadioInterfaceLayer::CALL_STATE_DIALING) {
    mOutgoing = true;
  }

  if (aCallState == nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED) {
    NS_ASSERTION(mLive, "Should be live!");
    mTelephony->RemoveCall(this);
    mLive = false;
  } else if (!mLive) {
    mTelephony->AddCall(this);
    mLive = true;
  }

  if (aFireEvents) {
    nsRefPtr<CallEvent> event = CallEvent::Create(this);
    NS_ASSERTION(event, "This should never fail!");

    if (NS_FAILED(event->Dispatch(ToIDOMEventTarget(),
                                  NS_LITERAL_STRING("statechange")))) {
      NS_WARNING("Failed to dispatch statechange event!");
    }

    // This can change if the statechange handler called back here... Need to
    // figure out something smarter.
    if (mCallState == aCallState) {
      event = CallEvent::Create(this);
      NS_ASSERTION(event, "This should never fail!");

      if (NS_FAILED(event->Dispatch(ToIDOMEventTarget(), stateString))) {
        NS_WARNING("Failed to dispatch specific event!");
      }
    }
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TelephonyCall)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TelephonyCall,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_PTR(tmp->mTelephony->ToISupports(),
                                               Telephony, "mTelephony")
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(statechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(dialing)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(ringing)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(busy)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(connecting)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(connected)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(disconnecting)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(disconnected)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(incoming)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TelephonyCall,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTelephony)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(statechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(dialing)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(ringing)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(busy)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(connecting)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(connected)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(disconnecting)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(disconnected)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(incoming)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TelephonyCall)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTelephonyCall)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TelephonyCall)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TelephonyCall, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TelephonyCall, nsDOMEventTargetHelper)

DOMCI_DATA(TelephonyCall, TelephonyCall)

NS_IMETHODIMP
TelephonyCall::GetNumber(nsAString& aNumber)
{
  aNumber.Assign(mNumber);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCall::GetState(nsAString& aState)
{
  aState.Assign(mState);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCall::Answer()
{
  if (mCallState != nsIRadioInterfaceLayer::CALL_STATE_INCOMING) {
    NS_WARNING("Answer on non-incoming call ignored!");
    return NS_OK;
  }

  nsresult rv = mTelephony->RIL()->AnswerCall(mCallIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  ChangeStateInternal(nsIRadioInterfaceLayer::CALL_STATE_CONNECTING, true);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCall::HangUp()
{
  if (mCallState == nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTING ||
      mCallState == nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED) {
    NS_WARNING("HangUp on previously disconnected call ignored!");
    return NS_OK;
  }

  nsresult rv = mCallState == nsIRadioInterfaceLayer::CALL_STATE_INCOMING ?
                mTelephony->RIL()->RejectCall(mCallIndex) :
                mTelephony->RIL()->HangUp(mCallIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  ChangeStateInternal(nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTING, true);
  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(TelephonyCall, statechange)
NS_IMPL_EVENT_HANDLER(TelephonyCall, dialing)
NS_IMPL_EVENT_HANDLER(TelephonyCall, ringing)
NS_IMPL_EVENT_HANDLER(TelephonyCall, busy)
NS_IMPL_EVENT_HANDLER(TelephonyCall, connecting)
NS_IMPL_EVENT_HANDLER(TelephonyCall, connected)
NS_IMPL_EVENT_HANDLER(TelephonyCall, disconnecting)
NS_IMPL_EVENT_HANDLER(TelephonyCall, disconnected)
NS_IMPL_EVENT_HANDLER(TelephonyCall, incoming)
