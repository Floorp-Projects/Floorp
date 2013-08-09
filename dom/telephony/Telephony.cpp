/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telephony.h"
#include "mozilla/dom/TelephonyBinding.h"

#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsIPermissionManager.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsTArrayHelpers.h"
#include "nsThreadUtils.h"

#include "CallEvent.h"
#include "CallsList.h"
#include "TelephonyCall.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

USING_TELEPHONY_NAMESPACE
using namespace mozilla::dom;

namespace {

typedef nsAutoTArray<Telephony*, 2> TelephonyList;

TelephonyList* gTelephonyList;

} // anonymous namespace

class Telephony::Listener : public nsITelephonyListener
{
  Telephony* mTelephony;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSITELEPHONYLISTENER(mTelephony)

  Listener(Telephony* aTelephony)
    : mTelephony(aTelephony)
  {
    MOZ_ASSERT(mTelephony);
  }

  void
  Disconnect()
  {
    MOZ_ASSERT(mTelephony);
    mTelephony = nullptr;
  }
};

class Telephony::EnumerationAck : public nsRunnable
{
  nsRefPtr<Telephony> mTelephony;

public:
  EnumerationAck(Telephony* aTelephony)
  : mTelephony(aTelephony)
  {
    MOZ_ASSERT(mTelephony);
  }

  NS_IMETHOD Run()
  {
    mTelephony->NotifyCallsChanged(nullptr);
    return NS_OK;
  }
};

Telephony::Telephony()
: mActiveCall(nullptr), mEnumerated(false)
{
  if (!gTelephonyList) {
    gTelephonyList = new TelephonyList();
  }

  gTelephonyList->AppendElement(this);

  SetIsDOMBinding();
}

Telephony::~Telephony()
{
  if (mListener) {
    mListener->Disconnect();

    if (mProvider) {
      mProvider->UnregisterTelephonyMsg(mListener);
    }
  }

  NS_ASSERTION(gTelephonyList, "This should never be null!");
  NS_ASSERTION(gTelephonyList->Contains(this), "Should be in the list!");

  if (gTelephonyList->Length() == 1) {
    delete gTelephonyList;
    gTelephonyList = nullptr;
  }
  else {
    gTelephonyList->RemoveElement(this);
  }
}

JSObject*
Telephony::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TelephonyBinding::Wrap(aCx, aScope, this);
}

// static
already_AddRefed<Telephony>
Telephony::Create(nsPIDOMWindow* aOwner, ErrorResult& aRv)
{
  NS_ASSERTION(aOwner, "Null owner!");

  nsCOMPtr<nsITelephonyProvider> ril =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  if (!ril) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  if (!sgo) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  if (!scriptContext) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Telephony> telephony = new Telephony();

  telephony->BindToOwner(aOwner);

  telephony->mProvider = ril;
  telephony->mListener = new Listener(telephony);
  telephony->mCallsList = new CallsList(telephony);

  nsresult rv = ril->EnumerateCalls(telephony->mListener);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  rv = ril->RegisterTelephonyMsg(telephony->mListener);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return telephony.forget();
}

already_AddRefed<TelephonyCall>
Telephony::CreateNewDialingCall(const nsAString& aNumber)
{
  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber,
                          nsITelephonyProvider::CALL_STATE_DIALING);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  return call.forget();
}

void
Telephony::NoteDialedCallFromOtherInstance(const nsAString& aNumber)
{
  // We don't need to hang on to this call object, it is held alive by mCalls.
  nsRefPtr<TelephonyCall> call = CreateNewDialingCall(aNumber);
}

nsresult
Telephony::NotifyCallsChanged(TelephonyCall* aCall)
{
  if (aCall) {
    if (aCall->CallState() == nsITelephonyProvider::CALL_STATE_DIALING ||
        aCall->CallState() == nsITelephonyProvider::CALL_STATE_ALERTING ||
        aCall->CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED) {
      NS_ASSERTION(!mActiveCall, "Already have an active call!");
      mActiveCall = aCall;
    } else if (mActiveCall && mActiveCall->CallIndex() == aCall->CallIndex()) {
      mActiveCall = nullptr;
    }
  }

  return DispatchCallEvent(NS_LITERAL_STRING("callschanged"), aCall);
}

already_AddRefed<TelephonyCall>
Telephony::DialInternal(bool isEmergency,
                        const nsAString& aNumber,
                        ErrorResult& aRv)
{
  if (aNumber.IsEmpty()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    const nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->IsOutgoing() &&
        tempCall->CallState() < nsITelephonyProvider::CALL_STATE_CONNECTED) {
      // One call has been dialed already and we only support one outgoing call
      // at a time.
      NS_WARNING("Only permitted to dial one call at a time!");
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }
  }

  nsresult rv;
  if (isEmergency) {
    rv = mProvider->DialEmergency(aNumber);
  } else {
    rv = mProvider->Dial(aNumber);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsRefPtr<TelephonyCall> call = CreateNewDialingCall(aNumber);

  // Notify other telephony objects that we just dialed.
  for (uint32_t index = 0; index < gTelephonyList->Length(); index++) {
    Telephony*& telephony = gTelephonyList->ElementAt(index);
    if (telephony != this) {
      nsRefPtr<Telephony> kungFuDeathGrip = telephony;
      telephony->NoteDialedCallFromOtherInstance(aNumber);
    }
  }

  return call.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Telephony)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Telephony,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Telephony,
                                                nsDOMEventTargetHelper)
  tmp->mActiveCall = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Telephony)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Telephony, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Telephony, nsDOMEventTargetHelper)

