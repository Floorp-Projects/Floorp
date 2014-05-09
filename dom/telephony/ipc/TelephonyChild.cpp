/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyChild.h"
#include "TelephonyIPCProvider.h"

USING_TELEPHONY_NAMESPACE

/*******************************************************************************
 * TelephonyChild
 ******************************************************************************/

TelephonyChild::TelephonyChild(TelephonyIPCProvider* aProvider)
  : mProvider(aProvider)
{
  MOZ_ASSERT(aProvider);
}

TelephonyChild::~TelephonyChild()
{
}

void
TelephonyChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mProvider) {
    mProvider->NoteActorDestroyed();
    mProvider = nullptr;
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
  MOZ_ASSERT(mProvider);

  mProvider->NotifyError(aClientId, aCallIndex, aError);
  return true;
}

bool
TelephonyChild::RecvNotifyCallStateChanged(const uint32_t& aClientId,
                                           const IPCCallStateData& aData)
{
  MOZ_ASSERT(mProvider);

  mProvider->CallStateChanged(aClientId,
                              aData.callIndex(),
                              aData.callState(),
                              aData.number(),
                              aData.isActive(),
                              aData.isOutGoing(),
                              aData.isEmergency(),
                              aData.isConference(),
                              aData.isSwitchable(),
                              aData.isMergeable());
  return true;
}

bool
TelephonyChild::RecvNotifyCdmaCallWaiting(const uint32_t& aClientId,
                                          const nsString& aNumber)
{
  MOZ_ASSERT(mProvider);

  mProvider->NotifyCdmaCallWaiting(aClientId, aNumber);
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceCallStateChanged(const uint16_t& aCallState)
{
  MOZ_ASSERT(mProvider);

  mProvider->ConferenceCallStateChanged(aCallState);
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceError(const nsString& aName,
                                          const nsString& aMessage)
{
  MOZ_ASSERT(mProvider);

  mProvider->NotifyConferenceError(aName, aMessage);
  return true;
}

bool
TelephonyChild::RecvNotifySupplementaryService(const uint32_t& aClientId,
                                               const int32_t& aCallIndex,
                                               const uint16_t& aNotification)
{
  MOZ_ASSERT(mProvider);

  mProvider->SupplementaryServiceNotification(aClientId, aCallIndex,
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
    case IPCTelephonyResponse::TDialResponse:
      // Do nothing.
      break;
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
                                aData.isActive(),
                                aData.isOutGoing(),
                                aData.isEmergency(),
                                aData.isConference(),
                                aData.isSwitchable(),
                                aData.isMergeable());
  return true;
}

bool
TelephonyRequestChild::RecvNotifyDialError(const nsString& aError)
{
  MOZ_ASSERT(mCallback);

  mCallback->NotifyDialError(aError);
  return true;
}

bool
TelephonyRequestChild::RecvNotifyDialSuccess()
{
  MOZ_ASSERT(mCallback);

  mCallback->NotifyDialSuccess();
  return true;
}
