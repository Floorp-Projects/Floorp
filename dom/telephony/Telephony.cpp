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

Telephony::Telephony(nsPIDOMWindow* aOwner)
  : DOMEventTargetHelper(aOwner),
    mAudioAgentNotify(nsIAudioChannelAgent::AUDIO_AGENT_NOTIFY),
    mIsAudioStartPlaying(false),
    mHaveDispatchedInterruptBeginEvent(false),
    mMuted(AudioChannelService::IsAudioChannelMutedByDefault())
{
  MOZ_ASSERT(aOwner);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aOwner);
  MOZ_ASSERT(global);

  ErrorResult rv;
  nsRefPtr<Promise> promise = Promise::Create(global, rv);
  MOZ_ASSERT(!rv.Failed());

  mReadyPromise = promise;
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
Telephony::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TelephonyBinding::Wrap(aCx, this, aGivenProto);
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

uint32_t
Telephony::GetServiceId(const Optional<uint32_t>& aServiceId,
                        bool aGetIfActiveCall)
{
  if (aServiceId.WasPassed()) {
    return aServiceId.Value();
  } else if (aGetIfActiveCall) {
    nsTArray<nsRefPtr<TelephonyCall> > &calls = mCalls;
    if (mGroup->IsActive()) {
      calls = mGroup->CallsArray();
    }
    for (uint32_t i = 0; i < calls.Length(); i++) {
      if (calls[i]->IsActive()) {
        return calls[i]->mServiceId;
      }
    }
  }

  uint32_t serviceId = 0;
  mService->GetDefaultServiceId(&serviceId);
  return serviceId;
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

  nsCOMPtr<nsITelephonyDialCallback> callback =
    new TelephonyDialCallback(GetOwner(), this, promise);

  nsresult rv = mService->Dial(aServiceId, aNumber, aEmergency, callback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  return promise.forget();
}

already_AddRefed<TelephonyCallId>
Telephony::CreateCallId(nsITelephonyCallInfo *aInfo)
{
  nsAutoString number;
  nsAutoString name;
  uint16_t numberPresentation;
  uint16_t namePresentation;

  aInfo->GetNumber(number);
  aInfo->GetName(name);
  aInfo->GetNumberPresentation(&numberPresentation);
  aInfo->GetNamePresentation(&namePresentation);

  return CreateCallId(number, numberPresentation, name, namePresentation);
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
                      uint32_t aCallIndex, TelephonyCallState aState,
                      bool aEmergency, bool aConference,
                      bool aSwitchable, bool aMergeable)
{
  // We don't have to create an already ended call.
  if (aState == TelephonyCallState::Disconnected) {
    return nullptr;
  }

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aId, aServiceId, aCallIndex, aState,
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

nsresult
Telephony::HandleCallInfo(nsITelephonyCallInfo* aInfo)
{
  uint32_t serviceId;
  uint32_t callIndex;
  uint16_t callState;
  bool isEmergency;
  bool isConference;
  bool isSwitchable;
  bool isMergeable;

  aInfo->GetClientId(&serviceId);
  aInfo->GetCallIndex(&callIndex);
  aInfo->GetCallState(&callState);
  aInfo->GetIsEmergency(&isEmergency);
  aInfo->GetIsConference(&isConference);
  aInfo->GetIsSwitchable(&isSwitchable);
  aInfo->GetIsMergeable(&isMergeable);

  TelephonyCallState state = TelephonyCall::ConvertToTelephonyCallState(callState);

  nsRefPtr<TelephonyCall> call = GetCallFromEverywhere(serviceId, callIndex);
  // Handle a newly created call.
  if (!call) {
    nsRefPtr<TelephonyCallId> id = CreateCallId(aInfo);
    call = CreateCall(id, serviceId, callIndex, state, isEmergency,
                      isConference, isSwitchable, isMergeable);
    // The newly created call is an incoming call.
    if (call &&
        state == TelephonyCallState::Incoming) {
      nsresult rv = DispatchCallEvent(NS_LITERAL_STRING("incoming"), call);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  } 

  // Update an existing call
  call->UpdateEmergency(isEmergency);
  call->UpdateSwitchable(isSwitchable);
  call->UpdateMergeable(isMergeable);

  nsAutoString number;
  aInfo->GetNumber(number);
  nsRefPtr<TelephonyCallId> id = call->Id();
  id->UpdateNumber(number);

  nsAutoString disconnectedReason;
  aInfo->GetDisconnectedReason(disconnectedReason);

  // State changed.
  if (call->State() != state) {
    if (state == TelephonyCallState::Disconnected) {
      call->UpdateDisconnectedReason(disconnectedReason);
      call->ChangeState(TelephonyCallState::Disconnected);
      return NS_OK;
    }

    // We don't fire the statechange event on a call in conference here.
    // Instead, the event will be fired later in
    // TelephonyCallGroup::ChangeState(). Thus the sequence of firing the
    // statechange events is guaranteed: first on TelephonyCallGroup then on
    // individual TelephonyCall objects.
    bool fireEvent = !isConference;
    call->ChangeStateInternal(state, fireEvent);
  }

  // Group changed.
  nsRefPtr<TelephonyCallGroup> group = call->GetGroup();

  if (!group && isConference) {
    // Add to conference.
    NS_ASSERTION(mCalls.Contains(call), "Should in mCalls");
    mGroup->AddCall(call);
    RemoveCall(call);
  } else if (group && !isConference) {
    // Remove from conference.
    NS_ASSERTION(mGroup->CallsArray().Contains(call), "Should in mGroup");
    mGroup->RemoveCall(call);
    AddCall(call);
  }

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Telephony)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Telephony,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCalls)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGroup)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReadyPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Telephony,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCalls)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallsList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGroup)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReadyPromise)
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
  uint32_t serviceId = GetServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, false, aRv);
  return promise.forget();
}