NS_IMPL_ISUPPORTS1(Telephony::Listener, nsITelephonyListener)

// Telephony WebIDL

already_AddRefed<TelephonyCall>
Telephony::Dial(const nsAString& aNumber, ErrorResult& aRv)
{
  nsRefPtr<TelephonyCall> call = DialInternal(false, aNumber, aRv);
  return call.forget();
}

already_AddRefed<TelephonyCall>
Telephony::DialEmergency(const nsAString& aNumber, ErrorResult& aRv)
{
  nsRefPtr<TelephonyCall> call = DialInternal(true, aNumber, aRv);
  return call.forget();
}

bool
Telephony::GetMuted(ErrorResult& aRv) const
{
  bool muted = false;
  aRv = mProvider->GetMicrophoneMuted(&muted);

  return muted;
}

void
Telephony::SetMuted(bool aMuted, ErrorResult& aRv)
{
  aRv = mProvider->SetMicrophoneMuted(aMuted);
}

bool
Telephony::GetSpeakerEnabled(ErrorResult& aRv) const
{
  bool enabled = false;
  aRv = mProvider->GetSpeakerEnabled(&enabled);

  return enabled;
}

void
Telephony::SetSpeakerEnabled(bool aEnabled, ErrorResult& aRv)
{
  aRv = mProvider->SetSpeakerEnabled(aEnabled);
}

already_AddRefed<TelephonyCall>
Telephony::GetActive() const
{
  nsCOMPtr<TelephonyCall> activeCall = mActiveCall;
  return activeCall.forget();
}

already_AddRefed<CallsList>
Telephony::Calls() const
{
  nsRefPtr<CallsList> list = mCallsList;
  return list.forget();
}

void
Telephony::StartTone(const nsAString& aDTMFChar, ErrorResult& aRv)
{
  if (aDTMFChar.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    return;
  }

  if (aDTMFChar.Length() > 1) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mProvider->StartTone(aDTMFChar);
}

void
Telephony::StopTone(ErrorResult& aRv)
{
  aRv = mProvider->StopTone();
}

// EventTarget

void
Telephony::EventListenerAdded(nsIAtom* aType)
{
  if (aType == nsGkAtoms::oncallschanged) {
    // Fire oncallschanged on the next tick if the calls array is ready.
    EnqueueEnumerationAck();
  }
}

// nsITelephonyListener

NS_IMETHODIMP
Telephony::CallStateChanged(uint32_t aCallIndex, uint16_t aCallState,
                            const nsAString& aNumber, bool aIsActive,
                            bool aIsOutgoing, bool aIsEmergency)
{
  NS_ASSERTION(aCallIndex != kOutgoingPlaceholderCallIndex,
               "This should never happen!");

  nsRefPtr<TelephonyCall> modifiedCall;
  nsRefPtr<TelephonyCall> outgoingCall;

  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->CallIndex() == kOutgoingPlaceholderCallIndex) {
      NS_ASSERTION(!outgoingCall, "More than one outgoing call not supported!");
      NS_ASSERTION(tempCall->CallState() ==
                   nsITelephonyProvider::CALL_STATE_DIALING,
                   "Something really wrong here!");
      // Stash this for later, we may need it if aCallIndex doesn't match one of
      // our other calls.
      outgoingCall = tempCall;
    } else if (tempCall->CallIndex() == aCallIndex) {
      // We already know about this call so just update its state.
      modifiedCall = tempCall;
      outgoingCall = nullptr;
      break;
    }
  }

  // If nothing matched above and the call state isn't incoming but we do have
  // an outgoing call then we must be seeing a status update for our outgoing
  // call.
  if (!modifiedCall &&
      aCallState != nsITelephonyProvider::CALL_STATE_INCOMING &&
      outgoingCall) {
    outgoingCall->UpdateCallIndex(aCallIndex);
    outgoingCall->UpdateEmergency(aIsEmergency);
    modifiedCall.swap(outgoingCall);
  }

  if (modifiedCall) {
    // See if this should replace our current active call.
    if (aIsActive) {
        mActiveCall = modifiedCall;
    } else if (mActiveCall && mActiveCall->CallIndex() == aCallIndex) {
      mActiveCall = nullptr;
    }

    // Change state.
    modifiedCall->ChangeState(aCallState);

    return NS_OK;
  }

  // Didn't know anything about this call before now.

  if (aCallState == nsITelephonyProvider::CALL_STATE_DISCONNECTED) {
    // Do nothing since we didn't know anything about it before now and it's
    // been ended already.
    return NS_OK;
  }

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber, aCallState, aCallIndex, aIsEmergency);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  if (aCallState == nsITelephonyProvider::CALL_STATE_INCOMING) {
    nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("incoming"), call);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallStateComplete()
{
  MOZ_ASSERT(!mEnumerated);

  mEnumerated = true;

  if (NS_FAILED(NotifyCallsChanged(nullptr))) {
    NS_WARNING("Failed to notify calls changed!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(uint32_t aCallIndex, uint16_t aCallState,
                              const nsAString& aNumber, bool aIsActive,
                              bool aIsOutgoing, bool aIsEmergency,
                              bool* aContinue)
{
  // Make sure we don't somehow add duplicates.
  for (uint32_t index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->CallIndex() == aCallIndex) {
      // We have the call already. Skip it.
      *aContinue = true;
      return NS_OK;
    }
  }

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber, aCallState, aCallIndex, aIsEmergency);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  *aContinue = true;
  return NS_OK;
}

