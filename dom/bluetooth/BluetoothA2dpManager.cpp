/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothA2dpManager.h"

#include "BluetoothCommon.h"
#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIAudioManager.h"
#include "nsIObserverService.h"


using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothA2dpManager> sBluetoothA2dpManager;
  bool sInShutdown = false;
} // anonymous namespace

NS_IMETHODIMP
BluetoothA2dpManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
  MOZ_ASSERT(sBluetoothA2dpManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothA2dpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothA2dpManager::BluetoothA2dpManager()
  : mConnected(false)
  , mPlaying(false)
  , mSinkState(SinkState::SINK_DISCONNECTED)
{
}

bool
BluetoothA2dpManager::Init()
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

BluetoothA2dpManager::~BluetoothA2dpManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

static SinkState
StatusStringToSinkState(const nsAString& aStatus)
{
  SinkState state;
  if (aStatus.EqualsLiteral("disconnected")) {
    state = SinkState::SINK_DISCONNECTED;
  } else if (aStatus.EqualsLiteral("connecting")) {
    state = SinkState::SINK_CONNECTING;
  } else if (aStatus.EqualsLiteral("connected")) {
    state = SinkState::SINK_CONNECTED;
  } else if (aStatus.EqualsLiteral("playing")) {
    state = SinkState::SINK_PLAYING;
  } else if (aStatus.EqualsLiteral("disconnecting")) {
    state = SinkState::SINK_DISCONNECTING;
  } else {
    MOZ_ASSERT(false, "Unknown sink state");
  }
  return state;
}

//static
BluetoothA2dpManager*
BluetoothA2dpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothA2dpManager already exists, exit early
  if (sBluetoothA2dpManager) {
    return sBluetoothA2dpManager;
  }

  // If we're in shutdown, don't create a new instance
  if (sInShutdown) {
    NS_WARNING("BluetoothA2dpManager can't be created during shutdown");
    return nullptr;
  }

  // Create a new instance, register, and return
  BluetoothA2dpManager* manager = new BluetoothA2dpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothA2dpManager = manager;
  return sBluetoothA2dpManager;
}

void
BluetoothA2dpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect();
  sBluetoothA2dpManager = nullptr;
}

bool
BluetoothA2dpManager::Connect(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  if (sInShutdown) {
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
  if (!mConnected) {
    NS_WARNING("BluetoothA2dpManager has been disconnected");
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsEmpty());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->SendSinkMessage(mDeviceAddress, NS_LITERAL_STRING("Disconnect"));
}

void
BluetoothA2dpManager::HandleSinkPropertyChanged(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSignal.value().type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aSignal.value().get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(arr.Length() == 1);

  const nsString& name = arr[0].name();
  const BluetoothValue& value = arr[0].value();
  if (name.EqualsLiteral("Connected")) {
    // Indicates if a stream is setup to a A2DP sink on the remote device.
    MOZ_ASSERT(value.type() == BluetoothValue::Tbool);
    mConnected = value.get_bool();
    NotifyStatusChanged();
    NotifyAudioManager();
  } else if (name.EqualsLiteral("Playing")) {
    // Indicates if a stream is active to a A2DP sink on the remote device.
    MOZ_ASSERT(value.type() == BluetoothValue::Tbool);
    mPlaying = value.get_bool();
  } else if (name.EqualsLiteral("State")) {
    MOZ_ASSERT(value.type() == BluetoothValue::TnsString);
    HandleSinkStateChanged(StatusStringToSinkState(value.get_nsString()));
  } else {
    NS_WARNING("Unknown sink property");
  }
}

/* HandleSinkPropertyChanged update sink state in A2dp
 *
 * Possible values: "disconnected", "connecting", "connected", "playing"
 *
 * 1. "disconnected" -> "connecting"
 * Either an incoming or outgoing connection attempt ongoing
 * 2. "connecting" -> "disconnected"
 * Connection attempt failed
 * 3. "connecting" -> "connected"
 * Successfully connected
 * 4. "connected" -> "playing"
 * Audio stream active
 * 5. "playing" -> "connected"
 * Audio stream suspended
 * 6. "connected" -> "disconnected"
 *    "playing" -> "disconnected"
 * Disconnected from the remote device
 * 7. "disconnecting" -> "disconnected"
 * Disconnected from local
 */
void
BluetoothA2dpManager::HandleSinkStateChanged(SinkState aState)
{
  MOZ_ASSERT_IF(aState == SinkState::SINK_CONNECTED,
                mSinkState == SinkState::SINK_CONNECTING ||
                mSinkState == SinkState::SINK_PLAYING);
  MOZ_ASSERT_IF(aState == SinkState::SINK_PLAYING,
                mSinkState == SinkState::SINK_CONNECTED);

  if (aState == SinkState::SINK_DISCONNECTED) {
    mDeviceAddress.Truncate();
  }

  mSinkState = aState;
}

void
BluetoothA2dpManager::NotifyStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_NAMED_LITERAL_STRING(type, BLUETOOTH_A2DP_STATUS_CHANGED_ID);
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
BluetoothA2dpManager::NotifyAudioManager()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE_VOID(obs);

  nsAutoString data;
  data.AppendInt(mConnected);

  if (NS_FAILED(obs->NotifyObservers(this,
                                     BLUETOOTH_A2DP_STATUS_CHANGED_ID,
                                     data.BeginReading()))) {
    NS_WARNING("Failed to notify bluetooth-a2dp-status-changed observsers!");
  }
}

void
BluetoothA2dpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                          const nsAString& aServiceUuid,
                                          int aChannel)
{
}

void
BluetoothA2dpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
}

void
BluetoothA2dpManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

NS_IMPL_ISUPPORTS1(BluetoothA2dpManager, nsIObserver)

