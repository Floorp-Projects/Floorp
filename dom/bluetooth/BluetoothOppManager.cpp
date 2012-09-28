/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothOppManager.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothServiceUuid.h"

#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/RefPtr.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla::ipc;

static mozilla::RefPtr<BluetoothOppManager> sInstance;

BluetoothOppManager::BluetoothOppManager()
{
}

BluetoothOppManager::~BluetoothOppManager()
{
}

//static
BluetoothOppManager*
BluetoothOppManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInstance == nullptr) {
    sInstance = new BluetoothOppManager();
  }

  return sInstance;
}

bool
BluetoothOppManager::Connect(const nsAString& aDeviceObjectPath,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsString serviceUuidStr =
    NS_ConvertUTF8toUTF16(mozilla::dom::bluetooth::BluetoothServiceUuidStr::ObjectPush);

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsresult rv = bs->GetSocketViaService(aDeviceObjectPath,
                                        serviceUuidStr,
                                        BluetoothSocketType::RFCOMM,
                                        true,
                                        false,
                                        this,
                                        runnable);

  runnable.forget();
  return NS_FAILED(rv) ? false : true;
}

void
BluetoothOppManager::Disconnect()
{
  CloseSocket();
}

bool
BluetoothOppManager::SendFile(BlobParent* aParent,
                              BluetoothReplyRunnable* aRunnable)
{
  // will implement in another patch.
  return true;
}

bool
BluetoothOppManager::StopSendingFile(BluetoothReplyRunnable* aRunnable)
{
  // will implement in another patch.
  return true;
}

// Virtual function of class SocketConsumer
void
BluetoothOppManager::ReceiveSocketData(UnixSocketRawData* aMessage)
{
  // will implement in another patch.
}

