/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothAvrcpManager.h"

#include "BluetoothCommon.h"
#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "MainThreadUtils.h"


using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothAvrcpManager> sBluetoothAvrcpManager;
  bool sInShutdown = false;
} // namespace

NS_IMETHODIMP
BluetoothAvrcpManager::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothAvrcpManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothAvrcpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothAvrcpManager::BluetoothAvrcpManager()
{
  Reset();
}

void
BluetoothAvrcpManager::Reset()
{
  mAvrcpConnected = false;
  mDuration = 0;
  mMediaNumber = 0;
  mTotalMediaCount = 0;
  mPosition = 0;
  mPlayStatus = ControlPlayStatus::PLAYSTATUS_UNKNOWN;
}

bool
BluetoothAvrcpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);
  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add shutdown observer!");
    return false;
  }

  return true;
}

BluetoothAvrcpManager::~BluetoothAvrcpManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

//static
BluetoothAvrcpManager*
BluetoothAvrcpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothA2dpManager already exists, exit early
  if (sBluetoothAvrcpManager) {
    return sBluetoothAvrcpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothAvrcpManager* manager = new BluetoothAvrcpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothAvrcpManager = manager;
  return sBluetoothAvrcpManager;
}

void
BluetoothAvrcpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothAvrcpManager = nullptr;
}

void
BluetoothAvrcpManager::Connect(const BluetoothAddress& aDeviceAddress,
                              BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsCleared());
  MOZ_ASSERT(aController && !mController);

  mDeviceAddress = aDeviceAddress;
  OnConnect(EmptyString());
}

void
BluetoothAvrcpManager::Disconnect(BluetoothProfileController* aController)
{
  mDeviceAddress.Clear();
  OnDisconnect(EmptyString());
}

void
BluetoothAvrcpManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  RefPtr<BluetoothProfileController> controller = mController.forget();
  controller->NotifyCompletion(aErrorStr);
}

void
BluetoothAvrcpManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  RefPtr<BluetoothProfileController> controller = mController.forget();
  controller->NotifyCompletion(aErrorStr);

  Reset();
}

void
BluetoothAvrcpManager::OnGetServiceChannel(
  const BluetoothAddress& aDeviceAddress,
  const BluetoothUuid& aServiceUuid,
  int aChannel)
{ }

void
BluetoothAvrcpManager::OnUpdateSdpRecords(const BluetoothAddress& aDeviceAddress)
{ }

void
BluetoothAvrcpManager::GetAddress(BluetoothAddress& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

bool
BluetoothAvrcpManager::IsConnected()
{
  return mAvrcpConnected;
}

void
BluetoothAvrcpManager::SetConnected(bool aConnected)
{
  mAvrcpConnected = aConnected;
  if (!aConnected) {
    Reset();
  }
}

void
BluetoothAvrcpManager::UpdateMetaData(const nsAString& aTitle,
                                      const nsAString& aArtist,
                                      const nsAString& aAlbum,
                                      uint64_t aMediaNumber,
                                      uint64_t aTotalMediaCount,
                                      uint32_t aDuration)
{
  mTitle.Assign(aTitle);
  mArtist.Assign(aArtist);
  mAlbum.Assign(aAlbum);
  mMediaNumber = aMediaNumber;
  mTotalMediaCount = aTotalMediaCount;
  mDuration = aDuration;
}

void
BluetoothAvrcpManager::UpdatePlayStatus(uint32_t aDuration,
                                        uint32_t aPosition,
                                        ControlPlayStatus aPlayStatus)
{
  mDuration = aDuration;
  mPosition = aPosition;
  mPlayStatus = aPlayStatus;
}

void
BluetoothAvrcpManager::GetAlbum(nsAString& aAlbum)
{
    aAlbum.Assign(mAlbum);
}

uint32_t
BluetoothAvrcpManager::GetDuration()
{
  return mDuration;
}

ControlPlayStatus
BluetoothAvrcpManager::GetPlayStatus()
{
  return mPlayStatus;
}

uint32_t
BluetoothAvrcpManager::GetPosition()
{
  return mPosition;
}

uint64_t
BluetoothAvrcpManager::GetMediaNumber()
{
  return mMediaNumber;
}

void
BluetoothAvrcpManager::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
}

NS_IMPL_ISUPPORTS(BluetoothAvrcpManager, nsIObserver)
