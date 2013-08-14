/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHidManager> sBluetoothHidManager;
  bool sInShutdown = false;
} // anonymous namespace

NS_IMETHODIMP
BluetoothHidManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const PRUnichar* aData)
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
  : mConnected(false)
{
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
  Disconnect();
  sBluetoothHidManager = nullptr;
}

bool
BluetoothHidManager::Connect(const nsAString& aDeviceAddress,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  NS_ENSURE_FALSE(sInShutdown, false);
  NS_ENSURE_FALSE(mConnected, false);

  mDeviceAddress = aDeviceAddress;

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, false);
  nsresult rv = bs->SendInputMessage(aDeviceAddress,
                                     NS_LITERAL_STRING("Connect"),
                                     aRunnable);

  return NS_SUCCEEDED(rv);
}

void
BluetoothHidManager::Disconnect()
{
  NS_ENSURE_TRUE_VOID(mConnected);

  MOZ_ASSERT(!mDeviceAddress.IsEmpty());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->SendInputMessage(mDeviceAddress,
                       NS_LITERAL_STRING("Disconnect"),
                       nullptr);
}

bool BluetoothHidManager::IsConnected()
{
  return mConnected;
}

void BluetoothHidManager::HandleInputPropertyChanged(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSignal.value().type() == BluetoothValue::TArrayOfBluetoothNamedValue);

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
  }
}

void
BluetoothHidManager::NotifyStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_NAMED_LITERAL_STRING(type, BLUETOOTH_HID_STATUS_CHANGED_ID);
  InfallibleTArray<BluetoothNamedValue> parameters;

  BluetoothValue v = mConnected;
  parameters.AppendElement(
    BluetoothNamedValue(NS_LITERAL_STRING("connected"), v));

  v = mDeviceAddress;
  parameters.AppendElement(
    BluetoothNamedValue(NS_LITERAL_STRING("address"), v));

  if (!BroadcastSystemMessage(type, parameters)) {
    NS_WARNING("Failed to broadcast system message to settings");
    return;
  }
}

void
BluetoothHidManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  // Do nothing here as bluez acquires service channel and connects for us
}

void
BluetoothHidManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  // Do nothing here as bluez acquires service channel and connects for us
}

void
BluetoothHidManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

NS_IMPL_ISUPPORTS1(BluetoothHidManager, nsIObserver)
