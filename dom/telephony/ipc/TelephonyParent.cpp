/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyParent.h"
#include "nsServiceManagerUtils.h"

USING_TELEPHONY_NAMESPACE

/*******************************************************************************
 * TelephonyParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyParent, nsITelephonyListener)

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
TelephonyParent::RecvPTelephonyRequestConstructor(PTelephonyRequestParent* aActor,
                                                  const IPCTelephonyRequest& aRequest)
{
  TelephonyRequestParent* actor = static_cast<TelephonyRequestParent*>(aActor);
  nsCOMPtr<nsITelephonyService> service = do_GetService(TELEPHONY_SERVICE_CONTRACTID);

  if (!service) {
    return NS_SUCCEEDED(actor->GetCallback()->NotifyError(NS_LITERAL_STRING("InvalidStateError")));
  }

  switch (aRequest.type()) {
    case IPCTelephonyRequest::TEnumerateCallsRequest: {
      nsresult rv = service->EnumerateCalls(actor);
      if (NS_FAILED(rv)) {
        return NS_SUCCEEDED(EnumerateCallStateComplete());
      } else {
        return true;
      }
    }

    case IPCTelephonyRequest::TDialRequest: {
      const DialRequest& request = aRequest.get_DialRequest();
      service->Dial(request.clientId(), request.number(),
                    request.isEmergency(), actor->GetDialCallback());
      return true;
    }

    case IPCTelephonyRequest::TSendUSSDRequest: {
      const SendUSSDRequest& request = aRequest.get_SendUSSDRequest();
      service->SendUSSD(request.clientId(), request.ussd(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TCancelUSSDRequest: {
      const CancelUSSDRequest& request = aRequest.get_CancelUSSDRequest();
      service->CancelUSSD(request.clientId(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TConferenceCallRequest: {
      const ConferenceCallRequest& request = aRequest.get_ConferenceCallRequest();
      service->ConferenceCall(request.clientId(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TSeparateCallRequest: {
      const SeparateCallRequest& request = aRequest.get_SeparateCallRequest();
      service->SeparateCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::THangUpConferenceRequest: {
      const HangUpConferenceRequest& request = aRequest.get_HangUpConferenceRequest();
      service->HangUpConference(request.clientId(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::THoldConferenceRequest: {
      const HoldConferenceRequest& request = aRequest.get_HoldConferenceRequest();
      service->HoldConference(request.clientId(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TResumeConferenceRequest: {
      const ResumeConferenceRequest& request = aRequest.get_ResumeConferenceRequest();
      service->ResumeConference(request.clientId(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TAnswerCallRequest: {
      const AnswerCallRequest& request = aRequest.get_AnswerCallRequest();
      service->AnswerCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::THangUpCallRequest: {
      const HangUpCallRequest& request = aRequest.get_HangUpCallRequest();
      service->HangUpCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TRejectCallRequest: {
      const RejectCallRequest& request = aRequest.get_RejectCallRequest();
      service->RejectCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::THoldCallRequest: {
      const HoldCallRequest& request = aRequest.get_HoldCallRequest();
      service->HoldCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TResumeCallRequest: {
      const ResumeCallRequest& request = aRequest.get_ResumeCallRequest();
      service->ResumeCall(request.clientId(), request.callIndex(), actor->GetCallback());
      return true;
    }

    case IPCTelephonyRequest::TSendTonesRequest: {
      const SendTonesRequest& request = aRequest.get_SendTonesRequest();
      service->SendTones(request.clientId(),
                         request.dtmfChars(),
                         request.pauseDuration(),
                         request.toneDuration(),
                         actor->GetCallback());
      return true;
    }

    default:
      MOZ_CRASH("Unknown type!");
  }

  return false;
}

PTelephonyRequestParent*
TelephonyParent::AllocPTelephonyRequestParent(const IPCTelephonyRequest& aRequest)
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

  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  mRegistered = NS_SUCCEEDED(service->RegisterListener(this));
  return true;
}

bool
TelephonyParent::RecvUnregisterListener()
{
  NS_ENSURE_TRUE(mRegistered, true);

  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  mRegistered = !NS_SUCCEEDED(service->UnregisterListener(this));
  return true;
}

bool
TelephonyParent::RecvStartTone(const uint32_t& aClientId, const nsString& aTone)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->StartTone(aClientId, aTone);
  return true;
}

bool
TelephonyParent::RecvStopTone(const uint32_t& aClientId)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->StopTone(aClientId);
  return true;
}

bool
TelephonyParent::RecvGetMicrophoneMuted(bool* aMuted)
{
  *aMuted = false;

  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->GetMicrophoneMuted(aMuted);
  return true;
}

bool
TelephonyParent::RecvSetMicrophoneMuted(const bool& aMuted)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->SetMicrophoneMuted(aMuted);
  return true;
}

bool
TelephonyParent::RecvGetSpeakerEnabled(bool* aEnabled)
{
  *aEnabled = false;

  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->GetSpeakerEnabled(aEnabled);
  return true;
}

bool
TelephonyParent::RecvSetSpeakerEnabled(const bool& aEnabled)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->SetSpeakerEnabled(aEnabled);
  return true;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyParent::CallStateChanged(uint32_t aLength, nsITelephonyCallInfo** aAllInfo)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  nsTArray<nsITelephonyCallInfo*> allInfo;
  for (uint32_t i = 0; i < aLength; i++) {
    allInfo.AppendElement(aAllInfo[i]);
  }

  return SendNotifyCallStateChanged(allInfo) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallStateComplete()
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::EnumerateCallState(nsITelephonyCallInfo* aInfo)
{
  MOZ_CRASH("Not a EnumerateCalls request!");
}

NS_IMETHODIMP
TelephonyParent::NotifyCdmaCallWaiting(uint32_t aClientId,
                                       const nsAString& aNumber,
                                       uint16_t aNumberPresentation,
                                       const nsAString& aName,
                                       uint16_t aNamePresentation)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCdmaWaitingCallData data(nsString(aNumber), aNumberPresentation,
                              nsString(aName), aNamePresentation);
  return SendNotifyCdmaCallWaiting(aClientId, data) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::NotifyConferenceError(const nsAString& aName,
                                       const nsAString& aMessage)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyConferenceError(nsString(aName), nsString(aMessage)) ? NS_OK
                                                                        : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyParent::SupplementaryServiceNotification(uint32_t aClientId,
                                                  int32_t aCallIndex,
                                                  uint16_t aNotification)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifySupplementaryService(aClientId, aCallIndex, aNotification)
      ? NS_OK : NS_ERROR_FAILURE;
}

/*******************************************************************************
 * TelephonyRequestParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyRequestParent,
                  nsITelephonyListener)

TelephonyRequestParent::TelephonyRequestParent()
  : mActorDestroyed(false),
    mCallback(new Callback(*this)),
    mDialCallback(new DialCallback(*this))
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

nsresult
TelephonyRequestParent::SendResponse(const IPCTelephonyResponse& aResponse)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, aResponse) ? NS_OK : NS_ERROR_FAILURE;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyRequestParent::CallStateChanged(uint32_t aLength, nsITelephonyCallInfo** aAllInfo)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallStateComplete()
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, EnumerateCallsResponse()) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallState(nsITelephonyCallInfo* aInfo)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyEnumerateCallState(aInfo) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyCdmaCallWaiting(uint32_t aClientId,
                                              const nsAString& aNumber,
                                              uint16_t aNumberPresentation,
                                              const nsAString& aName,
                                              uint16_t aNamePresentation)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyConferenceError(const nsAString& aName,
                                              const nsAString& aMessage)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

NS_IMETHODIMP
TelephonyRequestParent::SupplementaryServiceNotification(uint32_t aClientId,
                                                         int32_t aCallIndex,
                                                         uint16_t aNotification)
{
  MOZ_CRASH("Not a TelephonyParent!");
}

/*******************************************************************************
 * TelephonyRequestParent::Callback
 ******************************************************************************/