already_AddRefed<Promise>
Telephony::DialEmergency(const nsAString& aNumber,
                         const Optional<uint32_t>& aServiceId,
                         ErrorResult& aRv)
{
  uint32_t serviceId = GetServiceId(aServiceId);
  nsRefPtr<Promise> promise = DialInternal(serviceId, aNumber, true, aRv);
  return promise.forget();
}

already_AddRefed<Promise>
Telephony::SendTones(const nsAString& aDTMFChars,
                     uint32_t aPauseDuration,
                     uint32_t aToneDuration,
                     const Optional<uint32_t>& aServiceId,
                     ErrorResult& aRv)
{
  uint32_t serviceId = GetServiceId(aServiceId,
                                    true /* aGetIfActiveCall */);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aDTMFChars.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  if (!IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsITelephonyCallback> callback =
    new TelephonyCallback(promise);

  aRv = mService->SendTones(serviceId, aDTMFChars, aPauseDuration,
                            aToneDuration, callback);
  return promise.forget();
}

void
Telephony::StartTone(const nsAString& aDTMFChar,
                     const Optional<uint32_t>& aServiceId,
                     ErrorResult& aRv)
{
  uint32_t serviceId = GetServiceId(aServiceId,
                                    true /* aGetIfActiveCall */);

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
  uint32_t serviceId = GetServiceId(aServiceId,
                                    true /* aGetIfActiveCall */);

  if (!IsValidServiceId(serviceId)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  aRv = mService->StopTone(serviceId);
}

void
Telephony::OwnAudioChannel(ErrorResult& aRv)
{
  if (mAudioAgent) {
    return;
  }

  mAudioAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
  MOZ_ASSERT(mAudioAgent);
  aRv = mAudioAgent->Init(GetParentObject(),
                         (int32_t)AudioChannel::Telephony, this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  aRv = HandleAudioAgentState();
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

nsresult
Telephony::HandleAudioAgentState()
{
  if (!mAudioAgent) {
    return NS_OK;
  }

  Nullable<OwningTelephonyCallOrTelephonyCallGroup> activeCall;
  GetActive(activeCall);
  nsresult rv;
  // Only stop the agent when there's no call.
  if ((!mCalls.Length() && !mGroup->CallsArray().Length()) &&
       mIsAudioStartPlaying) {
    mIsAudioStartPlaying = false;
    rv = mAudioAgent->NotifyStoppedPlaying(mAudioAgentNotify);
    mAudioAgent = nullptr;
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (!activeCall.IsNull() && !mIsAudioStartPlaying) {
    mIsAudioStartPlaying = true;
    float volume;
    bool muted;
    rv = mAudioAgent->NotifyStartedPlaying(mAudioAgentNotify, &volume, &muted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // In B2G, the system app manages audio playback policy. If there is a new
    // sound want to be playback, it must wait for the permission from the
    // system app. It means that the sound would be muted first, and then be
    // unmuted. For telephony, the behaviors are hold() first, then resume().
    // However, the telephony service can't handle all these requests within a
    // short period. The telephony service would reject our resume request,
    // because the modem have not changed the call state yet. It causes that
    // the telephony can't be resumed. Therefore, we don't mute the telephony
    // at the beginning.
    volume = 1.0;
    muted = false;
    rv = WindowVolumeChanged(volume, muted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
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
  if (mGroup->IsActive()) {
    aValue.SetValue().SetAsTelephonyCallGroup() = mGroup;
    return;
  }

  // Search for the active call.
  for (uint32_t i = 0; i < mCalls.Length(); i++) {
    if (mCalls[i]->IsActive()) {
      aValue.SetValue().SetAsTelephonyCall() = mCalls[i];
      return;
    }
  }

  // Nothing active found.
  aValue.SetNull();
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

already_AddRefed<Promise>
Telephony::GetReady(ErrorResult& aRv) const
{
  if (!mReadyPromise) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = mReadyPromise;
  return promise.forget();
}

// nsIAudioChannelAgentCallback

NS_IMETHODIMP
Telephony::WindowVolumeChanged(float aVolume, bool aMuted)
{
  // It's impossible to put all the calls on-hold in the multi-call case.
  if (mCalls.Length() > 1 ||
     (mCalls.Length() == 1 && mGroup->CallsArray().Length())) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  nsRefPtr<Promise> promise = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  bool isSingleCall = mCalls.Length();
  nsCOMPtr<nsITelephonyCallback> callback = new TelephonyCallback(promise);
  if (isSingleCall) {
    rv = aMuted ? mCalls[0]->Hold(callback) : mCalls[0]->Resume(callback);
  } else {
    rv = aMuted ? mGroup->Hold(callback) : mGroup->Resume(callback);
  }
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // These events will be triggered when the telephony is interrupted by other
  // audio channel.
  if (mMuted != aMuted) {
    mMuted = aMuted;
    // We should not dispatch "mozinterruptend" when the system app initializes
    // the telephony audio from muted to unmuted at the first time. The event
    // "mozinterruptend" must be dispatched after the "mozinterruptbegin".
    if (!mHaveDispatchedInterruptBeginEvent && mMuted) {
      DispatchTrustedEvent(NS_LITERAL_STRING("mozinterruptbegin"));
      mHaveDispatchedInterruptBeginEvent = mMuted;
    } else if (mHaveDispatchedInterruptBeginEvent && !mMuted) {
      DispatchTrustedEvent(NS_LITERAL_STRING("mozinterruptend"));
      mHaveDispatchedInterruptBeginEvent = mMuted;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Telephony::WindowAudioCaptureChanged()
{
  // Do nothing, it's useless for the telephony object.
  return NS_OK;
}

// nsITelephonyListener

NS_IMETHODIMP
Telephony::CallStateChanged(uint32_t aLength, nsITelephonyCallInfo** aAllInfo)
{
  nsresult rv;
  for (uint32_t i = 0; i < aLength; ++i) {
    rv = HandleCallInfo(aAllInfo[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = HandleAudioAgentState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(nsITelephonyCallInfo* aInfo)
{
  return HandleCallInfo(aInfo);
}

NS_IMETHODIMP
Telephony::ConferenceCallStateChanged(uint16_t aCallState)
{
  // The current design of Telephony Stack gaurantees that the calls within a
  // call group are updated before this method being called, so we can let a
  // call update its state by its own, and we can discard |aCallState| here.
  // Anyway, this method is going to be deprecated in Bug 1155072.
  mGroup->ChangeState();
  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallStateComplete()
{
  // Set conference state.
  mGroup->ChangeState();

  HandleAudioAgentState();
  if (mReadyPromise) {
    mReadyPromise->MaybeResolve(JS::UndefinedHandleValue);
  }

  if (NS_FAILED(mService->RegisterListener(mListener))) {
    NS_WARNING("Failed to register listener!");
  }
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

already_AddRefed<nsITelephonyService>
NS_CreateTelephonyService()
{
  nsCOMPtr<nsITelephonyService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::telephony::TelephonyIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    service = do_CreateInstance(GONK_TELEPHONY_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}
