/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telephony.h"
#include "mozilla/dom/TelephonyBinding.h"
#include "mozilla/dom/Promise.h"

#include "nsIURI.h"
#include "nsPIDOMWindow.h"
#include "nsIPermissionManager.h"

#include "mozilla/dom/UnionTypes.h"
#include "mozilla/Preferences.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "CallEvent.h"
#include "CallsList.h"
#include "TelephonyCall.h"
#include "TelephonyCallGroup.h"

using namespace mozilla::dom;
using mozilla::ErrorResult;
using mozilla::dom::telephony::kOutgoingPlaceholderCallIndex;

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

  virtual ~Listener() {}

  void
  Disconnect()
  {
    MOZ_ASSERT(mTelephony);
    mTelephony = nullptr;
  }
};

class Telephony::Callback : public nsITelephonyCallback
{
  nsRefPtr<Telephony> mTelephony;
  nsRefPtr<Promise> mPromise;
  uint32_t mServiceId;
  nsString mNumber;

public:
  NS_DECL_ISUPPORTS

  Callback(Telephony* aTelephony, Promise* aPromise, uint32_t aServiceId,
           const nsAString& aNumber)
    : mTelephony(aTelephony), mPromise(aPromise), mServiceId(aServiceId),
      mNumber(aNumber)
  {
    MOZ_ASSERT(mTelephony);
  }

  virtual ~Callback() {}

  NS_IMETHODIMP
  NotifyDialError(const nsAString& aError)
  {
    mPromise->MaybeReject(aError);
    return NS_OK;
  }

