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
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISystemMessagesInternal.h"
#include "nsVariant.h"

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

namespace {
StaticRefPtr<BluetoothScoManager> gBluetoothScoManager;
bool gInShutdown = false;
static nsCOMPtr<nsIThread> sScoCommandThread;
} // anonymous namespace

NS_IMPL_ISUPPORTS1(BluetoothScoManager, nsIObserver)

BluetoothScoManager::BluetoothScoManager()
  : mConnected(false)
{
}

bool
BluetoothScoManager::Init()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    NS_WARNING("Failed to add shutdown observer!");
    return false;
  }

  if (!sScoCommandThread &&
      NS_FAILED(NS_NewThread(getter_AddRefs(sScoCommandThread)))) {
    NS_ERROR("Failed to new thread for sScoCommandThread");
    return false;
  }
  return true;
}

BluetoothScoManager::~BluetoothScoManager()
{
  Cleanup();
}

void
BluetoothScoManager::Cleanup()
{
  // Shut down the command thread if it still exists.
  if (sScoCommandThread) {
    nsCOMPtr<nsIThread> thread;
    sScoCommandThread.swap(thread);
    if (NS_FAILED(thread->Shutdown())) {
      NS_WARNING("Failed to shut down the bluetooth hfpmanager command thread!");
    }
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs &&
      NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    NS_WARNING("Can't unregister observers!");
  }
}

//static
BluetoothScoManager*
BluetoothScoManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (!gBluetoothScoManager) {
    return gBluetoothScoManager;
  }

  // If we already exist, exit early
  if (gBluetoothScoManager) {
    return gBluetoothScoManager;
  }

  // If we're in shutdown, don't create a new instance
  if (gInShutdown) {
    NS_WARNING("BluetoothScoManager can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  nsRefPtr<BluetoothScoManager> manager = new BluetoothScoManager();
  NS_ENSURE_TRUE(manager, nullptr);

  if (!manager->Init()) {
    manager->Cleanup();
    return nullptr;
  }

  gBluetoothScoManager = manager;
  return gBluetoothScoManager;
}

nsresult
BluetoothScoManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const PRUnichar* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothScoManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

// Virtual function of class SocketConsumer
void
BluetoothScoManager::ReceiveSocketData(mozilla::ipc::UnixSocketRawData* aMessage)
{
  // SCO socket do nothing here
  MOZ_NOT_REACHED("This should never be called!");
}

nsresult
BluetoothScoManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  gInShutdown = true;
  Cleanup();
  gBluetoothScoManager = nullptr;
  return NS_OK;
}

bool
BluetoothScoManager::Connect(const nsAString& aDeviceObjectPath)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Connect called while in shutdown!");
    return false;
  }

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