NS_IMPL_ISUPPORTS(TelephonyRequestParent::Callback,
                  nsITelephonyCallback)

nsresult TelephonyRequestParent::Callback::SendResponse(const IPCTelephonyResponse& aResponse)
{
  return mParent.SendResponse(aResponse);
}

NS_IMETHODIMP
TelephonyRequestParent::Callback::NotifySuccess()
{
  return SendResponse(SuccessResponse());
}

NS_IMETHODIMP
TelephonyRequestParent::Callback::NotifyError(const nsAString& aError)
{
  return SendResponse(ErrorResponse(nsAutoString(aError)));
}

/*******************************************************************************
 * TelephonyRequestParent::DialCallback
 ******************************************************************************/

NS_IMPL_ISUPPORTS_INHERITED(TelephonyRequestParent::DialCallback,
                            TelephonyRequestParent::Callback,
                            nsITelephonyDialCallback)

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMI(const nsAString& aServiceCode)
{
  NS_ENSURE_TRUE(!mParent.mActorDestroyed, NS_ERROR_FAILURE);

  return mParent.SendNotifyDialMMI(nsAutoString(aServiceCode)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialCallSuccess(uint32_t aClientId,
                                                            uint32_t aCallIndex,
                                                            const nsAString& aNumber)
{
  return SendResponse(DialResponseCallSuccess(aClientId, aCallIndex,
                                              nsAutoString(aNumber)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccess(const nsAString& aStatusMessage)
{
  return SendResponse(DialResponseMMISuccess(nsAutoString(aStatusMessage),
                                             AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccessWithInteger(const nsAString& aStatusMessage,
                                                                      uint16_t aAdditionalInformation)
{
  return SendResponse(DialResponseMMISuccess(nsAutoString(aStatusMessage),
                                             AdditionalInformation(aAdditionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccessWithStrings(const nsAString& aStatusMessage,
                                                                      uint32_t aCount,
                                                                      const char16_t** aAdditionalInformation)
{
  nsTArray<nsString> additionalInformation;
  nsString* infos = additionalInformation.AppendElements(aCount);
  for (uint32_t i = 0; i < aCount; i++) {
    infos[i].Rebind(aAdditionalInformation[i],
                    nsCharTraits<char16_t>::length(aAdditionalInformation[i]));
  }

  return SendResponse(DialResponseMMISuccess(nsAutoString(aStatusMessage),
                                             AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMISuccessWithCallForwardingOptions(const nsAString& aStatusMessage,
                                                                                    uint32_t aCount,
                                                                                    nsIMobileCallForwardingOptions** aAdditionalInformation)
{
  nsTArray<nsIMobileCallForwardingOptions*> additionalInformation;
  for (uint32_t i = 0; i < aCount; i++) {
    additionalInformation.AppendElement(aAdditionalInformation[i]);
  }

  return SendResponse(DialResponseMMISuccess(nsAutoString(aStatusMessage),
                                             AdditionalInformation(additionalInformation)));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMIError(const nsAString& aError)
{
  return SendResponse(DialResponseMMIError(nsAutoString(aError),
                                           AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
TelephonyRequestParent::DialCallback::NotifyDialMMIErrorWithInfo(const nsAString& aError,
                                                                 uint16_t aInfo)
{
  return SendResponse(DialResponseMMIError(nsAutoString(aError),
                                           AdditionalInformation(aInfo)));
}