  NS_IMETHODIMP
  NotifyDialSuccess()
  {
    nsRefPtr<TelephonyCall> call =
      mTelephony->CreateNewDialingCall(mServiceId, mNumber);

    mPromise->MaybeResolve(call);
    return NS_OK;
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

Telephony::Telephony(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner), mActiveCall(nullptr), mEnumerated(false)
{
  if (!gTelephonyList) {
    gTelephonyList = new TelephonyList();
  }

  gTelephonyList->AppendElement(this);
}

Telephony::~Telephony()
{
  Shutdown();

  NS_ASSERTION(gTelephonyList, "This should never be null!");
  NS_ASSERTION(gTelephonyList->Contains(this), "Should be in the list!");

  if (gTelephonyList->Length() == 1) {
    delete gTelephonyList;
    gTelephonyList = nullptr;
  } else {
    gTelephonyList->RemoveElement(this);
  }
}

void
Telephony::Shutdown()
{
  if (mListener) {
    mListener->Disconnect();

    if (mProvider) {
      mProvider->UnregisterListener(mListener);
      mProvider = nullptr;
    }

    mListener = nullptr;
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
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
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

  nsRefPtr<Telephony> telephony = new Telephony(aOwner);

  telephony->mProvider = ril;
  telephony->mListener = new Listener(telephony);
  telephony->mCallsList = new CallsList(telephony);
  telephony->mGroup = TelephonyCallGroup::Create(telephony);

  nsresult rv = ril->EnumerateCalls(telephony->mListener);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return telephony.forget();
}

// static
bool
Telephony::IsValidNumber(const nsAString& aNumber)
{
  return !aNumber.IsEmpty();
}

// static
uint32_t
Telephony::GetNumServices() {
  return mozilla::Preferences::GetInt("ril.numRadioInterfaces", 1);
}

// static
bool
Telephony::IsValidServiceId(uint32_t aServiceId)
{
  return aServiceId < GetNumServices();
}

// static
bool
Telephony::IsActiveState(uint16_t aCallState) {
  return aCallState == nsITelephonyProvider::CALL_STATE_DIALING ||
      aCallState == nsITelephonyProvider::CALL_STATE_ALERTING ||
      aCallState == nsITelephonyProvider::CALL_STATE_CONNECTED;
}

uint32_t
Telephony::ProvidedOrDefaultServiceId(const Optional<uint32_t>& aServiceId)
{
  if (aServiceId.WasPassed()) {
    return aServiceId.Value();
  } else {
    uint32_t serviceId = 0;
    mProvider->GetDefaultServiceId(&serviceId);
    return serviceId;
  }
}

bool
Telephony::HasDialingCall()
{
  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    const nsRefPtr<TelephonyCall>& call = mCalls[i];
    if (call->CallState() > nsITelephonyProvider::CALL_STATE_UNKNOWN &&
        call->CallState() < nsITelephonyProvider::CALL_STATE_CONNECTED) {
      return true;
    }
  }

  return false;
}

bool
Telephony::MatchActiveCall(TelephonyCall* aCall)
{
  return (mActiveCall &&
          mActiveCall->CallIndex() == aCall->CallIndex() &&
          mActiveCall->ServiceId() == aCall->ServiceId());
}

already_AddRefed<Promise>
Telephony::DialInternal(uint32_t aServiceId, const nsAString& aNumber,
                        bool aIsEmergency)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(global);

  if (!IsValidNumber(aNumber) || !IsValidServiceId(aServiceId)) {
    promise->MaybeReject(NS_LITERAL_STRING("InvalidAccessError"));
    return promise.forget();
  }

  // We only support one outgoing call at a time.
  if (HasDialingCall()) {
    promise->MaybeReject(NS_LITERAL_STRING("InvalidStateError"));
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback =
    new Callback(this, promise, aServiceId, aNumber);
  nsresult rv = mProvider->Dial(aServiceId, aNumber, aIsEmergency, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_LITERAL_STRING("InvalidStateError"));
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<TelephonyCall>
Telephony::CreateNewDialingCall(uint32_t aServiceId, const nsAString& aNumber)
{
  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aServiceId, aNumber,
                          nsITelephonyProvider::CALL_STATE_DIALING);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  return call.forget();
}

nsresult
Telephony::NotifyCallsChanged(TelephonyCall* aCall)
{
  return DispatchCallEvent(NS_LITERAL_STRING("callschanged"), aCall);
}

void
Telephony::UpdateActiveCall(TelephonyCall* aCall, bool aIsActive)
{
  if (aIsActive) {
    mActiveCall = aCall;
  } else if (MatchActiveCall(aCall)) {
    mActiveCall = nullptr;
  }
}

already_AddRefed<TelephonyCall>
Telephony::GetCall(uint32_t aServiceId, uint32_t aCallIndex)
{
  nsRefPtr<TelephonyCall> call;

  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[i];
    if (tempCall->ServiceId() == aServiceId &&
        tempCall->CallIndex() == aCallIndex) {
      call = tempCall;
      break;
    }
  }

  return call.forget();
}

already_AddRefed<TelephonyCall>
Telephony::GetOutgoingCall()
{
  nsRefPtr<TelephonyCall> call;

  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[i];
    if (tempCall->CallIndex() == kOutgoingPlaceholderCallIndex) {
      NS_ASSERTION(!call, "More than one outgoing call not supported!");
      NS_ASSERTION(tempCall->CallState() == nsITelephonyProvider::CALL_STATE_DIALING,
                   "Something really wrong here!");

      call = tempCall;
      // No break. We will search entire list to ensure only one outgoing call.
    }
  }

  return call.forget();
}

already_AddRefed<TelephonyCall>
Telephony::GetCallFromEverywhere(uint32_t aServiceId, uint32_t aCallIndex)
{
  nsRefPtr<TelephonyCall> call = GetCall(aServiceId, aCallIndex);

  if (!call) {
    call = mGroup->GetCall(aServiceId, aCallIndex);
  }

  return call.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Telephony)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Telephony,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGroup)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Telephony,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  tmp->mActiveCall = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGroup)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Telephony)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Telephony, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Telephony, DOMEventTargetHelper)

NS_IMPL_ISUPPORTS1(Telephony::Listener, nsITelephonyListener)
NS_IMPL_ISUPPORTS1(Telephony::Callback, nsITelephonyCallback)

// Telephony WebIDL

