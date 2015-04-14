/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocket.h"

#include "BluetoothSocketObserver.h"
#include "BluetoothUnixSocketConnector.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

BluetoothSocket::BluetoothSocket(BluetoothSocketObserver* aObserver,
                                 BluetoothSocketType aType,
                                 bool aAuth,
                                 bool aEncrypt)
  : mObserver(aObserver)
  , mType(aType)
  , mAuth(aAuth)
  , mEncrypt(aEncrypt)
{
  MOZ_ASSERT(aObserver);
}

bool
BluetoothSocket::Connect(const nsAString& aDeviceAddress,
                         const BluetoothUuid& aServiceUuid,
                         int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  nsAutoPtr<BluetoothUnixSocketConnector> c(
    new BluetoothUnixSocketConnector(mType, aChannel, mAuth, mEncrypt));

  if (!ConnectSocket(c.forget(),
                     NS_ConvertUTF16toUTF8(aDeviceAddress).BeginReading())) {
    nsAutoString addr;
    GetAddress(addr);
    BT_LOGD("%s failed. Current connected device address: %s",
           __FUNCTION__, NS_ConvertUTF16toUTF8(addr).get());
    return false;
  }

  return true;
}

bool
BluetoothSocket::Listen(const nsAString& aServiceName,
                        const BluetoothUuid& aServiceUuid,
                        int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothUnixSocketConnector> c(
    new BluetoothUnixSocketConnector(mType, aChannel, mAuth, mEncrypt));

  if (!ListenSocket(c.forget())) {
    nsAutoString addr;
    GetAddress(addr);
    BT_LOGD("%s failed. Current connected device address: %s",
           __FUNCTION__, NS_ConvertUTF16toUTF8(addr).get());
    return false;
  }

  return true;
}

void
BluetoothSocket::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->ReceiveSocketData(this, aMessage);
}

void
BluetoothSocket::OnConnectSuccess()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectSuccess(this);
}

void
BluetoothSocket::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketConnectError(this);
}

void
BluetoothSocket::OnDisconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mObserver);
  mObserver->OnSocketDisconnect(this);
}

