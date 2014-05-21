/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__
#define mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__

/**
 * Error Messages used in Bluetooth profiles
 *
 * These error messages would be sent to Gaia as an argument of onError event.
 */
#define ERR_ALREADY_CONNECTED           "AlreadyConnectedError"
#define ERR_ALREADY_DISCONNECTED        "AlreadyDisconnectedError"
#define ERR_CONNECTION_FAILED           "ConnectionFailedError"
#define ERR_DISCONNECTION_FAILED        "DisconnectionFailedError"
#define ERR_NO_AVAILABLE_RESOURCE       "NoAvailableResourceError"
#define ERR_REACHED_CONNECTION_LIMIT    "ReachedConnectionLimitError"
#define ERR_SERVICE_CHANNEL_NOT_FOUND   "DeviceChannelRetrievalError"
#define ERR_UNKNOWN_PROFILE             "UnknownProfileError"
#define ERR_OPERATION_TIMEOUT           "OperationTimeout"

#include "BluetoothCommon.h"
#include "nsIObserver.h"

BEGIN_BLUETOOTH_NAMESPACE
class BluetoothProfileController;

class BluetoothProfileManagerBase : public nsIObserver
{
public:
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) = 0;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) = 0;

  /**
   * Returns the address of the connected device.
   */
  virtual void GetAddress(nsAString& aDeviceAddress) = 0;

  /**
   * Returns true if the profile is connected.
   */
  virtual bool IsConnected() = 0;

  /**
   * Connect to a specific remote device. When it has been done, the
   * callback "OnConnect" will be invoked.
   */
  virtual void Connect(const nsAString& aDeviceAddress,
                       BluetoothProfileController* aController) = 0;

  /**
   * Close the socket and then invoke the callback "OnDisconnect".
   */
  virtual void Disconnect(BluetoothProfileController* aController) = 0;

  /**
   * If it establishes/releases a connection successfully, the error string
   * will be empty. Otherwise, the error string shows the failure reason.
   */
  virtual void OnConnect(const nsAString& aErrorStr) = 0;
  virtual void OnDisconnect(const nsAString& aErrorStr) = 0;

  /**
   * Clean up profile resources and set mController as null.
   */
  virtual void Reset() = 0;

  /**
   * Returns string of profile name.
   */
  virtual void GetName(nsACString& aName) = 0;
};

#define BT_DECL_PROFILE_MGR_BASE                                                 \
public:                                                                          \
  NS_DECL_ISUPPORTS                                                              \
  NS_DECL_NSIOBSERVER                                                            \
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,              \
                                   const nsAString& aServiceUuid,                \
                                   int aChannel) MOZ_OVERRIDE;                   \
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) MOZ_OVERRIDE; \
  virtual void GetAddress(nsAString& aDeviceAddress) MOZ_OVERRIDE;               \
  virtual bool IsConnected() MOZ_OVERRIDE;                                       \
  virtual void Connect(const nsAString& aDeviceAddress,                          \
                       BluetoothProfileController* aController) MOZ_OVERRIDE;    \
  virtual void Disconnect(BluetoothProfileController* aController) MOZ_OVERRIDE; \
  virtual void OnConnect(const nsAString& aErrorStr) MOZ_OVERRIDE;               \
  virtual void OnDisconnect(const nsAString& AErrorStr) MOZ_OVERRIDE;            \
  virtual void Reset() MOZ_OVERRIDE;

END_BLUETOOTH_NAMESPACE

#endif  //#ifndef mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__
