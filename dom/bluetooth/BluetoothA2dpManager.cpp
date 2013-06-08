/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothA2dpManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIAudioManager.h"
#include "nsIObserverService.h"

#define BLUETOOTH_A2DP_STATUS_CHANGED "bluetooth-a2dp-status-changed"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticAutoPtr<BluetoothA2dpManager> gBluetoothA2dpManager;
  StaticRefPtr<BluetoothA2dpManagerObserver> sA2dpObserver;
  bool gInShutdown = false;
} // anonymous namespace

class mozilla::dom::bluetooth::BluetoothA2dpManagerObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  BluetoothA2dpManagerObserver()
  {
  }

  bool Init()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_ASSERT(obs);
    if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
      NS_WARNING("Failed to add shutdown observer!");
      return false;
    }

    return true;
  }

  bool Shutdown()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs ||
        NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
      NS_WARNING("Can't unregister observers, or already unregistered!");
      return false;
    }

    return true;
  }

  ~BluetoothA2dpManagerObserver()
  {
    Shutdown();
  }
};

NS_IMETHODIMP
BluetoothA2dpManagerObserver::Observe(nsISupports* aSubject,
                                      const char* aTopic,
                                      const PRUnichar* aData)
{
  MOZ_ASSERT(gBluetoothA2dpManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return gBluetoothA2dpManager->HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothA2dpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_ISUPPORTS1(BluetoothA2dpManagerObserver, nsIObserver)

BluetoothA2dpManager::BluetoothA2dpManager()
  : mSinkState(SinkState::SINK_DISCONNECTED)
  , mConnected(false)
  , mPlaying(false)
{
}

bool
BluetoothA2dpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  sA2dpObserver = new BluetoothA2dpManagerObserver();
  if (!sA2dpObserver->Init()) {
    NS_WARNING("Cannot set up A2dp Observers!");
    sA2dpObserver = nullptr;
  }

  return true;
}

BluetoothA2dpManager::~BluetoothA2dpManager()
{
  Cleanup();
}

void
BluetoothA2dpManager::Cleanup()
{
  sA2dpObserver->Shutdown();
  sA2dpObserver = nullptr;
}

//static
BluetoothA2dpManager*
BluetoothA2dpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gBluetoothA2dpManager) {
    return gBluetoothA2dpManager;
  }

  // If we're in shutdown, don't create a new instance
  if (gInShutdown) {
    NS_WARNING("BluetoothA2dpManager can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  BluetoothA2dpManager* manager = new BluetoothA2dpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  gBluetoothA2dpManager = manager;
  return gBluetoothA2dpManager;
}

nsresult
BluetoothA2dpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  gInShutdown = true;
  Disconnect();
  gBluetoothA2dpManager = nullptr;
  return NS_OK;
}

bool
BluetoothA2dpManager::Connect(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  if (gInShutdown) {
    NS_WARNING("Connect called while in shutdown!");
    return false;
  }

  if (mConnected) {
    NS_WARNING("BluetoothA2dpManager is connected");
    return false;
  }

  mDeviceAddress = aDeviceAddress;

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, false);
  nsresult rv = bs->SendSinkMessage(aDeviceAddress,
                                    NS_LITERAL_STRING("Connect"));

  return NS_SUCCEEDED(rv);
}

void
BluetoothA2dpManager::Disconnect()
{
  MOZ_ASSERT(!mDeviceAddress.IsEmpty());

  if (!mConnected) {
    NS_WARNING("BluetoothA2dpManager has been disconnected");
    return;
  }

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->SendSinkMessage(mDeviceAddress, NS_LITERAL_STRING("Disconnect"));
}
