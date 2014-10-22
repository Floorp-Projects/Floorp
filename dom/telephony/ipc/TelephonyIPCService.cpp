/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/telephony/TelephonyChild.h"
#include "mozilla/Preferences.h"

USING_TELEPHONY_NAMESPACE
using namespace mozilla::dom;

namespace {

const char* kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
#define kPrefDefaultServiceId "dom.telephony.defaultServiceId"
const char* kObservedPrefs[] = {
  kPrefDefaultServiceId,
  nullptr
};

uint32_t
getDefaultServiceId()
{
  int32_t id = mozilla::Preferences::GetInt(kPrefDefaultServiceId, 0);
  int32_t numRil = mozilla::Preferences::GetInt(kPrefRilNumRadioInterfaces, 1);

  if (id >= numRil || id < 0) {
    id = 0;
  }

  return id;
}

} // Anonymous namespace

NS_IMPL_ISUPPORTS(TelephonyIPCService,
                  nsITelephonyService,
                  nsITelephonyListener,
                  nsIObserver)

TelephonyIPCService::TelephonyIPCService()
{
  // Deallocated in ContentChild::DeallocPTelephonyChild().
  mPTelephonyChild = new TelephonyChild(this);
  ContentChild::GetSingleton()->SendPTelephonyConstructor(mPTelephonyChild);

  Preferences::AddStrongObservers(this, kObservedPrefs);
  mDefaultServiceId = getDefaultServiceId();
}

TelephonyIPCService::~TelephonyIPCService()
{
  if (mPTelephonyChild) {
    mPTelephonyChild->Send__delete__(mPTelephonyChild);
    mPTelephonyChild = nullptr;
  }
}

void
TelephonyIPCService::NoteActorDestroyed()
{
  MOZ_ASSERT(mPTelephonyChild);

  mPTelephonyChild = nullptr;
}

/*
 * Implementation of nsIObserver.
 */

