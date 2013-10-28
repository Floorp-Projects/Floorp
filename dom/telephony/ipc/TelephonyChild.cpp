/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyChild.h"

USING_TELEPHONY_NAMESPACE

/*******************************************************************************
 * TelephonyChild
 ******************************************************************************/

TelephonyChild::TelephonyChild(nsITelephonyListener* aListener)
  : mListener(aListener)
{
  MOZ_ASSERT(aListener);
}

void
TelephonyChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mListener = nullptr;
}

PTelephonyRequestChild*
TelephonyChild::AllocPTelephonyRequestChild()
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
  MOZ_ASSERT(mListener);

  mListener->NotifyError(aClientId, aCallIndex, aError);
  return true;
}

bool
TelephonyChild::RecvNotifyCallStateChanged(const uint32_t& aClientId,
                                           const IPCCallStateData& aData)
{
  MOZ_ASSERT(mListener);

  mListener->CallStateChanged(aClientId,
                              aData.callIndex(),
                              aData.callState(),
                              aData.number(),
                              aData.isActive(),
                              aData.isOutGoing(),
                              aData.isEmergency(),
                              aData.isConference());
  return true;
}

bool
TelephonyChild::RecvNotifyCdmaCallWaiting(const uint32_t& aClientId,
                                          const nsString& aNumber)
{
  MOZ_ASSERT(mListener);

  mListener->NotifyCdmaCallWaiting(aClientId, aNumber);
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceCallStateChanged(const uint16_t& aCallState)
{
  MOZ_ASSERT(mListener);

  mListener->ConferenceCallStateChanged(aCallState);
  return true;
}

bool
TelephonyChild::RecvNotifyConferenceError(const nsString& aName,
                                          const nsString& aMessage)
{
  MOZ_ASSERT(mListener);

  mListener->NotifyConferenceError(aName, aMessage);
  return true;
}

bool
TelephonyChild::RecvNotifySupplementaryService(const uint32_t& aClientId,
                                               const int32_t& aCallIndex,
                                               const uint16_t& aNotification)
{
  MOZ_ASSERT(mListener);

  mListener->SupplementaryServiceNotification(aClientId, aCallIndex,
                                              aNotification);
  return true;
}

/*******************************************************************************
 * TelephonyRequestChild
 ******************************************************************************/

TelephonyRequestChild::TelephonyRequestChild(nsITelephonyListener* aListener)
  : mListener(aListener)
{
  MOZ_ASSERT(aListener);
}

void
TelephonyRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mListener = nullptr;
}

bool
TelephonyRequestChild::Recv__delete__()
{
  MOZ_ASSERT(mListener);

  mListener->EnumerateCallStateComplete();
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
                                aData.isConference());
  return true;
}
