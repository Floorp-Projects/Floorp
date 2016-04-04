/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothSocket_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothSocket_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/DataSocket.h"

class MessageLoop;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketInterface;
class BluetoothSocketObserver;
class BluetoothSocketResultHandler;
class DroidSocketImpl;

class BluetoothSocket final : public mozilla::ipc::DataSocket
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver);
  ~BluetoothSocket();

  void SetObserver(BluetoothSocketObserver* aObserver);

  nsresult Connect(const BluetoothAddress& aDeviceAddress,
                   const BluetoothUuid& aServiceUuid,
                   BluetoothSocketType aType,
                   int aChannel,
                   bool aAuth, bool aEncrypt,
                   MessageLoop* aConsumerLoop,
                   MessageLoop* aIOLoop);

  nsresult Connect(const BluetoothAddress& aDeviceAddress,
                   const BluetoothUuid& aServiceUuid,
                   BluetoothSocketType aType,
                   int aChannel,
                   bool aAuth, bool aEncrypt);

  nsresult Listen(const nsAString& aServiceName,
                  const BluetoothUuid& aServiceUuid,
                  BluetoothSocketType aType,
                  int aChannel,
                  bool aAuth, bool aEncrypt,
                  MessageLoop* aConsumerLoop,
                  MessageLoop* aIOLoop);

  nsresult Listen(const nsAString& aServiceName,
                  const BluetoothUuid& aServiceUuid,
                  BluetoothSocketType aType,
                  int aChannel,
                  bool aAuth, bool aEncrypt);

  nsresult Accept(int aListenFd, BluetoothSocketResultHandler* aRes);

  /**
   * Method to be called whenever data is received. This is only called on the
   * consumer thread.
   *
   * @param aBuffer Data received from the socket.
   */
  void ReceiveSocketData(UniquePtr<mozilla::ipc::UnixSocketBuffer>& aBuffer);

  inline void GetAddress(BluetoothAddress& aDeviceAddress)
  {
    aDeviceAddress = mDeviceAddress;
  }

  inline void SetAddress(const BluetoothAddress& aDeviceAddress)
  {
    mDeviceAddress = aDeviceAddress;
  }

  // Methods for |DataSocket|
  //

  void SendSocketData(mozilla::ipc::UnixSocketIOBuffer* aBuffer) override;

  // Methods for |SocketBase|
  //

  void Close() override;

  void OnConnectSuccess() override;
  void OnConnectError() override;
  void OnDisconnect() override;

private:
  nsresult LoadSocketInterface();
  void Cleanup();

  inline void SetCurrentResultHandler(BluetoothSocketResultHandler* aRes)
  {
    mCurrentRes = aRes;
  }

  BluetoothSocketInterface* mSocketInterface;
  BluetoothSocketObserver* mObserver;
  BluetoothSocketResultHandler* mCurrentRes;
  DroidSocketImpl* mImpl;
  BluetoothAddress mDeviceAddress;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothSocket_h