NS_IMETHODIMP
TelephonyIPCService::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral(kPrefDefaultServiceId)) {
      mDefaultServiceId = getDefaultServiceId();
    }
    return NS_OK;
  }

  MOZ_ASSERT(false, "TelephonyIPCService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/*
 * Implementation of nsITelephonyService.
 */

NS_IMETHODIMP
TelephonyIPCService::GetDefaultServiceId(uint32_t* aServiceId)
{
  *aServiceId = mDefaultServiceId;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::RegisterListener(nsITelephonyListener *aListener)
{
  MOZ_ASSERT(!mListeners.Contains(aListener));

  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // nsTArray doesn't fail.
  mListeners.AppendElement(aListener);

  if (mListeners.Length() == 1) {
    mPTelephonyChild->SendRegisterListener();
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::UnregisterListener(nsITelephonyListener *aListener)
{
  MOZ_ASSERT(mListeners.Contains(aListener));

  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // We always have the element here, so it can't fail.
  mListeners.RemoveElement(aListener);

  if (!mListeners.Length()) {
    mPTelephonyChild->SendUnregisterListener();
  }
  return NS_OK;
}

nsresult
TelephonyIPCService::SendRequest(nsITelephonyListener *aListener,
                                  nsITelephonyCallback *aCallback,
                                  const IPCTelephonyRequest& aRequest)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  // Life time of newly allocated TelephonyRequestChild instance is managed by
  // IPDL itself.
  TelephonyRequestChild* actor = new TelephonyRequestChild(aListener, aCallback);
  mPTelephonyChild->SendPTelephonyRequestConstructor(actor, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCalls(nsITelephonyListener *aListener)
{
  return SendRequest(aListener, nullptr, EnumerateCallsRequest());
}

NS_IMETHODIMP
TelephonyIPCService::Dial(uint32_t aClientId, const nsAString& aNumber,
                           bool aIsEmergency,
                           nsITelephonyDialCallback *aCallback)
{
  return SendRequest(nullptr, aCallback,
                     DialRequest(aClientId, nsString(aNumber), aIsEmergency));
}

NS_IMETHODIMP
TelephonyIPCService::HangUp(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendHangUpCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::AnswerCall(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendAnswerCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::RejectCall(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendRejectCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::HoldCall(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendHoldCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::ResumeCall(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendResumeCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::ConferenceCall(uint32_t aClientId)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendConferenceCall(aClientId);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SeparateCall(uint32_t aClientId, uint32_t aCallIndex)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSeparateCall(aClientId, aCallIndex);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::HangUpConference(uint32_t aClientId,
                                      nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, HangUpConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::HoldConference(uint32_t aClientId)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendHoldConference(aClientId);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::ResumeConference(uint32_t aClientId)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendResumeConference(aClientId);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::StartTone(uint32_t aClientId, const nsAString& aDtmfChar)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendStartTone(aClientId, nsString(aDtmfChar));
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::StopTone(uint32_t aClientId)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendStopTone(aClientId);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SendUSSD(uint32_t aClientId, const nsAString& aUssd,
                              nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback,
                     USSDRequest(aClientId, nsString(aUssd)));
}

NS_IMETHODIMP
TelephonyIPCService::GetMicrophoneMuted(bool* aMuted)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetMicrophoneMuted(aMuted);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetMicrophoneMuted(bool aMuted)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetMicrophoneMuted(aMuted);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::GetSpeakerEnabled(bool* aEnabled)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendGetSpeakerEnabled(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SetSpeakerEnabled(bool aEnabled)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  mPTelephonyChild->SendSetSpeakerEnabled(aEnabled);
  return NS_OK;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyIPCService::CallStateChanged(uint32_t aClientId,
                                       uint32_t aCallIndex,
                                       uint16_t aCallState,
                                       const nsAString& aNumber,
                                       uint16_t aNumberPresentation,
                                       const nsAString& aName,
                                       uint16_t aNamePresentation,
                                       bool aIsOutgoing,
                                       bool aIsEmergency,
                                       bool aIsConference,
                                       bool aIsSwitchable,
                                       bool aIsMergeable)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->CallStateChanged(aClientId, aCallIndex, aCallState, aNumber,
                                    aNumberPresentation, aName, aNamePresentation,
                                    aIsOutgoing, aIsEmergency, aIsConference,
                                    aIsSwitchable, aIsMergeable);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::ConferenceCallStateChanged(uint16_t aCallState)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->ConferenceCallStateChanged(aCallState);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCallStateComplete()
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyIPCService::EnumerateCallState(uint32_t aClientId,
                                         uint32_t aCallIndex,
                                         uint16_t aCallState,
                                         const nsAString& aNumber,
                                         uint16_t aNumberPresentation,
                                         const nsAString& aName,
                                         uint16_t aNamePresentation,
                                         bool aIsOutgoing,
                                         bool aIsEmergency,
                                         bool aIsConference,
                                         bool aIsSwitchable,
                                         bool aIsMergeable)
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyIPCService::NotifyCdmaCallWaiting(uint32_t aClientId,
                                            const nsAString& aNumber,
                                            uint16_t aNumberPresentation,
                                            const nsAString& aName,
                                            uint16_t aNamePresentation)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyCdmaCallWaiting(aClientId, aNumber, aNumberPresentation,
                                         aName, aNamePresentation);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyConferenceError(const nsAString& aName,
                                            const nsAString& aMessage)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyConferenceError(aName, aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::NotifyError(uint32_t aClientId, int32_t aCallIndex,
                                  const nsAString& aError)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->NotifyError(aClientId, aCallIndex, aError);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyIPCService::SupplementaryServiceNotification(uint32_t aClientId,
                                                       int32_t aCallIndex,
                                                       uint16_t aNotification)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->SupplementaryServiceNotification(aClientId, aCallIndex,
                                                    aNotification);
  }
  return NS_OK;
}
