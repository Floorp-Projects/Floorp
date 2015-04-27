/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothRilListener.h"

#include "BluetoothHfpManager.h"
#include "nsIIccService.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileConnectionService.h"
#include "nsITelephonyService.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsQueryObject.h"

USING_BLUETOOTH_NAMESPACE

/**
 *  IccListener
 */
NS_IMPL_ISUPPORTS(IccListener, nsIIccListener)

NS_IMETHODIMP
IccListener::NotifyIccInfoChanged()
{
  // mOwner would be set to nullptr only in the dtor of BluetoothRilListener
  NS_ENSURE_TRUE(mOwner, NS_ERROR_FAILURE);

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(hfp, NS_ERROR_FAILURE);

  hfp->HandleIccInfoChanged(mOwner->mClientId);

  return NS_OK;
}

NS_IMETHODIMP
IccListener::NotifyStkCommand(nsIStkProactiveCmd *aStkProactiveCmd)
{
  return NS_OK;
}

NS_IMETHODIMP
IccListener::NotifyStkSessionEnd()
{
  return NS_OK;
}

NS_IMETHODIMP
IccListener::NotifyCardStateChanged()
{
  return NS_OK;
}

bool
IccListener::Listen(bool aStart)
{
  NS_ENSURE_TRUE(mOwner, false);

  nsCOMPtr<nsIIccService> service =
    do_GetService(ICC_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, false);

  nsCOMPtr<nsIIcc> icc;
  service->GetIccByServiceId(mOwner->mClientId, getter_AddRefs(icc));
  NS_ENSURE_TRUE(icc, false);

  nsresult rv;
  if (aStart) {
    rv = icc->RegisterListener(this);
  } else {
    rv = icc->UnregisterListener(this);
  }

  return NS_SUCCEEDED(rv);
}

void
IccListener::SetOwner(BluetoothRilListener *aOwner)
{
  mOwner = aOwner;
}

/**
 *  MobileConnectionListener
 */
NS_IMPL_ISUPPORTS(MobileConnectionListener, nsIMobileConnectionListener)

NS_IMETHODIMP
MobileConnectionListener::NotifyVoiceChanged()
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(hfp, NS_OK);

  hfp->HandleVoiceConnectionChanged(mClientId);

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyDataChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyDataError(const nsAString & message)
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyCFStateChanged(uint16_t action,
                                               uint16_t reason,
                                               const nsAString& number,
                                               uint16_t timeSeconds,
                                               uint16_t serviceClass)
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyEmergencyCbModeChanged(bool active,
                                                       uint32_t timeoutMs)
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyOtaStatusChanged(const nsAString & status)
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyRadioStateChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyClirModeChanged(uint32_t aMode)
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyLastKnownNetworkChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyLastKnownHomeNetworkChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionListener::NotifyNetworkSelectionModeChanged()
{
  return NS_OK;
}

bool
MobileConnectionListener::Listen(bool aStart)
{
  nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, false);

  nsCOMPtr<nsIMobileConnection> connection;
  service->GetItemByServiceId(mClientId, getter_AddRefs(connection));
  NS_ENSURE_TRUE(connection, false);

  nsresult rv;
  if (aStart) {
    rv = connection->RegisterListener(this);
  } else {
    rv = connection->UnregisterListener(this);
  }

  return NS_SUCCEEDED(rv);
}

/**
 *  TelephonyListener Implementation
 */
NS_IMPL_ISUPPORTS(TelephonyListener, nsITelephonyListener)

/**
 * @param aSend A boolean indicates whether we need to notify headset or not
 */