already_AddRefed<Promise>
Telephony::Dial(const nsAString& aNumber, const Optional<uint32_t>& aServiceId)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, false);
  return promise.forget();
}

already_AddRefed<Promise>
Telephony::DialEmergency(const nsAString& aNumber,
                         const Optional<uint32_t>& aServiceId)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, true);
  return promise.forget();
}

void
Telephony::StartTone(const nsAString& aDTMFChar,
                     const Optional<uint32_t>& aServiceId,
                     ErrorResult& aRv)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);

  if (aDTMFChar.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    return;
  }

  if (aDTMFChar.Length() > 1 || !IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mProvider->StartTone(serviceId, aDTMFChar);
}

void
Telephony::StopTone(const Optional<uint32_t>& aServiceId, ErrorResult& aRv)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);

  if (!IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mProvider->StopTone(serviceId);
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

void
Telephony::GetActive(Nullable<OwningTelephonyCallOrTelephonyCallGroup>& aValue)
{
  if (mActiveCall) {
    aValue.SetValue().SetAsTelephonyCall() = mActiveCall;
  } else if (mGroup->CallState() == nsITelephonyProvider::CALL_STATE_CONNECTED) {
    aValue.SetValue().SetAsTelephonyCallGroup() = mGroup;
  } else {
    aValue.SetNull();
  }
}

already_AddRefed<CallsList>
Telephony::Calls() const
{
  nsRefPtr<CallsList> list = mCallsList;
  return list.forget();
}

already_AddRefed<TelephonyCallGroup>
Telephony::ConferenceGroup() const
{
  nsRefPtr<TelephonyCallGroup> group = mGroup;
  return group.forget();
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
Telephony::CallStateChanged(uint32_t aServiceId, uint32_t aCallIndex,
                            uint16_t aCallState, const nsAString& aNumber,
                            bool aIsActive, bool aIsOutgoing, bool aIsEmergency,
                            bool aIsConference, bool aIsSwitchable, bool aIsMergeable)
{
  NS_ASSERTION(aCallIndex != kOutgoingPlaceholderCallIndex,
               "This should never happen!");

  nsRefPtr<TelephonyCall> modifiedCall
      = GetCallFromEverywhere(aServiceId, aCallIndex);

  // Try to use the outgoing call if we don't find the modified call.
  if (!modifiedCall) {
    nsRefPtr<TelephonyCall> outgoingCall = GetOutgoingCall();

    // If the call state isn't incoming but we do have an outgoing call then
    // we must be seeing a status update for our outgoing call.
    if (outgoingCall &&
        aCallState != nsITelephonyProvider::CALL_STATE_INCOMING) {
      outgoingCall->UpdateCallIndex(aCallIndex);
      outgoingCall->UpdateEmergency(aIsEmergency);
      modifiedCall.swap(outgoingCall);
    }
  }

  if (modifiedCall) {
    modifiedCall->UpdateSwitchable(aIsSwitchable);
    modifiedCall->UpdateMergeable(aIsMergeable);

    if (!aIsConference) {
      UpdateActiveCall(modifiedCall, aIsActive);
    }

    if (modifiedCall->CallState() != aCallState) {
      // We don't fire the statechange event on a call in conference here.
      // Instead, the event will be fired later in
      // TelephonyCallGroup::ChangeState(). Thus the sequence of firing the
      // statechange events is guaranteed: first on TelephonyCallGroup then on
      // individual TelephonyCall objects.
      bool fireEvent = !aIsConference;
      modifiedCall->ChangeStateInternal(aCallState, fireEvent);
    }

    nsRefPtr<TelephonyCallGroup> group = modifiedCall->GetGroup();

    if (!group && aIsConference) {
      // Add to conference.
      NS_ASSERTION(mCalls.Contains(modifiedCall), "Should in mCalls");
      mGroup->AddCall(modifiedCall);
      RemoveCall(modifiedCall);
    } else if (group && !aIsConference) {
      // Remove from conference.
      NS_ASSERTION(mGroup->CallsArray().Contains(modifiedCall), "Should in mGroup");
      mGroup->RemoveCall(modifiedCall);
      AddCall(modifiedCall);
    }

    return NS_OK;
  }

  // Do nothing since we didn't know anything about it before now and it's
  // ended already.
  if (aCallState == nsITelephonyProvider::CALL_STATE_DISCONNECTED) {
    return NS_OK;
  }

  // Didn't find this call in mCalls or mGroup. Create a new call.
  nsRefPtr<TelephonyCall> call =
      TelephonyCall::Create(this, aServiceId, aNumber, aCallState, aCallIndex,
                            aIsEmergency, aIsConference, aIsSwitchable,
                            aIsMergeable);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(aIsConference ? mGroup->CallsArray().Contains(call) :
                               mCalls.Contains(call),
               "Should have auto-added new call!");

  if (aCallState == nsITelephonyProvider::CALL_STATE_INCOMING) {
    nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("incoming"), call);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
Telephony::ConferenceCallStateChanged(uint16_t aCallState)
{
  mGroup->ChangeState(aCallState);
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

  if (NS_FAILED(mProvider->RegisterListener(mListener))) {
    NS_WARNING("Failed to register listener!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(uint32_t aServiceId, uint32_t aCallIndex,
                              uint16_t aCallState, const nsAString& aNumber,
                              bool aIsActive, bool aIsOutgoing, bool aIsEmergency,
                              bool aIsConference, bool aIsSwitchable, bool aIsMergeable)
{
  nsRefPtr<TelephonyCall> call;

  // We request calls enumeration in constructor, and the asynchronous result
  // will be sent back through the callback function EnumerateCallState().
  // However, it is likely to have call state changes, i.e. CallStateChanged()
  // being called, before the enumeration result comes back. We'd make sure
  // we don't somehow add duplicates due to the race condition.
  call = GetCallFromEverywhere(aServiceId, aCallIndex);
  if (call) {
    return NS_OK;
  }

  // Didn't know anything about this call before now.
  call = TelephonyCall::Create(this, aServiceId, aNumber, aCallState,
                               aCallIndex, aIsEmergency, aIsConference,
                               aIsSwitchable, aIsMergeable);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(aIsConference ? mGroup->CallsArray().Contains(call) :
                               mCalls.Contains(call),
               "Should have auto-added new call!");

  return NS_OK;
}

NS_IMETHODIMP
Telephony::SupplementaryServiceNotification(uint32_t aServiceId,
                                            int32_t aCallIndex,
                                            uint16_t aNotification)
{
  nsRefPtr<TelephonyCall> associatedCall;
  if (!mCalls.IsEmpty()) {
    associatedCall = GetCall(aServiceId, aCallIndex);
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
Telephony::NotifyError(uint32_t aServiceId,
                       int32_t aCallIndex,
                       const nsAString& aError)
{
  if (mCalls.IsEmpty()) {
    NS_ERROR("No existing call!");
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<TelephonyCall> callToNotify = GetCall(aServiceId, aCallIndex);
  if (!callToNotify) {
    NS_ERROR("Don't call me with a bad call index!");
    return NS_ERROR_UNEXPECTED;
  }

  UpdateActiveCall(callToNotify, false);

  // Set the call state to 'disconnected' and remove it from the calls list.
  callToNotify->NotifyError(aError);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyCdmaCallWaiting(uint32_t aServiceId, const nsAString& aNumber)
{
  MOZ_ASSERT(mCalls.Length() == 1);

  nsRefPtr<TelephonyCall> callToNotify = mCalls[0];
  MOZ_ASSERT(callToNotify && callToNotify->ServiceId() == aServiceId);

  callToNotify->UpdateSecondNumber(aNumber);
  DispatchCallEvent(NS_LITERAL_STRING("callschanged"), callToNotify);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyConferenceError(const nsAString& aName,
                                 const nsAString& aMessage)
{
  mGroup->NotifyError(aName, aMessage);
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
