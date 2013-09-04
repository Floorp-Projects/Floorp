/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothTelephonyListener.h"

#include "BluetoothHfpManager.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

USING_BLUETOOTH_NAMESPACE

namespace {

class TelephonyListener : public nsITelephonyListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

  TelephonyListener() { }
};

NS_IMPL_ISUPPORTS1(TelephonyListener, nsITelephonyListener)

NS_IMETHODIMP
TelephonyListener::CallStateChanged(uint32_t aCallIndex,
                                    uint16_t aCallState,
                                    const nsAString& aNumber,
                                    bool aIsActive,
                                    bool aIsOutgoing,
                                    bool aIsEmergency,
                                    bool aIsConference)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->HandleCallStateChanged(aCallIndex, aCallState, EmptyString(), aNumber,
                              aIsOutgoing, true);

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
TelephonyListener::EnumerateCallState(uint32_t aCallIndex,
                                      uint16_t aCallState,
                                      const nsAString_internal& aNumber,
                                      bool aIsActive,
                                      bool aIsOutgoing,
                                      bool aIsEmergency,
                                      bool aIsConference)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->HandleCallStateChanged(aCallIndex, aCallState, EmptyString(), aNumber,
                              aIsOutgoing, false);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::SupplementaryServiceNotification(int32_t aCallIndex,
                                                    uint16_t aNotification)
{
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::NotifyError(int32_t aCallIndex,
                               const nsAString& aError)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();

  if (aCallIndex > 0) {
    // In order to not miss any related call state transition.
    // It's possible that 3G network signal lost for unknown reason.
    // If a call is released abnormally, NotifyError() will be called,
    // instead of CallStateChanged(). We need to reset the call array state
    // via setting CALL_STATE_DISCONNECTED
    hfp->HandleCallStateChanged(aCallIndex,
                                nsITelephonyProvider::CALL_STATE_DISCONNECTED,
                                aError, EmptyString(), false, true);
    NS_WARNING("Reset the call state due to call transition ends abnormally");
  }

  NS_WARNING(NS_ConvertUTF16toUTF8(aError).get());
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::NotifyCdmaCallWaiting(const nsAString& aNumber)
{
  return NS_OK;
}

} // anonymous namespace

BluetoothTelephonyListener::BluetoothTelephonyListener()
{
  mTelephonyListener = new TelephonyListener();
}

bool
BluetoothTelephonyListener::StartListening()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, false);

  nsresult rv = provider->RegisterListener(mTelephonyListener);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothTelephonyListener::StopListening()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(TELEPHONY_PROVIDER_CONTRACTID);
  NS_ENSURE_TRUE(provider, false);

  nsresult rv = provider->UnregisterListener(mTelephonyListener);

  return NS_FAILED(rv) ? false : true;
}

nsITelephonyListener*
BluetoothTelephonyListener::GetListener()
{
  return mTelephonyListener;
}