nsresult
TelephonyListener::HandleCallInfo(nsITelephonyCallInfo* aInfo, bool aSend)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(hfp, NS_ERROR_FAILURE);

  uint32_t callIndex;
  uint16_t callState;
  nsAutoString number;
  bool isOutgoing;
  bool isConference;

  aInfo->GetCallIndex(&callIndex);
  aInfo->GetCallState(&callState);
  aInfo->GetNumber(number);
  aInfo->GetIsOutgoing(&isOutgoing);
  aInfo->GetIsConference(&isConference);

  hfp->HandleCallStateChanged(callIndex, callState, EmptyString(), number,
                              isOutgoing, isConference, aSend);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::CallStateChanged(uint32_t aLength,
                                    nsITelephonyCallInfo** aAllInfo)
{
  for (uint32_t i = 0; i < aLength; ++i) {
    HandleCallInfo(aAllInfo[i], true);
  }
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::EnumerateCallState(nsITelephonyCallInfo* aInfo)
{
  return HandleCallInfo(aInfo, false);
}

NS_IMETHODIMP
TelephonyListener::NotifyError(uint32_t aServiceId,
                               int32_t aCallIndex,
                               const nsAString& aError)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(hfp, NS_ERROR_FAILURE);

  if (aCallIndex > 0) {
    // In order to not miss any related call state transition.
    // It's possible that 3G network signal lost for unknown reason.
    // If a call is released abnormally, NotifyError() will be called,
    // instead of CallStateChanged(). We need to reset the call array state
    // via setting CALL_STATE_DISCONNECTED
    hfp->HandleCallStateChanged(aCallIndex,
                                nsITelephonyService::CALL_STATE_DISCONNECTED,
                                aError, EmptyString(), false, false, true);
    BT_WARNING("Reset the call state due to call transition ends abnormally");
  }

  BT_WARNING(NS_ConvertUTF16toUTF8(aError).get());
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::ConferenceCallStateChanged(uint16_t aCallState)
{
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::EnumerateCallStateComplete()
{
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::SupplementaryServiceNotification(uint32_t aServiceId,
                                                    int32_t aCallIndex,
                                                    uint16_t aNotification)
{
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::NotifyConferenceError(const nsAString& aName,
                                         const nsAString& aMessage)
{
  BT_WARNING(NS_ConvertUTF16toUTF8(aName).get());
  BT_WARNING(NS_ConvertUTF16toUTF8(aMessage).get());

  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::NotifyCdmaCallWaiting(uint32_t aServiceId,
                                         const nsAString& aNumber,
                                         uint16_t aNumberPresentation,
                                         const nsAString& aName,
                                         uint16_t aNamePresentation)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(hfp, NS_ERROR_FAILURE);

  hfp->UpdateSecondNumber(aNumber);

  return NS_OK;
}

bool
TelephonyListener::Listen(bool aStart)
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(service, false);

  nsresult rv;
  if (aStart) {
    rv = service->RegisterListener(this);
  } else {
    rv = service->UnregisterListener(this);
  }

  return NS_SUCCEEDED(rv);
}

/**
 *  BluetoothRilListener
 */
BluetoothRilListener::BluetoothRilListener()
{
  nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  // Query number of total clients (sim slots)
  uint32_t numItems = 0;
  if (NS_SUCCEEDED(service->GetNumItems(&numItems))) {
    // Init MobileConnectionListener array and IccInfoListener
    for (uint32_t i = 0; i < numItems; i++) {
      mMobileConnListeners.AppendElement(new MobileConnectionListener(i));
    }
  }

  mTelephonyListener = new TelephonyListener();
  mIccListener = new IccListener();
  mIccListener->SetOwner(this);

  // Probe for available client
  SelectClient();
}

BluetoothRilListener::~BluetoothRilListener()
{
  mIccListener->SetOwner(nullptr);
}

bool
BluetoothRilListener::Listen(bool aStart)
{
  NS_ENSURE_TRUE(ListenMobileConnAndIccInfo(aStart), false);
  NS_ENSURE_TRUE(mTelephonyListener->Listen(aStart), false);

  return true;
}

void
BluetoothRilListener::SelectClient()
{
  // Reset mClientId
  mClientId = mMobileConnListeners.Length();

  nsCOMPtr<nsIMobileConnectionService> service =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  for (uint32_t i = 0; i < mMobileConnListeners.Length(); i++) {
    nsCOMPtr<nsIMobileConnection> connection;
    service->GetItemByServiceId(i, getter_AddRefs(connection));
    if (!connection) {
      BT_WARNING("%s: Failed to get mobile connection", __FUNCTION__);
      continue;
    }

    nsCOMPtr<nsIMobileConnectionInfo> voiceInfo;
    connection->GetVoice(getter_AddRefs(voiceInfo));
    if (!voiceInfo) {
      BT_WARNING("%s: Failed to get voice connection info", __FUNCTION__);
      continue;
    }

    nsString regState;
    voiceInfo->GetState(regState);
    if (regState.EqualsLiteral("registered")) {
      // Found available client
      mClientId = i;
      return;
    }
  }
}

void
BluetoothRilListener::ServiceChanged(uint32_t aClientId, bool aRegistered)
{
  // Stop listening
  ListenMobileConnAndIccInfo(false);

  /**
   * aRegistered:
   * - TRUE:  service becomes registered. We were listening to all clients
   *          and one of them becomes available. Select it to listen.
   * - FALSE: service becomes un-registered. The client we were listening
   *          becomes unavailable. Select another registered one to listen.
   */
  if (aRegistered) {
    mClientId = aClientId;
  } else {
    SelectClient();
  }

  // Restart listening
  ListenMobileConnAndIccInfo(true);

  BT_LOGR("%d client %d. new mClientId %d", aRegistered, aClientId,
          (mClientId < mMobileConnListeners.Length()) ? mClientId : -1);
}

void
BluetoothRilListener::EnumerateCalls()
{
  nsCOMPtr<nsITelephonyService> service =
    do_GetService(TELEPHONY_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  nsCOMPtr<nsITelephonyListener> listener(
    do_QueryObject(mTelephonyListener));

  service->EnumerateCalls(listener);
}

bool
BluetoothRilListener::ListenMobileConnAndIccInfo(bool aStart)
{
  /**
   * mClientId < number of total clients:
   *   The client with mClientId is available. Start/Stop listening
   *   mobile connection and icc info of this client only.
   *
   * mClientId >= number of total clients:
   *   All clients are unavailable. Start/Stop listening mobile
   *   connections of all clients.
   */
  if (mClientId < mMobileConnListeners.Length()) {
    NS_ENSURE_TRUE(mMobileConnListeners[mClientId]->Listen(aStart), false);
    NS_ENSURE_TRUE(mIccListener->Listen(aStart), false);
  } else {
    for (uint32_t i = 0; i < mMobileConnListeners.Length(); i++) {
      NS_ENSURE_TRUE(mMobileConnListeners[i]->Listen(aStart), false);
    }
  }

  return true;
}
