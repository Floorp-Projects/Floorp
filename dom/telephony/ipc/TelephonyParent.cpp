/* -*- Mode: C++ tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyParent.h"
#include "nsServiceManagerUtils.h"

USING_TELEPHONY_NAMESPACE

/*******************************************************************************
 * TelephonyParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(TelephonyParent, nsITelephonyListener)

TelephonyParent::TelephonyParent()
  : mActorDestroyed(false)
  , mRegistered(false)
{
}

void
TelephonyParent::ActorDestroy(ActorDestroyReason why)
{
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mActorDestroyed is set to true. Return
  // an error here to avoid sending a message to the dead process.
  mActorDestroyed = true;

  // Try to unregister listener if we're still registered.
  RecvUnregisterListener();
}

bool
TelephonyParent::RecvPTelephonyRequestConstructor(PTelephonyRequestParent* aActor)
{
  TelephonyRequestParent* actor = static_cast<TelephonyRequestParent*>(aActor);

  return actor->DoRequest();
}

PTelephonyRequestParent*
TelephonyParent::AllocPTelephonyRequestParent()
{
  TelephonyRequestParent* actor = new TelephonyRequestParent();
  // Add an extra ref for IPDL. Will be released in
  // TelephonyParent::DeallocPTelephonyRequestParent().
  NS_ADDREF(actor);

  return actor;
}

bool
TelephonyParent::DeallocPTelephonyRequestParent(PTelephonyRequestParent* aActor)
{
  // TelephonyRequestParent is refcounted, must not be freed manually.
  static_cast<TelephonyRequestParent*>(aActor)->Release();
  return true;
}

bool
TelephonyParent::Recv__delete__()
{
  return true; // Unregister listener in TelephonyParent::ActorDestroy().
}

bool
TelephonyParent::RecvRegisterListener()
{
  NS_ENSURE_TRUE(!mRegistered, true);

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  mRegistered = NS_SUCCEEDED(provider->RegisterListener(this));
  return true;
}

bool
TelephonyParent::RecvUnregisterListener()
{
  NS_ENSURE_TRUE(mRegistered, true);

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  mRegistered = !NS_SUCCEEDED(provider->UnregisterListener(this));
  return true;
}

bool
TelephonyParent::RecvDialCall(const nsString& aNumber,
                              const bool& aIsEmergency)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->Dial(aNumber, aIsEmergency);
  return true;
}

bool
TelephonyParent::RecvHangUpCall(const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->HangUp(aCallIndex);
  return true;
}

bool
TelephonyParent::RecvAnswerCall(const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->AnswerCall(aCallIndex);
  return true;
}

bool
TelephonyParent::RecvRejectCall(const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->RejectCall(aCallIndex);
  return true;
}

bool
TelephonyParent::RecvHoldCall(const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->HoldCall(aCallIndex);
  return true;
}

bool
TelephonyParent::RecvResumeCall(const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->ResumeCall(aCallIndex);
  return true;
}

bool
TelephonyParent::RecvConferenceCall()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->ConferenceCall();
  return true;
}

bool
TelephonyParent::RecvSeparateCall(const uint32_t& aCallState)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->SeparateCall(aCallState);
  return true;
}

bool
TelephonyParent::RecvHoldConference()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->HoldConference();
  return true;
}

bool
TelephonyParent::RecvResumeConference()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->ResumeConference();
  return true;
}

bool
TelephonyParent::RecvStartTone(const nsString& aTone)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->StartTone(aTone);
  return true;
}

bool
TelephonyParent::RecvStopTone()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->StopTone();
  return true;
}

bool
TelephonyParent::RecvGetMicrophoneMuted(bool* aMuted)
{
  *aMuted = false;

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->GetMicrophoneMuted(aMuted);
  return true;
}

bool
TelephonyParent::RecvSetMicrophoneMuted(const bool& aMuted)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->SetMicrophoneMuted(aMuted);
  return true;
}

bool
TelephonyParent::RecvGetSpeakerEnabled(bool* aEnabled)
{
  *aEnabled = false;

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->GetSpeakerEnabled(aEnabled);
  return true;
}

bool
TelephonyParent::RecvSetSpeakerEnabled(const bool& aEnabled)
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, true);

  provider->SetSpeakerEnabled(aEnabled);
  return true;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyParent::CallStateChanged(uint32_t aCallIndex,
                                  uint16_t aCallState,
                                  const nsAString& aNumber,
                                  bool aIsActive,
                                  bool aIsOutgoing,
                                  bool aIsEmergency,
                                  bool aIsConference)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCallStateData data(aCallIndex, aCallState, nsString(aNumber), aIsActive,
                        aIsOutgoing, aIsEmergency, aIsConference);
  return SendNotifyCallStateChanged(data) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::ConferenceCallStateChanged(uint16_t aCallState)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyConferenceCallStateChanged(aCallState) ? NS_OK
                                                          : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallStateComplete()
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallState(uint32_t aCallIndex,
                                    uint16_t aCallState,
                                    const nsAString& aNumber,
                                    bool aIsActive,
                                    bool aIsOutgoing,
                                    bool aIsEmergency,
                                    bool aIsConference)
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::NotifyCdmaCallWaiting(const nsAString& aNumber)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyCdmaCallWaiting(nsString(aNumber)) ? NS_OK
                                                      : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyError(int32_t aCallIndex,
                             const nsAString& aError)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyCallError(aCallIndex, nsString(aError)) ? NS_OK
                                                           : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::SupplementaryServiceNotification(int32_t aCallIndex,
                                                  uint16_t aNotification)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifySupplementaryService(aCallIndex, aNotification)
    ? NS_OK : NS_ERROR_FAILURE;
}

/*******************************************************************************
 * TelephonyRequestParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(TelephonyRequestParent, nsITelephonyListener)

TelephonyRequestParent::TelephonyRequestParent()
  : mActorDestroyed(false)
{
}

void
TelephonyRequestParent::ActorDestroy(ActorDestroyReason why)
{
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mActorDestroyed is set to true. Return
  // an error here to avoid sending a message to the dead process.
  mActorDestroyed = true;
}

bool
TelephonyRequestParent::DoRequest()
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  if (provider) {
    rv = provider->EnumerateCalls(this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(EnumerateCallStateComplete());
  }

  return true;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyRequestParent::CallStateChanged(uint32_t aCallIndex,
                                         uint16_t aCallState,
                                         const nsAString& aNumber,
                                         bool aIsActive,
                                         bool aIsOutgoing,
                                         bool aIsEmergency,
                                         bool aIsConference)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::ConferenceCallStateChanged(uint16_t aCallState)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallStateComplete()
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallState(uint32_t aCallIndex,
                                           uint16_t aCallState,
                                           const nsAString& aNumber,
                                           bool aIsActive,
                                           bool aIsOutgoing,
                                           bool aIsEmergency,
                                           bool aIsConference)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCallStateData data(aCallIndex, aCallState, nsString(aNumber), aIsActive,
                        aIsOutgoing, aIsEmergency, aIsConference);
  return SendNotifyEnumerateCallState(data) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyCdmaCallWaiting(const nsAString& aNumber)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyError(int32_t aCallIndex,
                                    const nsAString& aError)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::SupplementaryServiceNotification(int32_t aCallIndex,
                                                         uint16_t aNotification)
{
  MOZ_CRASH("Not a TelephonyParent!");
}
