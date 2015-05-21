/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/telephony/TelephonyChild.h"
#include "mozilla/Preferences.h"

#include "nsITelephonyCallInfo.h"

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
  nsCOMPtr<nsITelephonyCallback> callback = do_QueryInterface(aCallback);
  return SendRequest(nullptr, callback,
                     DialRequest(aClientId, nsString(aNumber), aIsEmergency));
}

NS_IMETHODIMP
TelephonyIPCService::AnswerCall(uint32_t aClientId, uint32_t aCallIndex,
                                nsITelephonyCallback *aCallback)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, AnswerCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::HangUpCall(uint32_t aClientId, uint32_t aCallIndex,
                                nsITelephonyCallback *aCallback)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, HangUpCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::RejectCall(uint32_t aClientId, uint32_t aCallIndex,
                                nsITelephonyCallback *aCallback)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, RejectCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::HoldCall(uint32_t aClientId, uint32_t aCallIndex,
                              nsITelephonyCallback *aCallback)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, HoldCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::ResumeCall(uint32_t aClientId, uint32_t aCallIndex,
                                nsITelephonyCallback *aCallback)
{
  if (!mPTelephonyChild) {
    NS_WARNING("TelephonyService used after shutdown has begun!");
    return NS_ERROR_FAILURE;
  }

  return SendRequest(nullptr, aCallback, ResumeCallRequest(aClientId, aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::ConferenceCall(uint32_t aClientId,
                                    nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, ConferenceCallRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::SeparateCall(uint32_t aClientId, uint32_t aCallIndex,
                                  nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, SeparateCallRequest(aClientId,
                                                             aCallIndex));
}

NS_IMETHODIMP
TelephonyIPCService::HangUpConference(uint32_t aClientId,
                                      nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, HangUpConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::HoldConference(uint32_t aClientId,
                                    nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, HoldConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::ResumeConference(uint32_t aClientId,
                                      nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, ResumeConferenceRequest(aClientId));
}

NS_IMETHODIMP
TelephonyIPCService::SendTones(uint32_t aClientId, const nsAString& aDtmfChars,
                               uint32_t aPauseDuration, uint32_t aToneDuration,
                               nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, SendTonesRequest(aClientId,
                     nsString(aDtmfChars), aPauseDuration, aToneDuration));
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
                     SendUSSDRequest(aClientId, nsString(aUssd)));
}

NS_IMETHODIMP
TelephonyIPCService::CancelUSSD(uint32_t aClientId,
                                nsITelephonyCallback *aCallback)
{
  return SendRequest(nullptr, aCallback, CancelUSSDRequest(aClientId));
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
TelephonyIPCService::CallStateChanged(uint32_t aLength, nsITelephonyCallInfo** aAllInfo)
{
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->CallStateChanged(aLength, aAllInfo);
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
TelephonyIPCService::EnumerateCallState(nsITelephonyCallInfo* aInfo)
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
