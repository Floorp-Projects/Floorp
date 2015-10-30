/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothProfileManagerBase_h
#define mozilla_dom_bluetooth_BluetoothProfileManagerBase_h

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

class BluetoothProfileResultHandler
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothProfileResultHandler);

  virtual void OnError(nsresult aResult) { }
  virtual void Init() { }
  virtual void Deinit() { }

protected:
  virtual ~BluetoothProfileResultHandler() { }
};

class BluetoothProfileManagerBase : public nsIObserver
{
public:
  virtual void OnGetServiceChannel(const BluetoothAddress& aDeviceAddress,
                                   const BluetoothUuid& aServiceUuid,
                                   int aChannel) = 0;
  virtual void OnUpdateSdpRecords(const BluetoothAddress& aDeviceAddress) = 0;

  /**
   * Return the address of the connected device.
   */
  virtual void GetAddress(BluetoothAddress& aDeviceAddress) = 0;

  /**
   * Return true if the profile is connected.
   */
  virtual bool IsConnected() = 0;

  /**
   * Connect to a specific remote device. When it has been done, the
   * callback "OnConnect" will be invoked.
   */
  virtual void Connect(const BluetoothAddress& aDeviceAddress,
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
   * Return string of profile name.
   */
  virtual void GetName(nsACString& aName) = 0;
};

#define BT_DECL_PROFILE_MGR_BASE                                             \
public:                                                                      \
  NS_DECL_ISUPPORTS                                                          \
  NS_DECL_NSIOBSERVER                                                        \
  virtual void OnGetServiceChannel(const BluetoothAddress& aDeviceAddress,   \
                                   const BluetoothUuid& aServiceUuid,        \
                                   int aChannel) override;                   \
  virtual void OnUpdateSdpRecords(                                           \
    const BluetoothAddress& aDeviceAddress) override;                        \
  virtual void GetAddress(BluetoothAddress& aDeviceAddress) override;        \
  virtual bool IsConnected() override;                                       \
  virtual void Connect(const BluetoothAddress& aDeviceAddress,               \
                       BluetoothProfileController* aController) override;    \
  virtual void Disconnect(BluetoothProfileController* aController) override; \
  virtual void OnConnect(const nsAString& aErrorStr) override;               \
  virtual void OnDisconnect(const nsAString& aErrorStr) override;            \
  virtual void Reset() override;

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_BluetoothProfileManagerBase_h
