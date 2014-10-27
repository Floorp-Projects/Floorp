/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telephony.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/CallEvent.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TelephonyBinding.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsIPermissionManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "CallsList.h"
#include "TelephonyCall.h"
#include "TelephonyCallGroup.h"
#include "TelephonyCallId.h"
#include "TelephonyDialCallback.h"

// Service instantiation
#include "ipc/TelephonyIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#include "nsIGonkTelephonyService.h"
#endif
#include "nsXULAppAPI.h" // For XRE_GetProcessType()

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;
using mozilla::ErrorResult;

class Telephony::Listener : public nsITelephonyListener
{
  Telephony* mTelephony;

  virtual ~Listener() {}

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSITELEPHONYLISTENER(mTelephony)

  explicit Listener(Telephony* aTelephony)
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
    mTelephony->NotifyEvent(NS_LITERAL_STRING("ready"));
    return NS_OK;
  }
};

Telephony::Telephony(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner), mEnumerated(false)
{
}

Telephony::~Telephony()
{
  Shutdown();
}

void
Telephony::Shutdown()
{
  if (mListener) {
    mListener->Disconnect();

    if (mService) {
      mService->UnregisterListener(mListener);
      mService = nullptr;
    }

    mListener = nullptr;
  }
}

JSObject*
Telephony::WrapObject(JSContext* aCx)
{
  return TelephonyBinding::Wrap(aCx, this);
}

// static
already_AddRefed<Telephony>
Telephony::Create(nsPIDOMWindow* aOwner, ErrorResult& aRv)
{
  NS_ASSERTION(aOwner, "Null owner!");

  nsCOMPtr<nsITelephonyService> ril =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
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

  telephony->mService = ril;
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
  return aCallState == nsITelephonyService::CALL_STATE_DIALING ||
      aCallState == nsITelephonyService::CALL_STATE_ALERTING ||
      aCallState == nsITelephonyService::CALL_STATE_HOLDING ||
      aCallState == nsITelephonyService::CALL_STATE_DISCONNECTING ||
      aCallState == nsITelephonyService::CALL_STATE_CONNECTED;
}

uint32_t
Telephony::ProvidedOrDefaultServiceId(const Optional<uint32_t>& aServiceId)
{
  if (aServiceId.WasPassed()) {
    return aServiceId.Value();
  } else {
    uint32_t serviceId = 0;
    mService->GetDefaultServiceId(&serviceId);
    return serviceId;
  }
}

bool
Telephony::HasDialingCall()
{
  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    const nsRefPtr<TelephonyCall>& call = mCalls[i];
    if (call->CallState() > nsITelephonyService::CALL_STATE_UNKNOWN &&
        call->CallState() < nsITelephonyService::CALL_STATE_CONNECTED) {
      return true;
    }
  }

  return false;
}

already_AddRefed<Promise>
Telephony::DialInternal(uint32_t aServiceId, const nsAString& aNumber,
                        bool aEmergency, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!IsValidNumber(aNumber) || !IsValidServiceId(aServiceId)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  // We only support one outgoing call at a time.
  if (HasDialingCall()) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyDialCallback> callback =
    new TelephonyDialCallback(GetOwner(), this, promise, aServiceId);

  nsresult rv = mService->Dial(aServiceId, aNumber, aEmergency, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<TelephonyCallId>
Telephony::CreateCallId(const nsAString& aNumber, uint16_t aNumberPresentation,
                        const nsAString& aName, uint16_t aNamePresentation)
{
  nsRefPtr<TelephonyCallId> id =
    new TelephonyCallId(GetOwner(), aNumber, aNumberPresentation,
                        aName, aNamePresentation);

  return id.forget();
}

already_AddRefed<TelephonyCall>
Telephony::CreateCall(TelephonyCallId* aId, uint32_t aServiceId,
                      uint32_t aCallIndex, uint16_t aCallState,
                      bool aEmergency, bool aConference,
                      bool aSwitchable, bool aMergeable)
{
  // We don't have to create an already ended call.
  if (aCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
    return nullptr;
  }

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aId, aServiceId, aCallIndex, aCallState,
                          aEmergency, aConference, aSwitchable, aMergeable);

  NS_ASSERTION(call, "This should never fail!");
  NS_ASSERTION(aConference ? mGroup->CallsArray().Contains(call)
                           : mCalls.Contains(call),
               "Should have auto-added new call!");

  return call.forget();
}

nsresult
Telephony::NotifyEvent(const nsAString& aType)
{
  return DispatchCallEvent(aType, nullptr);
}

nsresult
Telephony::NotifyCallsChanged(TelephonyCall* aCall)
{
  return DispatchCallEvent(NS_LITERAL_STRING("callschanged"), aCall);
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGroup)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Telephony)
  // Telephony does not expose nsITelephonyListener.  mListener is the exposed
  // nsITelephonyListener and forwards the calls it receives to us.
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Telephony, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Telephony, DOMEventTargetHelper)

