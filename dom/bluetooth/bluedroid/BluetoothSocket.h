/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothSocket_h
#define mozilla_dom_bluetooth_BluetoothSocket_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/DataSocket.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketObserver;
class BluetoothSocketResultHandler;
class DroidSocketImpl;

class BluetoothSocket final : public mozilla::ipc::DataSocket
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver,
                  BluetoothSocketType aType,
                  bool aAuth,
                  bool aEncrypt);

  bool ConnectSocket(const nsAString& aDeviceAddress,
                     const BluetoothUuid& aServiceUuid,
                     int aChannel);

  bool ListenSocket(const nsAString& aServiceName,
                    const BluetoothUuid& aServiceUuid,
                    int aChannel);

  /**
   * Method to be called whenever data is received. This is only called on the
   * main thread.
   *
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(nsAutoPtr<mozilla::ipc::UnixSocketBuffer>& aBuffer);

  inline void GetAddress(nsAString& aDeviceAddress)
  {
    aDeviceAddress = mDeviceAddress;
  }

  inline void SetAddress(const nsAString& aDeviceAddress)
  {
    mDeviceAddress = aDeviceAddress;
  }

  inline void SetCurrentResultHandler(BluetoothSocketResultHandler* aRes)
  {
    mCurrentRes = aRes;
  }

  // Methods for |DataSocket|
  //

  void SendSocketData(mozilla::ipc::UnixSocketIOBuffer* aBuffer) override;

  // Methods for |SocketBase|
  //

  void CloseSocket() override;

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  BluetoothSocketObserver* mObserver;
  BluetoothSocketResultHandler* mCurrentRes;
  DroidSocketImpl* mImpl;
  nsString mDeviceAddress;
  bool mAuth;
  bool mEncrypt;
};

END_BLUETOOTH_NAMESPACE

#endif
