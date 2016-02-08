/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyChild.h"

#include "mozilla/dom/telephony/TelephonyDialCallback.h"
#include "mozilla/UniquePtr.h"
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
TelephonyChild::RecvNotifyCallStateChanged(nsTArray<nsITelephonyCallInfo*>&& aAllInfo)
{
  uint32_t length = aAllInfo.Length();
  nsTArray<nsCOMPtr<nsITelephonyCallInfo>> results;
  for (uint32_t i = 0; i < length; ++i) {
    // Use dont_AddRef here because this instance has already been AddRef-ed in
    // TelephonyIPCSerializer.h
    nsCOMPtr<nsITelephonyCallInfo> info = dont_AddRef(aAllInfo[i]);
    results.AppendElement(info);
  }

  MOZ_ASSERT(mService);

  mService->CallStateChanged(length, const_cast<nsITelephonyCallInfo**>(aAllInfo.Elements()));

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
TelephonyRequestChild::RecvNotifyEnumerateCallState(nsITelephonyCallInfo* const& aInfo)
{
  // Use dont_AddRef here because this instances has already been AddRef-ed in
  // TelephonyIPCSerializer.h
  nsCOMPtr<nsITelephonyCallInfo> info = dont_AddRef(aInfo);

  MOZ_ASSERT(mListener);

  mListener->EnumerateCallState(aInfo);

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
  callback->NotifyDialCallSuccess(aResponse.clientId(), aResponse.callIndex(),
                                  aResponse.number());
  return true;
}

bool
TelephonyRequestChild::DoResponse(const DialResponseMMISuccess& aResponse)
{
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsITelephonyDialCallback> callback = do_QueryInterface(mCallback);

  nsAutoString statusMessage(aResponse.statusMessage());
  AdditionalInformation info(aResponse.additionalInformation());

  switch (info.type()) {
    case AdditionalInformation::Tvoid_t:
      callback->NotifyDialMMISuccess(statusMessage);
      break;
    case AdditionalInformation::Tuint16_t:
      callback->NotifyDialMMISuccessWithInteger(statusMessage, info.get_uint16_t());
      break;
    case AdditionalInformation::TArrayOfnsString: {
      uint32_t count = info.get_ArrayOfnsString().Length();
      const nsTArray<nsString>& additionalInformation = info.get_ArrayOfnsString();

      auto additionalInfoPtrs = MakeUnique<const char16_t*[]>(count);
      for (size_t i = 0; i < count; ++i) {
        additionalInfoPtrs[i] = additionalInformation[i].get();
      }

      callback->NotifyDialMMISuccessWithStrings(statusMessage, count,
                                                additionalInfoPtrs.get());
      break;
    }
    case AdditionalInformation::TArrayOfnsMobileCallForwardingOptions: {
      uint32_t count = info.get_ArrayOfnsMobileCallForwardingOptions().Length();

      nsTArray<nsCOMPtr<nsIMobileCallForwardingOptions>> results;
      for (uint32_t i = 0; i < count; i++) {
        // Use dont_AddRef here because these instances are already AddRef-ed in
        // MobileConnectionIPCSerializer.h
        nsCOMPtr<nsIMobileCallForwardingOptions> item = dont_AddRef(
          info.get_ArrayOfnsMobileCallForwardingOptions()[i]);
        results.AppendElement(item);
      }

      callback->NotifyDialMMISuccessWithCallForwardingOptions(statusMessage, count,
        const_cast<nsIMobileCallForwardingOptions**>(info.get_ArrayOfnsMobileCallForwardingOptions().Elements()));
      break;
    }
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
