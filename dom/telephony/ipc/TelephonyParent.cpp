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

  switch (aRequest.type()) {
    case IPCTelephonyRequest::TEnumerateCallsRequest:
      return actor->DoRequest(aRequest.get_EnumerateCallsRequest());
    case IPCTelephonyRequest::TDialRequest:
      return actor->DoRequest(aRequest.get_DialRequest());
    case IPCTelephonyRequest::TUSSDRequest:
      return actor->DoRequest(aRequest.get_USSDRequest());
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
TelephonyParent::RecvHangUpCall(const uint32_t& aClientId,
                                const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->HangUp(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvAnswerCall(const uint32_t& aClientId,
                                const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->AnswerCall(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvRejectCall(const uint32_t& aClientId,
                                const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->RejectCall(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvHoldCall(const uint32_t& aClientId,
                              const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->HoldCall(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvResumeCall(const uint32_t& aClientId,
                                const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->ResumeCall(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvConferenceCall(const uint32_t& aClientId)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->ConferenceCall(aClientId);
  return true;
}

bool
TelephonyParent::RecvSeparateCall(const uint32_t& aClientId,
                                  const uint32_t& aCallIndex)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->SeparateCall(aClientId, aCallIndex);
  return true;
}

bool
TelephonyParent::RecvHoldConference(const uint32_t& aClientId)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->HoldConference(aClientId);
  return true;
}

bool
TelephonyParent::RecvResumeConference(const uint32_t& aClientId)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, true);

  service->ResumeConference(aClientId);
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
TelephonyParent::CallStateChanged(uint32_t aClientId,
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
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCallStateData data(aCallIndex, aCallState, nsString(aNumber),
                        aNumberPresentation, nsString(aName), aNamePresentation,
                        aIsOutgoing, aIsEmergency, aIsConference,
                        aIsSwitchable, aIsMergeable);
  return SendNotifyCallStateChanged(aClientId, data) ? NS_OK : NS_ERROR_FAILURE;
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
TelephonyParent::EnumerateCallState(uint32_t aClientId,
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
TelephonyParent::NotifyError(uint32_t aClientId,
                             int32_t aCallIndex,
                             const nsAString& aError)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyCallError(aClientId, aCallIndex, nsString(aError))
      ? NS_OK : NS_ERROR_FAILURE;
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
                  nsITelephonyListener,
                  nsITelephonyCallback,
                  nsITelephonyDialCallback)

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
TelephonyRequestParent::DoRequest(const EnumerateCallsRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  if (service) {
    rv = service->EnumerateCalls(this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(EnumerateCallStateComplete());
  }

  return true;
}

bool
TelephonyRequestParent::DoRequest(const DialRequest& aRequest)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  if (service) {
    service->Dial(aRequest.clientId(), aRequest.number(),
                  aRequest.isEmergency(), this);
  } else {
    return NS_SUCCEEDED(NotifyError(NS_LITERAL_STRING("InvalidStateError")));
  }

  return true;
}

bool
TelephonyRequestParent::DoRequest(const USSDRequest& aRequest)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  if (service) {
    service->SendUSSD(aRequest.clientId(), aRequest.ussd(), this);
  } else {
    return NS_SUCCEEDED(NotifyError(NS_LITERAL_STRING("InvalidStateError")));
  }

  return true;
}

nsresult
TelephonyRequestParent::SendResponse(const IPCTelephonyResponse& aResponse)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, aResponse) ? NS_OK : NS_ERROR_FAILURE;
}

// nsITelephonyListener

NS_IMETHODIMP
TelephonyRequestParent::CallStateChanged(uint32_t aClientId,
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

  return Send__delete__(this, EnumerateCallsResponse()) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::EnumerateCallState(uint32_t aClientId,
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
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  IPCCallStateData data(aCallIndex, aCallState, nsString(aNumber),
                        aNumberPresentation, nsString(aName), aNamePresentation,
                        aIsOutgoing, aIsEmergency, aIsConference,
                        aIsSwitchable, aIsMergeable);
  return SendNotifyEnumerateCallState(aClientId, data) ? NS_OK
                                                       : NS_ERROR_FAILURE;
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
TelephonyRequestParent::NotifyError(uint32_t aClientId,
                                    int32_t aCallIndex,
                                    const nsAString& aError)
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

// nsITelephonyDialCallback

NS_IMETHODIMP
TelephonyRequestParent::NotifyDialMMI(const nsAString& aServiceCode)
{
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return SendNotifyDialMMI(nsAutoString(aServiceCode)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TelephonyRequestParent::NotifySuccess()
{
  return SendResponse(SuccessResponse());
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyError(const nsAString& aError)
{
  return SendResponse(ErrorResponse(nsAutoString(aError)));
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyDialCallSuccess(uint32_t aCallIndex,
                                              const nsAString& aNumber)
{
  return SendResponse(DialResponseCallSuccess(aCallIndex, nsAutoString(aNumber)));
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyDialMMISuccess(JS::Handle<JS::Value> aResult)
{
  AutoSafeJSContext cx;
  RootedDictionary<MozMMIResult> result(cx);

  if (!result.Init(cx, aResult)) {
    return NS_ERROR_TYPE_ERR;
  }

  // No additionInformation passed
  if (!result.mAdditionalInformation.WasPassed()) {
    return SendResponse(DialResponseMMISuccess(result.mStatusMessage,
                                               AdditionalInformation(mozilla::void_t())));
  }

  OwningUnsignedShortOrObject& info = result.mAdditionalInformation.Value();

  // Currently, we could only accept the following values for |info|:
  //   1. array of string
  //   2. array of MozCallForwardingOptions
  if (!info.IsObject()) {
    return NS_ERROR_TYPE_ERR;
  }

  JS::Rooted<JSObject*> object(cx, info.GetAsObject());
  JS::Rooted<JS::Value> value(cx);
  uint32_t length;

  if (!JS_IsArrayObject(cx, object) ||
      !JS_GetArrayLength(cx, object, &length) || length <= 0 ||
      // Check first element to decide the format of array.
      !JS_GetElement(cx, object, 0, &value)) {
    return NS_ERROR_TYPE_ERR;
  }

  if (value.isString()) {
    // String[]
    nsTArray<nsString> infos;

    for (uint32_t i = 0; i < length; i++) {
      nsAutoJSString str;
      if (!JS_GetElement(cx, object, i, &value) || !value.isString() ||
          !str.init(cx, value.toString())) {
        return NS_ERROR_TYPE_ERR;
      }
      infos.AppendElement(str);
    }

    return SendResponse(DialResponseMMISuccess(result.mStatusMessage,
                                               AdditionalInformation(infos)));
  } else {
    // IPC::MozCallForwardingOptions[]
    nsTArray<IPC::MozCallForwardingOptions> infos;

    for (uint32_t i = 0; i < length; i++) {
      IPC::MozCallForwardingOptions info;
      if (!JS_GetElement(cx, object, i, &value) || !info.Init(cx, value)) {
        return NS_ERROR_TYPE_ERR;
      }
      infos.AppendElement(info);
    }

    return SendResponse(DialResponseMMISuccess(result.mStatusMessage,
                                               AdditionalInformation(infos)));
  }
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyDialMMIError(const nsAString& aError)
{
  return SendResponse(DialResponseMMIError(nsAutoString(aError),
                                           AdditionalInformation(mozilla::void_t())));
}

NS_IMETHODIMP
TelephonyRequestParent::NotifyDialMMIErrorWithInfo(const nsAString& aError,
                                                   uint16_t aInfo)
{
  return SendResponse(DialResponseMMIError(nsAutoString(aError),
                                           AdditionalInformation(aInfo)));
}
