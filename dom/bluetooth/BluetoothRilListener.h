/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothrillistener_h__
#define mozilla_dom_bluetooth_bluetoothrillistener_h__

#include "BluetoothCommon.h"

#include "nsAutoPtr.h"

#include "nsIIccProvider.h"
#include "nsIMobileConnectionProvider.h"
#include "nsITelephonyProvider.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothRilListener;

class IccListener : public nsIIccListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCLISTENER

  IccListener() { }
  virtual ~IccListener() { }

  bool Listen(bool aStart);
  void SetOwner(BluetoothRilListener *aOwner);

private:
  BluetoothRilListener* mOwner;
};

class MobileConnectionListener : public nsIMobileConnectionListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTIONLISTENER

  MobileConnectionListener(uint32_t aClientId)
  : mClientId(aClientId) { }
  virtual ~MobileConnectionListener() { }

  bool Listen(bool aStart);

private:
  uint32_t mClientId;
};

class TelephonyListener : public nsITelephonyListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

  TelephonyListener() { }
  virtual ~TelephonyListener() { }

  bool Listen(bool aStart);
};

class BluetoothRilListener
{
public:
  BluetoothRilListener();
  ~BluetoothRilListener();

  /**
   * Start/Stop listening.
   *
   * @param aStart [in] whether to start/stop listening
   */
  bool Listen(bool aStart);

  /**
   * Be informed that certain client's service has changed.
   *
   * @param aClientId   [in] the client id with service change
   * @param aRegistered [in] whether changed service is registered
   */
  void ServiceChanged(uint32_t aClientId, bool aRegistered);

  /**
   * Enumerate current calls.
   */
  void EnumerateCalls();

  /**
   * The id of client that mobile connection and icc info listeners
   * are listening to.
   *
   * mClientId equals to number of total clients (array length of
   * mobile connection listeners) if there is no available client to listen.
   */
  uint32_t mClientId;

private:
  /**
   * Start/Stop listening of mobile connection and icc info.
   *
   * @param aStart [in] whether to start/stop listening
   */
  bool ListenMobileConnAndIccInfo(bool aStart);

  /**
   * Select available client to listen and assign mClientId.
   *
   * mClientId is assigned to number of total clients (array length of
   * mobile connection listeners) if there is no available client to listen.
   */
  void SelectClient();

  /**
   * Array of mobile connection listeners.
   *
   * The length equals to number of total clients.
   */
  nsTArray<nsRefPtr<MobileConnectionListener> > mMobileConnListeners;

  nsRefPtr<IccListener> mIccListener;
  nsRefPtr<TelephonyListener> mTelephonyListener;
};

END_BLUETOOTH_NAMESPACE

#endif
