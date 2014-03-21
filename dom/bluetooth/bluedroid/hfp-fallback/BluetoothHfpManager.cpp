/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothHfpManager.h"
#include "BluetoothProfileController.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHfpManager> sBluetoothHfpManager;
  bool sInShutdown = false;
} // anonymous namespace

/**
 * nsIObserver function
 */
NS_IMETHODIMP
BluetoothHfpManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
  } else {
    MOZ_ASSERT(false, "BluetoothHfpManager got unexpected topic!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

/**
 * BluetoothProfileManagerBase functions
 */
void
BluetoothHfpManager::Connect(const nsAString& aDeviceAddress,
                             BluetoothProfileController* aController)
{
  MOZ_ASSERT(aController);

  aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
}

void
BluetoothHfpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(aController);

  aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
}

bool
BluetoothHfpManager::IsConnected()
{
  return false;
}

void
BluetoothHfpManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothHfpManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothHfpManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
}

void
BluetoothHfpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  MOZ_ASSERT(false);
}

void
BluetoothHfpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(false);
}

/**
 * BluetoothHfpManagerBase function
 */
bool
BluetoothHfpManager::IsScoConnected()
{
  return false;
}

/**
 * Non-inherited functions
 */
// static
BluetoothHfpManager*
BluetoothHfpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothHfpManager already exists, exit early
  if (sBluetoothHfpManager) {
    return sBluetoothHfpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance and return
  BluetoothHfpManager* manager = new BluetoothHfpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothHfpManager = manager;
  return sBluetoothHfpManager;
}

bool
BluetoothHfpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add observers!");
    return false;
  }

  return true;
}

void
BluetoothHfpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  sBluetoothHfpManager = nullptr;
}

bool
BluetoothHfpManager::ConnectSco()
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * TODO:
   *   Implement ConnectSco() for applications that want to create SCO link
   *   without a HFP connection (e.g., VoIP).
   */
  return false;
}

bool
BluetoothHfpManager::DisconnectSco()
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * TODO:
   *   Implement DisconnectSco() for applications that want to destroy SCO link
   *   without a HFP connection (e.g., VoIP).
   */
  return false;
}

void
BluetoothHfpManager::Reset()
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS1(BluetoothHfpManager, nsIObserver)