NS_IMPL_ISUPPORTS(Telephony::Listener, nsITelephonyListener)

// Telephony WebIDL

already_AddRefed<Promise>
Telephony::Dial(const nsAString& aNumber, const Optional<uint32_t>& aServiceId,
                ErrorResult& aRv)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, false, aRv);
  return promise.forget();
}

already_AddRefed<Promise>
Telephony::DialEmergency(const nsAString& aNumber,
                         const Optional<uint32_t>& aServiceId,
                         ErrorResult& aRv)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, true, aRv);
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

  aRv = mService->StartTone(serviceId, aDTMFChar);
}

void
Telephony::StopTone(const Optional<uint32_t>& aServiceId, ErrorResult& aRv)
{
  uint32_t serviceId = ProvidedOrDefaultServiceId(aServiceId);

  if (!IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mService->StopTone(serviceId);
}

bool
Telephony::GetMuted(ErrorResult& aRv) const
{
  bool muted = false;
  aRv = mService->GetMicrophoneMuted(&muted);

  return muted;
}

void
Telephony::SetMuted(bool aMuted, ErrorResult& aRv)
{
  aRv = mService->SetMicrophoneMuted(aMuted);
}

bool
Telephony::GetSpeakerEnabled(ErrorResult& aRv) const
{
  bool enabled = false;
  aRv = mService->GetSpeakerEnabled(&enabled);

  return enabled;
}

void
Telephony::SetSpeakerEnabled(bool aEnabled, ErrorResult& aRv)
{
  aRv = mService->SetSpeakerEnabled(aEnabled);
}

void
Telephony::GetActive(Nullable<OwningTelephonyCallOrTelephonyCallGroup>& aValue)
{
  if (mGroup->CallState() == nsITelephonyService::CALL_STATE_CONNECTED) {
    aValue.SetValue().SetAsTelephonyCallGroup() = mGroup;
  } else {
    // Search the first active call.
    for (uint32_t i = 0; i < mCalls.Length(); i++) {
      if (IsActiveState(mCalls[i]->CallState())) {
        aValue.SetValue().SetAsTelephonyCall() = mCalls[i];
        return;
      }
    }
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
  if (aType == nsGkAtoms::onready) {
    EnqueueEnumerationAck();
  }
}

// nsITelephonyListener

NS_IMETHODIMP
Telephony::CallStateChanged(uint32_t aServiceId, uint32_t aCallIndex,
                            uint16_t aCallState, const nsAString& aNumber,
                            uint16_t aNumberPresentation, const nsAString& aName,
                            uint16_t aNamePresentation, bool aIsOutgoing,
                            bool aIsEmergency, bool aIsConference,
                            bool aIsSwitchable, bool aIsMergeable)
{
  nsRefPtr<TelephonyCall> modifiedCall
      = GetCallFromEverywhere(aServiceId, aCallIndex);

  if (modifiedCall) {
    modifiedCall->UpdateEmergency(aIsEmergency);
    modifiedCall->UpdateSwitchable(aIsSwitchable);
    modifiedCall->UpdateMergeable(aIsMergeable);

    if (modifiedCall->CallState() != aCallState) {
      if (aCallState == nsITelephonyService::CALL_STATE_DISCONNECTED) {
        modifiedCall->ChangeStateInternal(aCallState, true);
        return NS_OK;
      }

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

  nsRefPtr<TelephonyCallId> id = CreateCallId(aNumber, aNumberPresentation,
                                              aName, aNamePresentation);
  nsRefPtr<TelephonyCall> call =
    CreateCall(id, aServiceId, aCallIndex, aCallState,
               aIsEmergency, aIsConference, aIsSwitchable, aIsMergeable);

  if (call && aCallState == nsITelephonyService::CALL_STATE_INCOMING) {
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

  // Set conference state.
  if (mGroup->CallsArray().Length() >= 2) {
    const nsTArray<nsRefPtr<TelephonyCall> > &calls = mGroup->CallsArray();

    uint16_t callState = calls[0]->CallState();
    for (uint32_t i = 1; i < calls.Length(); i++) {
      if (calls[i]->CallState() != callState) {
        callState = nsITelephonyService::CALL_STATE_UNKNOWN;
        break;
      }
    }

    mGroup->ChangeState(callState);
  }

  mEnumerated = true;

  if (NS_FAILED(NotifyEvent(NS_LITERAL_STRING("ready")))) {
    NS_WARNING("Failed to notify ready!");
  }

  if (NS_FAILED(mService->RegisterListener(mListener))) {
    NS_WARNING("Failed to register listener!");
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(uint32_t aServiceId, uint32_t aCallIndex,
                              uint16_t aCallState, const nsAString& aNumber,
                              uint16_t aNumberPresentation, const nsAString& aName,
                              uint16_t aNamePresentation, bool aIsOutgoing,
                              bool aIsEmergency, bool aIsConference,
                              bool aIsSwitchable, bool aIsMergeable)
{
  // We request calls enumeration in constructor, and the asynchronous result
  // will be sent back through the callback function EnumerateCallState().
  // However, it is likely to have call state changes, i.e. CallStateChanged()
  // being called, before the enumeration result comes back. We'd make sure
  // we don't somehow add duplicates due to the race condition.
  nsRefPtr<TelephonyCall> call = GetCallFromEverywhere(aServiceId, aCallIndex);
  if (call) {
    return NS_OK;
  }

  // Didn't know anything about this call before now.
  nsRefPtr<TelephonyCallId> id = CreateCallId(aNumber, aNumberPresentation,
                                              aName, aNamePresentation);
  call = CreateCall(id, aServiceId, aCallIndex, aCallState,
                    aIsEmergency, aIsConference, aIsSwitchable, aIsMergeable);

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
    case nsITelephonyService::NOTIFICATION_REMOTE_HELD:
      rv = DispatchCallEvent(NS_LITERAL_STRING("remoteheld"), associatedCall);
      break;
    case nsITelephonyService::NOTIFICATION_REMOTE_RESUMED:
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
  nsRefPtr<TelephonyCall> callToNotify =
    GetCallFromEverywhere(aServiceId, aCallIndex);
  if (!callToNotify) {
    NS_ERROR("Don't call me with a bad call index!");
    return NS_ERROR_UNEXPECTED;
  }

  // Set the call state to 'disconnected' and remove it from the calls list.
  callToNotify->NotifyError(aError);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::NotifyCdmaCallWaiting(uint32_t aServiceId, const nsAString& aNumber,
                                 uint16_t aNumberPresentation,
                                 const nsAString& aName,
                                 uint16_t aNamePresentation)
{
  MOZ_ASSERT(mCalls.Length() == 1);

  nsRefPtr<TelephonyCall> callToNotify = mCalls[0];
  MOZ_ASSERT(callToNotify && callToNotify->ServiceId() == aServiceId);

  nsRefPtr<TelephonyCallId> id =
    new TelephonyCallId(GetOwner(), aNumber, aNumberPresentation, aName,
                        aNamePresentation);
  callToNotify->UpdateSecondId(id);
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
  // If it is an incoming event, the call should not be null.
  MOZ_ASSERT(!aType.EqualsLiteral("incoming") || aCall);

  CallEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mCall = aCall;

  nsRefPtr<CallEvent> event = CallEvent::Constructor(this, aType, init);

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

already_AddRefed<nsITelephonyService>
NS_CreateTelephonyService()
{
  nsCOMPtr<nsITelephonyService> service;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    service = new mozilla::dom::telephony::TelephonyIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    service = do_CreateInstance(GONK_TELEPHONY_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}
