/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothSocket_h
#define mozilla_dom_bluetooth_BluetoothSocket_h

#include "BluetoothCommon.h"
#include "mozilla/ipc/SocketBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocketObserver;
class BluetoothSocketResultHandler;
class DroidSocketImpl;

class BluetoothSocket : public mozilla::ipc::SocketConsumerBase
{
public:
  BluetoothSocket(BluetoothSocketObserver* aObserver,
                  BluetoothSocketType aType,
                  bool aAuth,
                  bool aEncrypt);

  bool ConnectSocket(const nsAString& aDeviceAddress, int aChannel);

  bool ListenSocket(int aChannel);

  void CloseSocket();

  bool SendSocketData(mozilla::ipc::UnixSocketRawData* aData);

  virtual void OnConnectSuccess() MOZ_OVERRIDE;
  virtual void OnConnectError() MOZ_OVERRIDE;
  virtual void OnDisconnect() MOZ_OVERRIDE;
  virtual void ReceiveSocketData(
    nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage) MOZ_OVERRIDE;

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
