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
                                    bool aIsOutgoing)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->HandleCallStateChanged(aCallIndex, aCallState, aNumber,
                              aIsOutgoing, true);

  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::EnumerateCallState(uint32_t aCallIndex,
                                      uint16_t aCallState,
                                      const nsAString_internal& aNumber,
                                      bool aIsActive,
                                      bool aIsOutgoing,
                                      bool* aResult)
{
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->HandleCallStateChanged(aCallIndex, aCallState, aNumber,
                              aIsOutgoing, false);
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyListener::NotifyError(int32_t aCallIndex,
                               const nsAString& aError)
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
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE(provider, false);

  nsresult rv = provider->RegisterTelephonyMsg(mTelephonyListener);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
BluetoothTelephonyListener::StopListening()
{
  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE(provider, false);

  nsresult rv = provider->UnregisterTelephonyMsg(mTelephonyListener);

  return NS_FAILED(rv) ? false : true;
}

nsITelephonyListener*
BluetoothTelephonyListener::GetListener()
{
  return mTelephonyListener;
}
