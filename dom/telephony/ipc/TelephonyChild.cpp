/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyChild.h"

#include "mozilla/dom/telephony/TelephonyDialCallback.h"
#include "TelephonyIPCService.h"

USING_TELEPHONY_NAMESPACE

/*******************************************************************************
 * TelephonyChild
 ******************************************************************************/

TelephonyChild::TelephonyChild(TelephonyIPCService* aService)
  : mService(aService)
{
  MOZ_ASSERT(aService);
}

TelephonyChild::~TelephonyChild()
{
}

void
TelephonyChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mService) {
    mService->NoteActorDestroyed();
    mService = nullptr;
  }
}

PTelephonyRequestChild*
TelephonyChild::AllocPTelephonyRequestChild(const IPCTelephonyRequest& aRequest)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
TelephonyChild::DeallocPTelephonyRequestChild(PTelephonyRequestChild* aActor)
{
  delete aActor;
  return true;
}

bool
TelephonyChild::RecvNotifyCallError(const uint32_t& aClientId,
                                    const int32_t& aCallIndex,
                                    const nsString& aError)
{
  MOZ_ASSERT(mService);

  mService->NotifyError(aClientId, aCallIndex, aError);
  return true;
}

bool
TelephonyChild::RecvNotifyCallStateChanged(const uint32_t& aClientId,
                                           const IPCCallStateData& aData)
{
  MOZ_ASSERT(mService);

  mService->CallStateChanged(aClientId,
                              aData.callIndex(),
                              aData.callState(),
                              aData.number(),
                              aData.numberPresentation(),
                              aData.name(),
                              aData.namePresentation(),
                              aData.isOutGoing(),
                              aData.isEmergency(),
                              aData.isConference(),
                              aData.isSwitchable(),
                              aData.isMergeable());
  return true;
}

bool
TelephonyChild::RecvNotifyCdmaCallWaiting(const uint32_t& aClientId,
                                          const IPCCdmaWaitingCallData& aData)
{
  MOZ_ASSERT(mService);

  mService->NotifyCdmaCallWaiting(aClientId,
                                  aData.number(),
                                  aData.numberPresentation(),
                                  aData.name(),
                                  aData.namePresentation());
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceCallStateChanged(const uint16_t& aCallState)
{
  MOZ_ASSERT(mService);

  mService->ConferenceCallStateChanged(aCallState);
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceError(const nsString& aName,
                                          const nsString& aMessage)
{
  MOZ_ASSERT(mService);

  mService->NotifyConferenceError(aName, aMessage);
  return true;
}

bool
TelephonyChild::RecvNotifySupplementaryService(const uint32_t& aClientId,
                                               const int32_t& aCallIndex,
                                               const uint16_t& aNotification)
{
  MOZ_ASSERT(mService);

  mService->SupplementaryServiceNotification(aClientId, aCallIndex,
                                              aNotification);
  return true;
}

/*******************************************************************************
 * TelephonyRequestChild
 ******************************************************************************/

TelephonyRequestChild::TelephonyRequestChild(nsITelephonyListener* aListener,
                                             nsITelephonyCallback* aCallback)
  : mListener(aListener), mCallback(aCallback)
{
}

void
TelephonyRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mListener = nullptr;
  mCallback = nullptr;
}

bool
TelephonyRequestChild::Recv__delete__(const IPCTelephonyResponse& aResponse)
{
  switch (aResponse.type()) {
    case IPCTelephonyResponse::TEnumerateCallsResponse:
      mListener->EnumerateCallStateComplete();
      break;
    case IPCTelephonyResponse::TSuccessResponse:
      return DoResponse(aResponse.get_SuccessResponse());
    case IPCTelephonyResponse::TErrorResponse:
      return DoResponse(aResponse.get_ErrorResponse());
    case IPCTelephonyResponse::TDialResponseCallSuccess:
      return DoResponse(aResponse.get_DialResponseCallSuccess());
    case IPCTelephonyResponse::TDialResponseMMISuccess:
      return DoResponse(aResponse.get_DialResponseMMISuccess());
    case IPCTelephonyResponse::TDialResponseMMIError:
      return DoResponse(aResponse.get_DialResponseMMIError());
    default:
      MOZ_CRASH("Unknown type!");
  }

  return true;
}

bool
TelephonyRequestChild::RecvNotifyEnumerateCallState(const uint32_t& aClientId,
                                                    const IPCCallStateData& aData)
{
  MOZ_ASSERT(mListener);

  mListener->EnumerateCallState(aClientId,
                                aData.callIndex(),
                                aData.callState(),
                                aData.number(),
                                aData.numberPresentation(),
                                aData.name(),
                                aData.namePresentation(),
                                aData.isOutGoing(),
                                aData.isEmergency(),
                                aData.isConference(),
                                aData.isSwitchable(),
                                aData.isMergeable());
  return true;
}

bool
TelephonyRequestChild::RecvNotifyDialMMI(const nsString& aServiceCode)
{
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);
  callback->NotifyDialMMI(aServiceCode);
  return true;
}

bool
TelephonyRequestChild::DoResponse(const SuccessResponse& aResponse)
{
  MOZ_ASSERT(mCallback);
  mCallback->NotifySuccess();
  return true;
}

bool
TelephonyRequestChild::DoResponse(const ErrorResponse& aResponse)
{
  MOZ_ASSERT(mCallback);
  mCallback->NotifyError(aResponse.name());
  return true;
}

bool
TelephonyRequestChild::DoResponse(const DialResponseCallSuccess& aResponse)
{
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);
  callback->NotifyDialCallSuccess(aResponse.callIndex(), aResponse.number());
  return true;
}

bool
TelephonyRequestChild::DoResponse(const DialResponseMMISuccess& aResponse)
{
  MOZ_ASSERT(mCallback);

  // FIXME: Need to overload NotifyDialMMISuccess in the IDL. mCallback is not
  // necessarily an instance of TelephonyCallback.
  nsCOMPtr<nsITelephonyDialCallback> dialCallback = do_QueryInterface(mCallback);
  nsRefPtr<TelephonyDialCallback> callback = static_cast<TelephonyDialCallback*>(dialCallback.get());

  nsAutoString statusMessage(aResponse.statusMessage());
  AdditionalInformation info(aResponse.additionalInformation());

  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      callback->NotifyDialMMISuccess(statusMessage);
      break;
    case AdditionalInformation::TArrayOfnsString:
      callback->NotifyDialMMISuccess(statusMessage, info.get_ArrayOfnsString());
      break;
    case AdditionalInformation::TArrayOfMozCallForwardingOptions:
      callback->NotifyDialMMISuccess(statusMessage, info.get_ArrayOfMozCallForwardingOptions());
      break;
    default:
      MOZ_CRASH("Received invalid type!");
      break;
  }

  return true;
}

bool
TelephonyRequestChild::DoResponse(const DialResponseMMIError& aResponse)
{
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);

  nsAutoString name(aResponse.name());
  AdditionalInformation info(aResponse.additionalInformation());

  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      callback->NotifyDialMMIError(name);
      break;
    case AdditionalInformation::Tuint16_t:
      callback->NotifyDialMMIErrorWithInfo(name, info.get_uint16_t());
      break;
    default:
      MOZ_CRASH("Received invalid type!");
      break;
  }

  return true;
}