NS_IMETHODIMP
Telephony::SupplementaryServiceNotification(int32_t aCallIndex,
                                            uint16_t aNotification)
{
  nsRefPtr<TelephonyCall> associatedCall;
  if (!mCalls.IsEmpty() && aCallIndex != -1) {
    for (uint32_t index = 0; index < mCalls.Length(); index++) {
      nsRefPtr<TelephonyCall>& call = mCalls[index];
      if (call->CallIndex() == uint32_t(aCallIndex)) {
        associatedCall = call;
        break;
      }
    }
  }

  nsresult rv;
  switch (aNotification) {
    case nsITelephonyProvider::NOTIFICATION_REMOTE_HELD:
      rv = DispatchCallEvent(NS_LITERAL_STRING("remoteheld"), associatedCall);
      break;
    case nsITelephonyProvider::NOTIFICATION_REMOTE_RESUMED:
      rv = DispatchCallEvent(NS_LITERAL_STRING("remoteresumed"), associatedCall);
      break;
    default:
      NS_ERROR("Got a bad notification!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyError(int32_t aCallIndex,
                       const nsAString& aError)
{
  nsRefPtr<TelephonyCall> callToNotify;
  if (!mCalls.IsEmpty()) {
    // The connection is not established yet. Get the latest dialing call object.
    if (aCallIndex == -1) {
      nsRefPtr<TelephonyCall>& lastCall = mCalls[mCalls.Length() - 1];
      if (lastCall->CallIndex() == kOutgoingPlaceholderCallIndex) {
        callToNotify = lastCall;
      }
    } else {
      // The connection has been established. Get the failed call.
      for (uint32_t index = 0; index < mCalls.Length(); index++) {
        nsRefPtr<TelephonyCall>& call = mCalls[index];
        if (call->CallIndex() == aCallIndex) {
          callToNotify = call;
          break;
        }
      }
    }
  }

  if (!callToNotify) {
    NS_ERROR("Don't call me with a bad call index!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mActiveCall && mActiveCall->CallIndex() == callToNotify->CallIndex()) {
    mActiveCall = nullptr;
  }

  // Set the call state to 'disconnected' and remove it from the calls list.
  callToNotify->NotifyError(aError);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyCdmaCallWaiting(const nsAString& aNumber)
{
  MOZ_ASSERT(mActiveCall &&
             mActiveCall->CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED);

  nsRefPtr<TelephonyCall> callToNotify = mActiveCall;
  callToNotify->UpdateSecondNumber(aNumber);
  DispatchCallEvent(NS_LITERAL_STRING("callschanged"), callToNotify);
  return NS_OK;
}

nsresult
Telephony::DispatchCallEvent(const nsAString& aType,
                             TelephonyCall* aCall)
{
  // The call may be null in following cases:
  //   1. callschanged when notifying enumeration being completed
  //   2. remoteheld/remoteresumed.
  MOZ_ASSERT(aCall ||
             aType.EqualsLiteral("callschanged") ||
             aType.EqualsLiteral("remoteheld") ||
             aType.EqualsLiteral("remtoeresumed"));

  nsRefPtr<CallEvent> event = CallEvent::Create(this, aType, aCall, false, false);

  return DispatchTrustedEvent(event);
}

void
Telephony::EnqueueEnumerationAck()
{
  if (!mEnumerated) {
    return;
  }

  nsCOMPtr<nsIRunnable> task = new EnumerationAck(this);
  if (NS_FAILED(NS_DispatchToCurrentThread(task))) {
    NS_WARNING("Failed to dispatch to current thread!");
  }
}

/* static */
bool
Telephony::CheckPermission(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow && aWindow->IsInnerWindow());

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission;
  nsresult rv =
    permMgr->TestPermissionFromWindow(aWindow, "telephony", &permission);
  NS_ENSURE_SUCCESS(rv, false);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return false;
  }

  return true;
}
