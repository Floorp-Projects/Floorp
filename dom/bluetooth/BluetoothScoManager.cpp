/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothScoManager.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothServiceUuid.h"

#include "mozilla/Services.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISystemMessagesInternal.h"
#include "nsVariant.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla::ipc;

static nsRefPtr<BluetoothScoManager> sInstance;
static nsCOMPtr<nsIThread> sScoCommandThread;

BluetoothScoManager::BluetoothScoManager()
{
  if (!sScoCommandThread) {
    if (NS_FAILED(NS_NewThread(getter_AddRefs(sScoCommandThread)))) {
      NS_ERROR("Failed to new thread for sScoCommandThread");
    }
  }
  mConnected = false;
}

BluetoothScoManager::~BluetoothScoManager()
{
  // Shut down the command thread if it still exists.
  if (sScoCommandThread) {
    nsCOMPtr<nsIThread> thread;
    sScoCommandThread.swap(thread);
    if (NS_FAILED(thread->Shutdown())) {
      NS_WARNING("Failed to shut down the bluetooth hfpmanager command thread!");
    }
  }
}

//static
BluetoothScoManager*
BluetoothScoManager::Get()
{
  if (sInstance == nullptr) {
    sInstance = new BluetoothScoManager();
  }

  // TODO: destroy pointer sInstance on shutdown
  return sInstance;
}

// Virtual function of class SocketConsumer
void
BluetoothScoManager::ReceiveSocketData(mozilla::ipc::UnixSocketRawData* aMessage)
{
  // SCO socket do nothing here
  MOZ_NOT_REACHED("This should never be called!");
}

bool
BluetoothScoManager::Connect(const nsAString& aDeviceObjectPath)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mConnected) {
    NS_WARNING("Sco socket has been ready");
    return true;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsresult rv = bs->GetScoSocket(aDeviceObjectPath,
                                 true,
                                 false,
                                 this);

  return NS_FAILED(rv) ? false : true;
}

void
BluetoothScoManager::Disconnect()
{
  CloseSocket();
  mConnected = false;
}

// FIXME: detect connection in UnixSocketConsumer
bool
BluetoothScoManager::GetConnected()
{
  return mConnected;
}

void
BluetoothScoManager::SetConnected(bool aConnected)
{
  mConnected = aConnected;
}
