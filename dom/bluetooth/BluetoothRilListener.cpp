/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothRilListener.h"

#include "BluetoothHfpManager.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

USING_BLUETOOTH_NAMESPACE

class BluetoothRILTelephonyCallback : public nsIRILTelephonyCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRILTELEPHONYCALLBACK

  BluetoothRILTelephonyCallback() { }
};

NS_IMPL_ISUPPORTS1(BluetoothRILTelephonyCallback, nsIRILTelephonyCallback)

NS_IMETHODIMP
BluetoothRILTelephonyCallback::CallStateChanged(uint32_t aCallIndex,
                                                uint16_t aCallState,
                                                const nsAString& aNumber,
                                                bool aIsActive)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->CallStateChanged(aCallIndex, aCallState, aNumber, aIsActive);

  return NS_OK;
}

NS_IMETHODIMP
BluetoothRILTelephonyCallback::EnumerateCallState(uint32_t aCallIndex,
                                                  uint16_t aCallState,
                                                  const nsAString_internal& aNumber,
                                                  bool aIsActive,
                                                  bool* aResult)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->EnumerateCallState(aCallIndex, aCallState, aNumber, aIsActive);
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
BluetoothRILTelephonyCallback::NotifyError(int32_t aCallIndex,
                                           const nsAString& aError)
{
  return NS_OK;
}

BluetoothRilListener::BluetoothRilListener()
{
  mRILTelephonyCallback = new BluetoothRILTelephonyCallback();
}

bool
BluetoothRilListener::StartListening()
{
  nsCOMPtr<nsIRILContentHelper> ril = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  if (!ril) {
    NS_ERROR("No RIL Service!");
    return false;
  }

  nsresult rv = ril->RegisterTelephonyCallback(mRILTelephonyCallback);
  NS_ENSURE_SUCCESS(rv, false);
  rv = ril->RegisterTelephonyMsg();
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothRilListener::StopListening()
{
  nsCOMPtr<nsIRILContentHelper> ril = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  if (!ril) {
    NS_ERROR("No RIL Service!");
    return false;
  }

  nsresult rv = ril->UnregisterTelephonyCallback(mRILTelephonyCallback);

  return NS_FAILED(rv) ? false : true;
}

nsIRILTelephonyCallback*
BluetoothRilListener::GetCallback()
{
  return mRILTelephonyCallback;
}
