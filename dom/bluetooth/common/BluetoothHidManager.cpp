/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothHidManager.h"

#include "BluetoothCommon.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "MainThreadUtils.h"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHidManager> sBluetoothHidManager;
  bool sInShutdown = false;
} // namespace

NS_IMETHODIMP
BluetoothHidManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothHidManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothHidManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothHidManager::BluetoothHidManager()
{
  Reset();
}

void
BluetoothHidManager::Reset()
{
  mConnected = false;
  mController = nullptr;
}

bool
BluetoothHidManager::Init()
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

BluetoothHidManager::~BluetoothHidManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

// static
void
BluetoothHidManager::InitHidInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aRes) {
    aRes->Init();
  }
}

// static
void
BluetoothHidManager::DeinitHidInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  sBluetoothHidManager = nullptr;

  if (aRes) {
    aRes->Deinit();
  }
}

//static
BluetoothHidManager*
BluetoothHidManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (sBluetoothHidManager) {
    return sBluetoothHidManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothHidManager* manager = new BluetoothHidManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothHidManager = manager;
  return sBluetoothHidManager;
}

void
BluetoothHidManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothHidManager = nullptr;
}

void
BluetoothHidManager::Connect(const BluetoothAddress& aDeviceAddress,
                             BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsCleared());
  MOZ_ASSERT(aController && !mController);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || sInShutdown) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if (mConnected) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_ALREADY_CONNECTED));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  if (NS_FAILED(bs->SendInputMessage(deviceAddressStr,
                                     NS_LITERAL_STRING("Connect")))) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }
}

void
BluetoothHidManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  if (!mConnected) {
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_ALREADY_DISCONNECTED));
    }
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsCleared());
  MOZ_ASSERT(!mController);

  mController = aController;

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  if (NS_FAILED(bs->SendInputMessage(deviceAddressStr,
                                     NS_LITERAL_STRING("Disconnect")))) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }
}

void
BluetoothHidManager::OnConnect(const nsAString& aErrorStr)
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
BluetoothHidManager::OnDisconnect(const nsAString& aErrorStr)
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

bool
BluetoothHidManager::IsConnected()
{
  return mConnected;
}

void
BluetoothHidManager::HandleInputPropertyChanged(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSignal.value().type() ==
             BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aSignal.value().get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(arr.Length() == 1);

  const nsString& name = arr[0].name();
  const BluetoothValue& value = arr[0].value();

  if (name.EqualsLiteral("Connected")) {
    MOZ_ASSERT(value.type() == BluetoothValue::Tbool);
    MOZ_ASSERT(mConnected != value.get_bool());

    mConnected = value.get_bool();
    NotifyStatusChanged();
    if (mConnected) {
      OnConnect(EmptyString());
    } else {
      OnDisconnect(EmptyString());
    }
  }
}

void
BluetoothHidManager::NotifyStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_NAMED_LITERAL_STRING(type, BLUETOOTH_HID_STATUS_CHANGED_ID);
  InfallibleTArray<BluetoothNamedValue> parameters;

  AppendNamedValue(parameters, "connected", mConnected);
  AppendNamedValue(parameters, "address", mDeviceAddress);

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);
}

void
BluetoothHidManager::OnGetServiceChannel(
  const BluetoothAddress& aDeviceAddress,
  const BluetoothUuid& aServiceUuid,
  int aChannel)
{
  // Do nothing here as bluez acquires service channel and connects for us
}

void
BluetoothHidManager::OnUpdateSdpRecords(const BluetoothAddress& aDeviceAddress)
{
  // Do nothing here as bluez acquires service channel and connects for us
}

void
BluetoothHidManager::GetAddress(BluetoothAddress& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

NS_IMPL_ISUPPORTS(BluetoothHidManager, nsIObserver)
