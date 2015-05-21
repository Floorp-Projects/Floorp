/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyCallInfo.h"

namespace mozilla {
namespace dom {
namespace telephony {

NS_IMPL_ISUPPORTS(TelephonyCallInfo, nsITelephonyCallInfo)

TelephonyCallInfo::TelephonyCallInfo(uint32_t aClientId,
                                     uint32_t aCallIndex,
                                     uint16_t aCallState,
                                     const nsAString& aDisconnectedReason,

                                     const nsAString& aNumber,
                                     uint16_t aNumberPresentation,
                                     const nsAString& aName,
                                     uint16_t aNamePresentation,

                                     bool aIsOutgoing,
                                     bool aIsEmergency,
                                     bool aIsConference,
                                     bool aIsSwitchable,
                                     bool aIsMergeable)
  : mClientId(aClientId),
    mCallIndex(aCallIndex),
    mCallState(aCallState),
    mDisconnectedReason(aDisconnectedReason),

    mNumber(aNumber),
    mNumberPresentation(aNumberPresentation),
    mName(aName),
    mNamePresentation(aNamePresentation),

    mIsOutgoing(aIsOutgoing),
    mIsEmergency(aIsEmergency),
    mIsConference(aIsConference),
    mIsSwitchable(aIsSwitchable),
    mIsMergeable(aIsMergeable)
{
}

NS_IMETHODIMP
TelephonyCallInfo::GetClientId(uint32_t* aClientId)
{
  *aClientId = mClientId;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetCallIndex(uint32_t* aCallIndex)
{
  *aCallIndex = mCallIndex;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetCallState(uint16_t* aCallState)
{
  *aCallState = mCallState;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetDisconnectedReason(nsAString& aDisconnectedReason)
{
  aDisconnectedReason = mDisconnectedReason;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNumberPresentation(uint16_t* aNumberPresentation)
{
  *aNumberPresentation = mNumberPresentation;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNamePresentation(uint16_t* aNamePresentation)
{
  *aNamePresentation = mNamePresentation;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsOutgoing(bool* aIsOutgoing)
{
  *aIsOutgoing = mIsOutgoing;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsEmergency(bool* aIsEmergency)
{
  *aIsEmergency = mIsEmergency;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsConference(bool* aIsConference)
{
  *aIsConference = mIsConference;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsSwitchable(bool* aIsSwitchable)
{
  *aIsSwitchable = mIsSwitchable;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsMergeable(bool* aIsMergeable)
{
  *aIsMergeable = mIsMergeable;
  return NS_OK;
}

} // namespace telephony
} // namespace dom
} // namespace mozilla
