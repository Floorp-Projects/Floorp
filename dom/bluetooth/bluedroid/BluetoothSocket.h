/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothSocket_h
#define mozilla_dom_bluetooth_BluetoothSocket_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/UnixSocket.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketObserver;
class DroidSocketImpl;

class BluetoothSocket : public mozilla::ipc::UnixSocketConsumer
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver,
                  BluetoothSocketType aType,
                  bool aAuth,
                  bool aEncrypt);

  /**
   * Connect to remote server as a client.
   *
   * The steps are as following:
   * 1) BluetoothSocket acquires fd from bluedroid, and creates
   *    a DroidSocketImpl to watch read/write of the fd.
   * 2) DroidSocketImpl receives first 2 messages to get socket info.
   * 3) Obex client session starts.
   */
  bool Connect(const nsAString& aDeviceAddress, int aChannel);

  /**
   * Listen to incoming connection as a server.
   *
   * The steps are as following:
   * 1) BluetoothSocket acquires fd from bluedroid, and creates
   *    a DroidSocketImpl to watch read of the fd. DroidSocketImpl
   *    receives the 1st message immediately.
   * 2) When there's incoming connection, DroidSocketImpl receives
   *    2nd message to get socket info and client fd.
   * 3) DroidSocketImpl stops watching read of original fd and
   *    starts to watch read/write of client fd.
   * 4) Obex server session starts.
   */
  bool Listen(int aChannel);

  inline void Disconnect()
  {
    CloseDroidSocket();
  }

  virtual void OnConnectSuccess() MOZ_OVERRIDE;
  virtual void OnConnectError() MOZ_OVERRIDE;
  virtual void OnDisconnect() MOZ_OVERRIDE;
  virtual void ReceiveSocketData(
    nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage) MOZ_OVERRIDE;

  inline void GetAddress(nsAString& aDeviceAddress)
  {
    aDeviceAddress = mDeviceAddress;
  }

  void CloseDroidSocket();
  bool IsWaitingForClientFd();
  bool SendDroidSocketData(mozilla::ipc::UnixSocketRawData* aData);

private:
  BluetoothSocketObserver* mObserver;
  DroidSocketImpl* mImpl;
  nsString mDeviceAddress;
  bool mAuth;
  bool mEncrypt;
  bool mIsServer;
  int mReceivedSocketInfoLength;

  bool CreateDroidSocket(int aFd);
  bool ReceiveSocketInfo(nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage);
};

END_BLUETOOTH_NAMESPACE

#endif
