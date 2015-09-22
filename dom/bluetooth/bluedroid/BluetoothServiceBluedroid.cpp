/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "BluetoothServiceBluedroid.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothAvrcpManager.h"
#include "BluetoothGattManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothMapSmsManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothPbapManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"

#define ENSURE_BLUETOOTH_IS_READY(runnable, result)                    \
  do {                                                                 \
    if (!sBtInterface || !IsEnabled()) {                               \
      DispatchReplyError(runnable,                                     \
        NS_LITERAL_STRING("Bluetooth is not ready"));                  \
      return result;                                                   \
    }                                                                  \
  } while(0)

#define ENSURE_BLUETOOTH_IS_READY_VOID(runnable)                       \
  do {                                                                 \
    if (!sBtInterface || !IsEnabled()) {                               \
      DispatchReplyError(runnable,                                     \
        NS_LITERAL_STRING("Bluetooth is not ready"));                  \
      return;                                                          \
    }                                                                  \
  } while(0)

#define ENSURE_GATT_MGR_IS_READY_VOID(gatt, runnable)                  \
  do {                                                                 \
    if (!gatt) {                                                       \
      DispatchReplyError(runnable,                                     \
        NS_LITERAL_STRING("GattManager is not ready"));                \
      return;                                                          \
    }                                                                  \
  } while(0)

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

static BluetoothInterface* sBtInterface;
static nsTArray<nsRefPtr<BluetoothProfileController> > sControllerArray;

/*
 *  Static methods
 */

ControlPlayStatus
BluetoothServiceBluedroid::PlayStatusStringToControlPlayStatus(
  const nsAString& aPlayStatus)
{
  ControlPlayStatus playStatus = ControlPlayStatus::PLAYSTATUS_UNKNOWN;
  if (aPlayStatus.EqualsLiteral("STOPPED")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_STOPPED;
  } else if (aPlayStatus.EqualsLiteral("PLAYING")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_PLAYING;
  } else if (aPlayStatus.EqualsLiteral("PAUSED")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_PAUSED;
  } else if (aPlayStatus.EqualsLiteral("FWD_SEEK")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_FWD_SEEK;
  } else if (aPlayStatus.EqualsLiteral("REV_SEEK")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_REV_SEEK;
  } else if (aPlayStatus.EqualsLiteral("ERROR")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_ERROR;
  }

  return playStatus;
}

class BluetoothServiceBluedroid::EnableResultHandler final
  : public BluetoothResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Enable failed: %d", aStatus);

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

/* |ProfileInitResultHandler| collects the results of all profile
 * result handlers and calls |Proceed| after all results handlers
 * have been run.
 */
class BluetoothServiceBluedroid::ProfileInitResultHandler final
  : public BluetoothProfileResultHandler
{
public:
  ProfileInitResultHandler(unsigned char aNumProfiles)
    : mNumProfiles(aNumProfiles)
  {
    MOZ_ASSERT(mNumProfiles);
  }

  void Init() override
  {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

  void OnError(nsresult aResult) override
  {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

private:
  void Proceed() const
  {
    sBtInterface->Enable(new EnableResultHandler());
  }

  unsigned char mNumProfiles;
};

class BluetoothServiceBluedroid::InitResultHandler final
  : public BluetoothResultHandler
{
public:
  void Init() override
  {
    static void (* const sInitManager[])(BluetoothProfileResultHandler*) = {
      BluetoothHfpManager::InitHfpInterface,
      BluetoothA2dpManager::InitA2dpInterface,
      BluetoothAvrcpManager::InitAvrcpInterface,
      BluetoothGattManager::InitGattInterface
    };

    MOZ_ASSERT(NS_IsMainThread());

    // Register all the bluedroid callbacks before enable() gets called. This is
    // required to register a2dp callbacks before a2dp media task starts up.
    // If any interface cannot be initialized, turn on bluetooth core anyway.
    nsRefPtr<ProfileInitResultHandler> res =
      new ProfileInitResultHandler(MOZ_ARRAY_LENGTH(sInitManager));

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sInitManager); ++i) {
      sInitManager[i](res);
    }
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Init failed: %d", aStatus);

    sBtInterface = nullptr;

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

nsresult
BluetoothServiceBluedroid::StartGonkBluetooth()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  if (bs->IsEnabled()) {
    // Keep current enable status
    BluetoothService::AcknowledgeToggleBt(true);
    return NS_OK;
  }

  sBtInterface->Init(reinterpret_cast<BluetoothServiceBluedroid*>(bs),
                     new InitResultHandler());

  return NS_OK;
}

class BluetoothServiceBluedroid::DisableResultHandler final
  : public BluetoothResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Disable failed: %d", aStatus);

    // Always make progress; even on failures
    BluetoothService::AcknowledgeToggleBt(false);
  }
};

nsresult
BluetoothServiceBluedroid::StopGonkBluetooth()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  if (!bs->IsEnabled()) {
    // Keep current enable status
    BluetoothService::AcknowledgeToggleBt(false);
    return NS_OK;
  }

  sBtInterface->Disable(new DisableResultHandler());

  return NS_OK;
}

/*
 *  Member functions
 */

BluetoothServiceBluedroid::BluetoothServiceBluedroid()
  : mEnabled(false)
  , mDiscoverable(false)
  , mDiscovering(false)
  , mIsRestart(false)
  , mIsFirstTimeToggleOffBt(false)
{
  sBtInterface = BluetoothInterface::GetInstance();
  if (!sBtInterface) {
    BT_LOGR("Error! Failed to get instance of bluetooth interface");
    return;
  }
}

BluetoothServiceBluedroid::~BluetoothServiceBluedroid()
{
}

nsresult
BluetoothServiceBluedroid::StartInternal(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // aRunnable will be a nullptr while startup
  if (aRunnable) {
    mChangeAdapterStateRunnables.AppendElement(aRunnable);
  }

  nsresult ret = StartGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(false);

    // Reject Promise
    if (aRunnable) {
      DispatchReplyError(aRunnable, NS_LITERAL_STRING("StartBluetoothError"));
      mChangeAdapterStateRunnables.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
}

nsresult
BluetoothServiceBluedroid::StopInternal(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  static BluetoothProfileManagerBase* sProfiles[] = {
    BluetoothHfpManager::Get(),
    BluetoothAvrcpManager::Get(),
    BluetoothA2dpManager::Get(),
    BluetoothOppManager::Get(),
    BluetoothPbapManager::Get(),
    BluetoothMapSmsManager::Get(),
    BluetoothHidManager::Get()
  };

  // Disconnect all connected profiles
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sProfiles); i++) {
    nsCString profileName;
    sProfiles[i]->GetName(profileName);

    if (NS_WARN_IF(!sProfiles[i])) {
      BT_LOGR("Profile manager [%s] is null", profileName.get());
      return NS_ERROR_FAILURE;
    }

    if (sProfiles[i]->IsConnected()) {
      sProfiles[i]->Disconnect(nullptr);
    } else if (!profileName.EqualsLiteral("OPP") &&
               !profileName.EqualsLiteral("PBAP") &&
               !profileName.EqualsLiteral("MapSms")) {
      sProfiles[i]->Reset();
    }
  }

  // aRunnable will be a nullptr during starup and shutdown
  if (aRunnable) {
    mChangeAdapterStateRunnables.AppendElement(aRunnable);
  }

  nsresult ret = StopGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(true);

    // Reject Promise
    if (aRunnable) {
      DispatchReplyError(aRunnable, NS_LITERAL_STRING("StopBluetoothError"));
      mChangeAdapterStateRunnables.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
}

//
// GATT Client
//

void
BluetoothServiceBluedroid::StartLeScanInternal(
  const nsTArray<nsString>& aServiceUuids,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->StartLeScan(aServiceUuids, aRunnable);
}

void
BluetoothServiceBluedroid::StopLeScanInternal(
  const nsAString& aScanUuid,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->StopLeScan(aScanUuid, aRunnable);
}

void
BluetoothServiceBluedroid::ConnectGattClientInternal(
  const nsAString& aAppUuid, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Connect(aAppUuid, aDeviceAddress, aRunnable);
}

void
BluetoothServiceBluedroid::DisconnectGattClientInternal(
  const nsAString& aAppUuid, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Disconnect(aAppUuid, aDeviceAddress, aRunnable);
}

void
BluetoothServiceBluedroid::DiscoverGattServicesInternal(
  const nsAString& aAppUuid, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->Discover(aAppUuid, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientStartNotificationsInternal(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->RegisterNotifications(aAppUuid, aServId, aCharId, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientStopNotificationsInternal(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->DeregisterNotifications(aAppUuid, aServId, aCharId, aRunnable);
}

void
BluetoothServiceBluedroid::UnregisterGattClientInternal(
  int aClientIf, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->UnregisterClient(aClientIf, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientReadRemoteRssiInternal(
  int aClientIf, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadRemoteRssi(aClientIf, aDeviceAddress, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientReadCharacteristicValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadCharacteristicValue(aAppUuid, aServiceId, aCharacteristicId,
                                aRunnable);
}

void
BluetoothServiceBluedroid::GattClientWriteCharacteristicValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattWriteType& aWriteType,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->WriteCharacteristicValue(aAppUuid, aServiceId, aCharacteristicId,
                                 aWriteType, aValue, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientReadDescriptorValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ReadDescriptorValue(aAppUuid, aServiceId, aCharacteristicId,
                            aDescriptorId, aRunnable);
}

void
BluetoothServiceBluedroid::GattClientWriteDescriptorValueInternal(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->WriteDescriptorValue(aAppUuid, aServiceId, aCharacteristicId,
                             aDescriptorId, aValue, aRunnable);
}

// GATT Server
void
BluetoothServiceBluedroid::GattServerConnectPeripheralInternal(
  const nsAString& aAppUuid, const nsAString& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->ConnectPeripheral(aAppUuid, aAddress, aRunnable);
}

void
BluetoothServiceBluedroid::GattServerDisconnectPeripheralInternal(
  const nsAString& aAppUuid, const nsAString& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->DisconnectPeripheral(aAppUuid, aAddress, aRunnable);
}

void
BluetoothServiceBluedroid::UnregisterGattServerInternal(
  int aServerIf, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  BluetoothGattManager* gatt = BluetoothGattManager::Get();
  ENSURE_GATT_MGR_IS_READY_VOID(gatt, aRunnable);

  gatt->UnregisterServer(aServerIf, aRunnable);
}

nsresult
BluetoothServiceBluedroid::GetAdaptersInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * Wrap BluetoothValue =
   *   BluetoothNamedValue[]
   *     |
   *     |__ BluetoothNamedValue =
   *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
   *     |
   *     |__ BluetoothNamedValue =
   *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
   *     ...
   */
  InfallibleTArray<BluetoothNamedValue> adaptersProperties;
  uint32_t numAdapters = 1; // Bluedroid supports single adapter only

  for (uint32_t i = 0; i < numAdapters; i++) {
    InfallibleTArray<BluetoothNamedValue> properties;

    AppendNamedValue(properties, "State", mEnabled);
    AppendNamedValue(properties, "Address", mBdAddress);
    AppendNamedValue(properties, "Name", mBdName);
    AppendNamedValue(properties, "Discoverable", mDiscoverable);
    AppendNamedValue(properties, "Discovering", mDiscovering);
    AppendNamedValue(properties, "PairedDevices", mBondedAddresses);

    AppendNamedValue(adaptersProperties, "Adapter",
                     BluetoothValue(properties));
  }

  DispatchReplySuccess(aRunnable, adaptersProperties);
  return NS_OK;
}

class BluetoothServiceBluedroid::GetDeviceRequest final
{
public:
  GetDeviceRequest(int aDeviceCount, BluetoothReplyRunnable* aRunnable)
    : mDeviceCount(aDeviceCount)
    , mRunnable(aRunnable)
  { }

  int mDeviceCount;
  InfallibleTArray<BluetoothNamedValue> mDevicesPack;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

class BluetoothServiceBluedroid::GetRemoteDevicePropertiesResultHandler
  final
  : public BluetoothResultHandler
{
public:
  GetRemoteDevicePropertiesResultHandler(
    nsTArray<GetDeviceRequest>& aRequests,
    const nsAString& aDeviceAddress)
    : mRequests(aRequests)
    , mDeviceAddress(aDeviceAddress)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mRequests.IsEmpty());

    BT_WARNING("GetRemoteDeviceProperties(%s) failed: %d",
               NS_ConvertUTF16toUTF8(mDeviceAddress).get(), aStatus);

    /* Dispatch result after the final pending operation */
    if (--mRequests[0].mDeviceCount == 0) {
      if (mRequests[0].mRunnable) {
        DispatchReplySuccess(mRequests[0].mRunnable,
                             mRequests[0].mDevicesPack);
      }
      mRequests.RemoveElementAt(0);
    }
  }

private:
  nsTArray<GetDeviceRequest>& mRequests;
  nsString mDeviceAddress;
};

nsresult
BluetoothServiceBluedroid::GetConnectedDevicePropertiesInternal(
  uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  BluetoothProfileManagerBase* profile =
    BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
    return NS_OK;
  }

  // Reply success if no device of this profile is connected
  if (!profile->IsConnected()) {
    DispatchReplySuccess(aRunnable, InfallibleTArray<BluetoothNamedValue>());
    return NS_OK;
  }

  // Get address of the connected device
  nsString address;
  profile->GetAddress(address);

  // Append request of the connected device
  GetDeviceRequest request(1, aRunnable);
  mGetDeviceRequests.AppendElement(request);

  sBtInterface->GetRemoteDeviceProperties(address,
    new GetRemoteDevicePropertiesResultHandler(mGetDeviceRequests, address));

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetPairedDevicePropertiesInternal(
  const nsTArray<nsString>& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  if (aDeviceAddress.IsEmpty()) {
    DispatchReplySuccess(aRunnable);
    return NS_OK;
  }

  // Append request of all paired devices
  GetDeviceRequest request(aDeviceAddress.Length(), aRunnable);
  mGetDeviceRequests.AppendElement(request);

  for (uint8_t i = 0; i < aDeviceAddress.Length(); i++) {
    // Retrieve all properties of devices
    sBtInterface->GetRemoteDeviceProperties(aDeviceAddress[i],
      new GetRemoteDevicePropertiesResultHandler(mGetDeviceRequests,
                                                 aDeviceAddress[i]));
  }

  return NS_OK;
}

class BluetoothServiceBluedroid::DispatchReplyErrorResultHandler final
  : public BluetoothResultHandler
{
public:
  DispatchReplyErrorResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
    : mRunnableArray(aRunnableArray)
    , mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mRunnableArray.RemoveElement(mRunnable);
    if (mRunnable) {
      DispatchReplyError(mRunnable, aStatus);
    }
  }

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>>& mRunnableArray;
  BluetoothReplyRunnable* mRunnable;
};

void
BluetoothServiceBluedroid::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtInterface->StartDiscovery(
    new DispatchReplyErrorResultHandler(mChangeDiscoveryRunnables, aRunnable));
}

nsresult
BluetoothServiceBluedroid::FetchUuidsInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  /*
   * get_remote_services request will not be performed by bluedroid
   * if it is currently discovering nearby remote devices.
   */
  if (mDiscovering) {
    StopDiscoveryInternal(aRunnable);
  }

  mFetchUuidsRunnables.AppendElement(aRunnable);
  sBtInterface->GetRemoteServices(aDeviceAddress,
    new DispatchReplyErrorResultHandler(mFetchUuidsRunnables, aRunnable));

  return NS_OK;
}

void
BluetoothServiceBluedroid::StopDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtInterface->CancelDiscovery(
    new DispatchReplyErrorResultHandler(mChangeDiscoveryRunnables, aRunnable));
}

nsresult
BluetoothServiceBluedroid::SetProperty(BluetoothObjectType aType,
                                       const BluetoothNamedValue& aValue,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mSetAdapterPropertyRunnables.AppendElement(aRunnable);
  sBtInterface->SetAdapterProperty(aValue,
    new DispatchReplyErrorResultHandler(mSetAdapterPropertyRunnables,
                                        aRunnable));

  return NS_OK;
}

struct BluetoothServiceBluedroid::GetRemoteServiceRecordRequest final
{
  GetRemoteServiceRecordRequest(const nsAString& aDeviceAddress,
                                const BluetoothUuid& aUuid,
                                BluetoothProfileManagerBase* aManager)
    : mDeviceAddress(aDeviceAddress)
    , mUuid(aUuid)
    , mManager(aManager)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mManager);
  }

  nsString mDeviceAddress;
  BluetoothUuid mUuid;
  BluetoothProfileManagerBase* mManager;
};

class BluetoothServiceBluedroid::GetRemoteServiceRecordResultHandler final
  : public BluetoothResultHandler
{
public:
  GetRemoteServiceRecordResultHandler(
    nsTArray<GetRemoteServiceRecordRequest>& aGetRemoteServiceRecordArray,
    const nsAString& aDeviceAddress,
    const BluetoothUuid& aUuid)
    : mGetRemoteServiceRecordArray(aGetRemoteServiceRecordArray)
    , mDeviceAddress(aDeviceAddress)
    , mUuid(aUuid)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  }

  void OnError(BluetoothStatus aStatus) override
  {
    // Find call in array

    ssize_t i = FindRequest();

    if (i == -1) {
      BT_WARNING("No GetRemoteService request found");
      return;
    }

    // Signal error to profile manager

    nsAutoString uuidStr;
    UuidToString(mUuid, uuidStr);
    mGetRemoteServiceRecordArray[i].mManager->OnGetServiceChannel(
      mDeviceAddress, uuidStr, -1);
    mGetRemoteServiceRecordArray.RemoveElementAt(i);
  }

  void CancelDiscovery() override
  {
    // Disabled discovery mode, now perform SDP operation.
    sBtInterface->GetRemoteServiceRecord(mDeviceAddress, mUuid, this);
  }

private:
  ssize_t FindRequest() const
  {
    for (size_t i = 0; i < mGetRemoteServiceRecordArray.Length(); ++i) {
      if ((mGetRemoteServiceRecordArray[i].mDeviceAddress == mDeviceAddress) &&
          (mGetRemoteServiceRecordArray[i].mUuid == mUuid)) {
        return i;
      }
    }

    return -1;
  }

  nsTArray<GetRemoteServiceRecordRequest>& mGetRemoteServiceRecordArray;
  nsString mDeviceAddress;
  BluetoothUuid mUuid;
};

nsresult
BluetoothServiceBluedroid::GetServiceChannel(
  const nsAString& aDeviceAddress,
  const nsAString& aServiceUuid,
  BluetoothProfileManagerBase* aManager)
{
  BluetoothUuid uuid;
  StringToUuid(aServiceUuid, uuid);
  mGetRemoteServiceRecordArray.AppendElement(
    GetRemoteServiceRecordRequest(aDeviceAddress, uuid, aManager));

  nsRefPtr<BluetoothResultHandler> res =
    new GetRemoteServiceRecordResultHandler(mGetRemoteServiceRecordArray,
                                            aDeviceAddress, uuid);

  /* Stop discovery of remote devices here, because SDP operations
   * won't be performed while the adapter is in discovery mode.
   */
  if (mDiscovering) {
    sBtInterface->CancelDiscovery(res);
  } else {
    sBtInterface->GetRemoteServiceRecord(
      aDeviceAddress, uuid, res);
  }

  return NS_OK;
}

struct BluetoothServiceBluedroid::GetRemoteServicesRequest final
{
  GetRemoteServicesRequest(const nsAString& aDeviceAddress,
                           BluetoothProfileManagerBase* aManager)
    : mDeviceAddress(aDeviceAddress)
    , mManager(aManager)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mManager);
  }

  const nsString mDeviceAddress;
  BluetoothProfileManagerBase* mManager;
};

class BluetoothServiceBluedroid::GetRemoteServicesResultHandler final
  : public BluetoothResultHandler
{
public:
  GetRemoteServicesResultHandler(
    nsTArray<GetRemoteServicesRequest>& aGetRemoteServicesArray,
    const nsAString& aDeviceAddress,
    BluetoothProfileManagerBase* aManager)
    : mGetRemoteServicesArray(aGetRemoteServicesArray)
    , mDeviceAddress(aDeviceAddress)
    , mManager(aManager)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    // Find call in array

    ssize_t i = FindRequest();

    if (i == -1) {
      BT_WARNING("No GetRemoteServices request found");
      return;
    }

    // Cleanup array
    mGetRemoteServicesArray.RemoveElementAt(i);

    // There's no error-signaling mechanism; just call manager
    mManager->OnUpdateSdpRecords(mDeviceAddress);
  }

  void CancelDiscovery() override
  {
    // Disabled discovery mode, now perform SDP operation.
    sBtInterface->GetRemoteServices(mDeviceAddress, this);
  }

private:
  ssize_t FindRequest() const
  {
    for (size_t i = 0; i < mGetRemoteServicesArray.Length(); ++i) {
      if ((mGetRemoteServicesArray[i].mDeviceAddress == mDeviceAddress) &&
          (mGetRemoteServicesArray[i].mManager == mManager)) {
        return i;
      }
    }

    return -1;
  }

  nsTArray<GetRemoteServicesRequest>& mGetRemoteServicesArray;
  const nsString mDeviceAddress;
  BluetoothProfileManagerBase* mManager;
};

bool
BluetoothServiceBluedroid::UpdateSdpRecords(
  const nsAString& aDeviceAddress,
  BluetoothProfileManagerBase* aManager)
{
  mGetRemoteServicesArray.AppendElement(
    GetRemoteServicesRequest(aDeviceAddress, aManager));

  nsRefPtr<BluetoothResultHandler> res =
    new GetRemoteServicesResultHandler(mGetRemoteServicesArray,
                                       aDeviceAddress, aManager);

  /* Stop discovery of remote devices here, because SDP operations
   * won't be performed while the adapter is in discovery mode.
   */
  if (mDiscovering) {
    sBtInterface->CancelDiscovery(res);
  } else {
    sBtInterface->GetRemoteServices(aDeviceAddress, res);
  }

  return true;
}

nsresult
BluetoothServiceBluedroid::CreatePairedDeviceInternal(
  const nsAString& aDeviceAddress, int aTimeout,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mCreateBondRunnables.AppendElement(aRunnable);
  sBtInterface->CreateBond(aDeviceAddress, TRANSPORT_AUTO,
    new DispatchReplyErrorResultHandler(mCreateBondRunnables, aRunnable));

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::RemoveDeviceInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mRemoveBondRunnables.AppendElement(aRunnable);
  sBtInterface->RemoveBond(aDeviceAddress,
    new DispatchReplyErrorResultHandler(mRemoveBondRunnables, aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::PinReplyResultHandler final
  : public BluetoothResultHandler
{
public:
  PinReplyResultHandler(BluetoothReplyRunnable* aRunnable)
    : mRunnable(aRunnable)
  { }

  void PinReply() override
  {
    DispatchReplySuccess(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

void
BluetoothServiceBluedroid::PinReplyInternal(
  const nsAString& aDeviceAddress, bool aAccept,
  const nsAString& aPinCode, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  sBtInterface->PinReply(aDeviceAddress, aAccept, aPinCode,
                         new PinReplyResultHandler(aRunnable));
}

void
BluetoothServiceBluedroid::SetPinCodeInternal(
  const nsAString& aDeviceAddress, const nsAString& aPinCode,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by BlueZ only.
}

void
BluetoothServiceBluedroid::SetPasskeyInternal(
  const nsAString& aDeviceAddress, uint32_t aPasskey,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by BlueZ only.
}

class BluetoothServiceBluedroid::SspReplyResultHandler final
  : public BluetoothResultHandler
{
public:
  SspReplyResultHandler(BluetoothReplyRunnable* aRunnable)
    : mRunnable(aRunnable)
  { }

  void SspReply() override
  {
    DispatchReplySuccess(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

void
BluetoothServiceBluedroid::SspReplyInternal(
  const nsAString& aDeviceAddress, BluetoothSspVariant aVariant,
  bool aAccept, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

  sBtInterface->SspReply(aDeviceAddress, aVariant, aAccept, 0 /* passkey */,
                         new SspReplyResultHandler(aRunnable));
}

void
BluetoothServiceBluedroid::SetPairingConfirmationInternal(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by BlueZ only.
}

void
BluetoothServiceBluedroid::NextBluetoothProfileController()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Remove the completed task at the head
  NS_ENSURE_FALSE_VOID(sControllerArray.IsEmpty());
  sControllerArray.RemoveElementAt(0);

  // Start the next task if task array is not empty
  if (!sControllerArray.IsEmpty()) {
    sControllerArray[0]->StartSession();
  }
}

void
BluetoothServiceBluedroid::ConnectDisconnect(
  bool aConnect, const nsAString& aDeviceAddress,
  BluetoothReplyRunnable* aRunnable,
  uint16_t aServiceUuid, uint32_t aCod)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  BluetoothProfileController* controller =
    new BluetoothProfileController(aConnect, aDeviceAddress, aRunnable,
                                   NextBluetoothProfileController,
                                   aServiceUuid, aCod);
  sControllerArray.AppendElement(controller);

  /**
   * If the request is the first element of the queue, start from here. Note
   * that other requests are pushed into the queue and popped out after the
   * first one is completed. See NextBluetoothProfileController() for details.
   */
  if (sControllerArray.Length() == 1) {
    sControllerArray[0]->StartSession();
  }
}

void
BluetoothServiceBluedroid::Connect(const nsAString& aDeviceAddress,
                                   uint32_t aCod,
                                   uint16_t aServiceUuid,
                                   BluetoothReplyRunnable* aRunnable)
{
  ConnectDisconnect(true, aDeviceAddress, aRunnable, aServiceUuid, aCod);
}

void
BluetoothServiceBluedroid::Disconnect(
  const nsAString& aDeviceAddress, uint16_t aServiceUuid,
  BluetoothReplyRunnable* aRunnable)
{
  ConnectDisconnect(false, aDeviceAddress, aRunnable, aServiceUuid);
}

void
BluetoothServiceBluedroid::SendFile(const nsAString& aDeviceAddress,
                                    BlobParent* aBlobParent,
                                    BlobChild* aBlobChild,
                                    BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  if (!opp || !opp->SendFile(aDeviceAddress, aBlobParent)) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("SendFile failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::SendFile(const nsAString& aDeviceAddress,
                                    Blob* aBlob,
                                    BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  if (!opp || !opp->SendFile(aDeviceAddress, aBlob)) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("SendFile failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::StopSendingFile(const nsAString& aDeviceAddress,
                                           BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->StopSendingFile()) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("StopSendingFile failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ConfirmReceivingFile(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.

  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->ConfirmReceivingFile(aConfirm)) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("ConfirmReceivingFile failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ConnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->ConnectSco()) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("ConnectSco failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::DisconnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->DisconnectSco()) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("DisconnectSco failed"));
    return;
  }

  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::IsScoConnected(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp) {
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("IsScoConnected failed"));
    return;
  }

  DispatchReplySuccess(aRunnable, BluetoothValue(hfp->IsScoConnected()));
}

void
BluetoothServiceBluedroid::ReplyTovCardPulling(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to vCardPulling failed"));
    return;
  }

  pbap->ReplyToPullvCardEntry(aBlobParent);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ReplyTovCardPulling(
  Blob* aBlob,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to vCardPulling failed"));
    return;
  }

  pbap->ReplyToPullvCardEntry(aBlob);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ReplyToPhonebookPulling(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to Phonebook Pulling failed"));
    return;
  }

  pbap->ReplyToPullPhonebook(aBlobParent, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ReplyToPhonebookPulling(
  Blob* aBlob,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to Phonebook Pulling failed"));
    return;
  }

  pbap->ReplyToPullPhonebook(aBlob, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ReplyTovCardListing(
  BlobParent* aBlobParent,
  BlobChild* aBlobChild,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to vCard Listing failed"));
    return;
  }

  pbap->ReplyToPullvCardListing(aBlobParent, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::ReplyTovCardListing(
  Blob* aBlob,
  uint16_t aPhonebookSize,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
  if (!pbap) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Reply to vCard Listing failed"));
    return;
  }

  pbap->ReplyToPullvCardListing(aBlob, aPhonebookSize);
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::SendMetaData(const nsAString& aTitle,
                                        const nsAString& aArtist,
                                        const nsAString& aAlbum,
                                        int64_t aMediaNumber,
                                        int64_t aTotalMediaCount,
                                        int64_t aDuration,
                                        BluetoothReplyRunnable* aRunnable)
{
  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  if (avrcp) {
    avrcp->UpdateMetaData(aTitle, aArtist, aAlbum, aMediaNumber,
                          aTotalMediaCount, aDuration);
  }
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::SendPlayStatus(
  int64_t aDuration, int64_t aPosition,
  const nsAString& aPlayStatus,
  BluetoothReplyRunnable* aRunnable)
{
  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  if (avrcp) {
    ControlPlayStatus playStatus =
      PlayStatusStringToControlPlayStatus(aPlayStatus);
    avrcp->UpdatePlayStatus(aDuration, aPosition, playStatus);
  }
  DispatchReplySuccess(aRunnable);
}

void
BluetoothServiceBluedroid::UpdatePlayStatus(
  uint32_t aDuration, uint32_t aPosition, ControlPlayStatus aPlayStatus)
{
  // We don't need this function for bluedroid.
  // In bluez, it only calls dbus api
  // But it does not update BluetoothA2dpManager member fields
  MOZ_ASSERT(false);
}

nsresult
BluetoothServiceBluedroid::SendSinkMessage(const nsAString& aDeviceAddresses,
                                           const nsAString& aMessage)
{
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::SendInputMessage(const nsAString& aDeviceAddresses,
                                            const nsAString& aMessage)
{
  return NS_OK;
}

void
BluetoothServiceBluedroid::AnswerWaitingCall(BluetoothReplyRunnable* aRunnable)
{
}

void
BluetoothServiceBluedroid::IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable)
{
}

void
BluetoothServiceBluedroid::ToggleCalls(BluetoothReplyRunnable* aRunnable)
{
}

//
// Bluetooth notifications
//

class BluetoothServiceBluedroid::CleanupResultHandler final
  : public BluetoothResultHandler
{
public:
  void Cleanup() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService::AcknowledgeToggleBt(false);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_LOGR("BluetoothInterface::Cleanup failed: %d", aStatus);

    BluetoothService::AcknowledgeToggleBt(false);
  }
};

/* |ProfileDeinitResultHandler| collects the results of all profile
 * result handlers and cleans up the Bluedroid driver after all handlers
 * have been run.
 */
class BluetoothServiceBluedroid::ProfileDeinitResultHandler final
  : public BluetoothProfileResultHandler
{
public:
  ProfileDeinitResultHandler(unsigned char aNumProfiles, bool aIsRestart)
    : mNumProfiles(aNumProfiles)
    , mIsRestart(aIsRestart)
  {
    MOZ_ASSERT(mNumProfiles);
  }

  void Deinit() override
  {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

  void OnError(nsresult aResult) override
  {
    if (!(--mNumProfiles)) {
      Proceed();
    }
  }

private:
  void Proceed() const
  {
    if (mIsRestart) {
      BT_LOGR("ProfileDeinitResultHandler::Proceed cancel cleanup() ");
      return;
    }

    sBtInterface->Cleanup(new CleanupResultHandler());
  }

  unsigned char mNumProfiles;
  bool mIsRestart;
};

class BluetoothServiceBluedroid::SetAdapterPropertyDiscoverableResultHandler
  final
  : public BluetoothResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_LOGR("Fail to set: BT_SCAN_MODE_CONNECTABLE");
  }
};

void
BluetoothServiceBluedroid::AdapterStateChangedNotification(bool aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("BT_STATE: %d", aState);

  if (mIsRestart && aState) {
    // daemon restarted, reset flag
    BT_LOGR("daemon restarted, reset flag");
    mIsRestart = false;
    mIsFirstTimeToggleOffBt = false;
  }

  mEnabled = aState;

  if (!mEnabled) {
    static void (* const sDeinitManager[])(BluetoothProfileResultHandler*) = {
      BluetoothHfpManager::DeinitHfpInterface,
      BluetoothA2dpManager::DeinitA2dpInterface,
      BluetoothGattManager::DeinitGattInterface
    };

    // Return error if BluetoothService is unavailable
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Cleanup static adapter properties and notify adapter.
    mBdAddress.Truncate();
    mBdName.Truncate();

    InfallibleTArray<BluetoothNamedValue> props;
    AppendNamedValue(props, "Name", mBdName);
    AppendNamedValue(props, "Address", mBdAddress);
    if (mDiscoverable) {
      mDiscoverable = false;
      AppendNamedValue(props, "Discoverable", false);
    }
    if (mDiscovering) {
      mDiscovering = false;
      AppendNamedValue(props, "Discovering", false);
    }

    bs->DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         BluetoothValue(props));

    // Cleanup Bluetooth interfaces after state becomes BT_STATE_OFF. This
    // will also stop the Bluetooth daemon and disable the adapter.
    nsRefPtr<ProfileDeinitResultHandler> res =
      new ProfileDeinitResultHandler(MOZ_ARRAY_LENGTH(sDeinitManager),
                                     mIsRestart);

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sDeinitManager); ++i) {
      sDeinitManager[i](res);
    }
  }

  if (mEnabled) {

    // We enable the Bluetooth adapter here. Disabling is implemented
    // in |CleanupResultHandler|, which runs at the end of the shutdown
    // procedure. We cannot disable the adapter immediately, because re-
    // enabling it might interfere with the shutdown procedure.
    BluetoothService::AcknowledgeToggleBt(true);

    // Bluetooth just enabled, clear profile controllers and runnable arrays.
    sControllerArray.Clear();
    mGetDeviceRequests.Clear();
    mChangeDiscoveryRunnables.Clear();
    mSetAdapterPropertyRunnables.Clear();
    mFetchUuidsRunnables.Clear();
    mCreateBondRunnables.Clear();
    mRemoveBondRunnables.Clear();
    mDeviceNameMap.Clear();

    // Bluetooth scan mode is SCAN_MODE_CONNECTABLE by default, i.e., it should
    // be connectable and non-discoverable.
    NS_ENSURE_TRUE_VOID(sBtInterface);
    sBtInterface->SetAdapterProperty(
      BluetoothNamedValue(NS_ConvertUTF8toUTF16("Discoverable"), false),
      new SetAdapterPropertyDiscoverableResultHandler());

    // Trigger OPP & PBAP managers to listen
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_LOGR("Fail to start BluetoothOppManager listening");
    }

    BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
    if (!pbap || !pbap->Listen()) {
      BT_LOGR("Fail to start BluetoothPbapManager listening");
    }

    BluetoothMapSmsManager* map = BluetoothMapSmsManager::Get();
    if (!map || !map->Listen()) {
      BT_LOGR("Fail to start BluetoothMapSmsManager listening");
    }
  }

  // Resolve promise if existed
  if (!mChangeAdapterStateRunnables.IsEmpty()) {
    DispatchReplySuccess(mChangeAdapterStateRunnables[0]);
    mChangeAdapterStateRunnables.RemoveElementAt(0);
  }

  // After ProfileManagers deinit and cleanup, now restart bluetooth daemon
  if (mIsRestart && !aState) {
    BT_LOGR("mIsRestart and off, now restart");
    StartBluetooth(false, nullptr);
  }
}

/**
 * AdapterPropertiesNotification will be called after enable() but before
 * AdapterStateChangeCallback is called. At that moment, both BluetoothManager
 * and BluetoothAdapter have not registered observer yet.
 */
void
BluetoothServiceBluedroid::AdapterPropertiesNotification(
  BluetoothStatus aStatus, int aNumProperties,
  const BluetoothProperty* aProperties)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      mBdAddress = p.mString;
      AppendNamedValue(propertiesArray, "Address", mBdAddress);

    } else if (p.mType == PROPERTY_BDNAME) {
      mBdName = p.mString;
      AppendNamedValue(propertiesArray, "Name", mBdName);

    } else if (p.mType == PROPERTY_ADAPTER_SCAN_MODE) {

      // If BT is not enabled, Bluetooth scan mode should be non-discoverable
      // by defalut. |AdapterStateChangedNotification| would set default
      // properties to bluetooth backend once Bluetooth is enabled.
      if (IsEnabled()) {
        mDiscoverable = (p.mScanMode == SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        AppendNamedValue(propertiesArray, "Discoverable", mDiscoverable);
      }
    } else if (p.mType == PROPERTY_ADAPTER_BONDED_DEVICES) {
      // We have to cache addresses of bonded devices. Unlike BlueZ,
      // Bluedroid would not send another PROPERTY_ADAPTER_BONDED_DEVICES
      // event after bond completed.
      BT_LOGD("Adapter property: BONDED_DEVICES. Count: %d",
              p.mStringArray.Length());

      // Whenever reloading paired devices, force refresh
      mBondedAddresses.Clear();
      mBondedAddresses.AppendElements(p.mStringArray);

      AppendNamedValue(propertiesArray, "PairedDevices", mBondedAddresses);
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Unhandled adapter property type: %d", p.mType);
      continue;
    }
  }

  NS_ENSURE_TRUE_VOID(propertiesArray.Length() > 0);

  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Send reply for SetProperty
  if (!mSetAdapterPropertyRunnables.IsEmpty()) {
    DispatchReplySuccess(mSetAdapterPropertyRunnables[0]);
    mSetAdapterPropertyRunnables.RemoveElementAt(0);
  }
}

/**
 * RemoteDevicePropertiesNotification will be called
 *
 *   (1) automatically by Bluedroid when BT is turning on, or
 *   (2) as result of remote device properties update during discovery, or
 *   (3) as result of CreateBond, or
 *   (4) as result of GetRemoteDeviceProperties, or
 *   (5) as result of GetRemoteServices.
 */
void
BluetoothServiceBluedroid::RemoteDevicePropertiesNotification(
  BluetoothStatus aStatus, const nsAString& aBdAddr,
  int aNumProperties, const BluetoothProperty* aProperties)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  nsString bdAddr(aBdAddr);
  AppendNamedValue(propertiesArray, "Address", bdAddr);

  for (int i = 0; i < aNumProperties; ++i) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDNAME) {
      AppendNamedValue(propertiesArray, "Name", p.mString);

      // Update <address, name> mapping
      mDeviceNameMap.Remove(bdAddr);
      mDeviceNameMap.Put(bdAddr, p.mString);
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = p.mUint32;
      AppendNamedValue(propertiesArray, "Cod", cod);

    } else if (p.mType == PROPERTY_UUIDS) {

      size_t index;

      // Handler for |UpdateSdpRecords|

      for (index = 0; index < mGetRemoteServicesArray.Length(); ++index) {
        if (mGetRemoteServicesArray[index].mDeviceAddress == aBdAddr) {
          break;
        }
      }

      if (index < mGetRemoteServicesArray.Length()) {
        mGetRemoteServicesArray[index].mManager->OnUpdateSdpRecords(aBdAddr);
        mGetRemoteServicesArray.RemoveElementAt(index);
        continue; // continue with outer loop
      }

      // Handler for |FetchUuidsInternal|

      nsTArray<nsString> uuids;

      // Construct a sorted uuid set
      for (index = 0; index < p.mUuidArray.Length(); ++index) {
        nsAutoString uuid;
        UuidToString(p.mUuidArray[index], uuid);

        if (!uuids.Contains(uuid)) { // filter out duplicate uuids
          uuids.InsertElementSorted(uuid);
        }
      }
      AppendNamedValue(propertiesArray, "UUIDs", uuids);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Type",
                       static_cast<uint32_t>(p.mTypeOfDevice));

    } else if (p.mType == PROPERTY_SERVICE_RECORD) {

      size_t i;

      // Find call in array

      for (i = 0; i < mGetRemoteServiceRecordArray.Length(); ++i) {
        if ((mGetRemoteServiceRecordArray[i].mDeviceAddress == aBdAddr) &&
            (mGetRemoteServiceRecordArray[i].mUuid == p.mServiceRecord.mUuid)) {

          // Signal channel to profile manager
          nsAutoString uuidStr;
          UuidToString(mGetRemoteServiceRecordArray[i].mUuid, uuidStr);

          mGetRemoteServiceRecordArray[i].mManager->OnGetServiceChannel(
            aBdAddr, uuidStr, p.mServiceRecord.mChannel);

          mGetRemoteServiceRecordArray.RemoveElementAt(i);
          break;
        }
      }
      unused << NS_WARN_IF(i == mGetRemoteServiceRecordArray.Length());
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Other non-handled device properties. Type: %d", p.mType);
    }
  }

  // The order of operations below is
  //
  //  (1) modify global state (i.e., the variables starting with 's'),
  //  (2) distribute the signal, and finally
  //  (3) send any pending Bluetooth replies.
  //
  // |DispatchReplySuccess| creates its own internal runnable, which is
  // always run after we completed the current method. This means that we
  // can exchange |DispatchReplySuccess| with other operations without
  // changing the order of (1,2) and (3).

  // Update to registered BluetoothDevice objects
  BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                         bdAddr, propertiesArray);

  // FetchUuids task
  if (!mFetchUuidsRunnables.IsEmpty()) {
    // propertiesArray contains Address and Uuids only
    DispatchReplySuccess(mFetchUuidsRunnables[0],
                         propertiesArray[1].value()); /* Uuids */
    mFetchUuidsRunnables.RemoveElementAt(0);
    DistributeSignal(signal);
    return;
  }

  // GetDevices task
  if (mGetDeviceRequests.IsEmpty()) {
    // Callback is called after Bluetooth is turned on
    DistributeSignal(signal);
    return;
  }

  // Use address as the index
  mGetDeviceRequests[0].mDevicesPack.AppendElement(
    BluetoothNamedValue(bdAddr, propertiesArray));

  if (--mGetDeviceRequests[0].mDeviceCount == 0) {
    if (mGetDeviceRequests[0].mRunnable) {
      DispatchReplySuccess(mGetDeviceRequests[0].mRunnable,
                           mGetDeviceRequests[0].mDevicesPack);
    }
    mGetDeviceRequests.RemoveElementAt(0);
  }

  DistributeSignal(signal);
}

void
BluetoothServiceBluedroid::DeviceFoundNotification(
  int aNumProperties, const BluetoothProperty* aProperties)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  nsString bdAddr, bdName;
  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      AppendNamedValue(propertiesArray, "Address", p.mString);
      bdAddr = p.mString;
    } else if (p.mType == PROPERTY_BDNAME) {
      AppendNamedValue(propertiesArray, "Name", p.mString);
      bdName = p.mString;
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Cod", p.mUint32);

    } else if (p.mType == PROPERTY_UUIDS) {
      nsTArray<nsString> uuids;

      // Construct a sorted uuid set
      for (uint32_t index = 0; index < p.mUuidArray.Length(); ++index) {
        nsAutoString uuid;
        UuidToString(p.mUuidArray[index], uuid);

        if (!uuids.Contains(uuid)) { // filter out duplicate uuids
          uuids.InsertElementSorted(uuid);
        }
      }
      AppendNamedValue(propertiesArray, "UUIDs", uuids);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      AppendNamedValue(propertiesArray, "Type",
                       static_cast<uint32_t>(p.mTypeOfDevice));

    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Not handled remote device property: %d", p.mType);
    }
  }

  // Update <address, name> mapping
  mDeviceNameMap.Remove(bdAddr);
  mDeviceNameMap.Put(bdAddr, bdName);

  DistributeSignal(NS_LITERAL_STRING("DeviceFound"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));
}

void
BluetoothServiceBluedroid::DiscoveryStateChangedNotification(bool aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  mDiscovering = aState;

  // Fire PropertyChanged of Discovering
  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  AppendNamedValue(propertiesArray, "Discovering", mDiscovering);

  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Reply that Promise is resolved
  if (!mChangeDiscoveryRunnables.IsEmpty()) {
    DispatchReplySuccess(mChangeDiscoveryRunnables[0]);
    mChangeDiscoveryRunnables.RemoveElementAt(0);
  }
}

void
BluetoothServiceBluedroid::PinRequestNotification(
  const nsAString& aRemoteBdAddr, const nsAString& aBdName, uint32_t aCod)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  // If |aBdName| is empty, get device name from |mDeviceNameMap|;
  // Otherwise update <address, name> mapping with |aBdName|
  nsString bdAddr(aRemoteBdAddr);
  nsString bdName(aBdName);
  if (bdName.IsEmpty()) {
    mDeviceNameMap.Get(bdAddr, &bdName);
  } else {
    mDeviceNameMap.Remove(bdAddr);
    mDeviceNameMap.Put(bdAddr, bdName);
  }

  AppendNamedValue(propertiesArray, "address", bdAddr);
  AppendNamedValue(propertiesArray, "name", bdName);
  AppendNamedValue(propertiesArray, "passkey", EmptyString());
  AppendNamedValue(propertiesArray, "type",
                   NS_LITERAL_STRING(PAIRING_REQ_TYPE_ENTERPINCODE));

  DistributeSignal(NS_LITERAL_STRING("PairingRequest"),
                   NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                   BluetoothValue(propertiesArray));
}

void
BluetoothServiceBluedroid::SspRequestNotification(
  const nsAString& aRemoteBdAddr, const nsAString& aBdName, uint32_t aCod,
  BluetoothSspVariant aPairingVariant, uint32_t aPassKey)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  // If |aBdName| is empty, get device name from |mDeviceNameMap|;
  // Otherwise update <address, name> mapping with |aBdName|
  nsString bdAddr(aRemoteBdAddr);
  nsString bdName(aBdName);
  if (bdName.IsEmpty()) {
    mDeviceNameMap.Get(bdAddr, &bdName);
  } else {
    mDeviceNameMap.Remove(bdAddr);
    mDeviceNameMap.Put(bdAddr, bdName);
  }

  /**
   * Assign pairing request type and passkey based on the pairing variant.
   *
   * passkey value based on pairing request type:
   * 1) aPasskey: PAIRING_REQ_TYPE_CONFIRMATION and
   *              PAIRING_REQ_TYPE_DISPLAYPASSKEY
   * 2) empty string: PAIRING_REQ_TYPE_CONSENT
   */
  nsAutoString passkey;
  nsAutoString pairingType;
  switch (aPairingVariant) {
    case SSP_VARIANT_PASSKEY_CONFIRMATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONFIRMATION);
      passkey.AppendInt(aPassKey);
      break;
    case SSP_VARIANT_PASSKEY_NOTIFICATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_DISPLAYPASSKEY);
      passkey.AppendInt(aPassKey);
      break;
    case SSP_VARIANT_CONSENT:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONSENT);
      break;
    default:
      BT_WARNING("Unhandled SSP Bonding Variant: %d", aPairingVariant);
      return;
  }

  AppendNamedValue(propertiesArray, "address", bdAddr);
  AppendNamedValue(propertiesArray, "name", bdName);
  AppendNamedValue(propertiesArray, "passkey", passkey);
  AppendNamedValue(propertiesArray, "type", pairingType);

  DistributeSignal(NS_LITERAL_STRING("PairingRequest"),
                   NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                   BluetoothValue(propertiesArray));
}

void
BluetoothServiceBluedroid::BondStateChangedNotification(
  BluetoothStatus aStatus, const nsAString& aRemoteBdAddr,
  BluetoothBondState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aState == BOND_STATE_BONDING) {
    // No need to handle bonding state
    return;
  }

  BT_LOGR("Bond state: %d status: %d", aState, aStatus);

  bool bonded = (aState == BOND_STATE_BONDED);
  if (aStatus != STATUS_SUCCESS) {
    if (!bonded) { // Active/passive pair failed
      BT_LOGR("Pair failed! Abort pairing.");

      // Notify adapter of pairing aborted
      DistributeSignal(NS_LITERAL_STRING(PAIRING_ABORTED_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER));

      // Reject pair promise
      if (!mCreateBondRunnables.IsEmpty()) {
        DispatchReplyError(mCreateBondRunnables[0], aStatus);
        mCreateBondRunnables.RemoveElementAt(0);
      }
    } else if (!mRemoveBondRunnables.IsEmpty()) { // Active unpair failed
      // Reject unpair promise
      DispatchReplyError(mRemoveBondRunnables[0], aStatus);
      mRemoveBondRunnables.RemoveElementAt(0);
    }

    return;
  }

  // Query pairing device name from hash table
  nsString remoteBdAddr(aRemoteBdAddr);
  nsString remotebdName;
  mDeviceNameMap.Get(remoteBdAddr, &remotebdName);

  // Update bonded address array and append pairing device name
  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  if (!bonded) {
    mBondedAddresses.RemoveElement(remoteBdAddr);
  } else {
    if (!mBondedAddresses.Contains(remoteBdAddr)) {
      mBondedAddresses.AppendElement(remoteBdAddr);
    }

    // We don't assert |!remotebdName.IsEmpty()| since empty string is also
    // valid, according to Bluetooth Core Spec. v3.0 - Sec. 6.22:
    // "a valid Bluetooth name is a UTF-8 encoding string which is up to 248
    // bytes in length."
    AppendNamedValue(propertiesArray, "Name", remotebdName);
  }

  // Notify device of attribute changed
  AppendNamedValue(propertiesArray, "Paired", bonded);
  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   remoteBdAddr,
                   BluetoothValue(propertiesArray));

  // Notify adapter of device paired/unpaired
  InsertNamedValue(propertiesArray, 0, "Address", remoteBdAddr);
  DistributeSignal(bonded ? NS_LITERAL_STRING(DEVICE_PAIRED_ID)
                          : NS_LITERAL_STRING(DEVICE_UNPAIRED_ID),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Resolve existing pair/unpair promise
  if (bonded && !mCreateBondRunnables.IsEmpty()) {
    DispatchReplySuccess(mCreateBondRunnables[0]);
    mCreateBondRunnables.RemoveElementAt(0);
  } else if (!bonded && !mRemoveBondRunnables.IsEmpty()) {
    DispatchReplySuccess(mRemoveBondRunnables[0]);
    mRemoveBondRunnables.RemoveElementAt(0);
  }
}

void
BluetoothServiceBluedroid::AclStateChangedNotification(
  BluetoothStatus aStatus, const nsAString& aRemoteBdAddr, bool aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void
BluetoothServiceBluedroid::DutModeRecvNotification(uint16_t aOpcode,
                                                   const uint8_t* aBuf,
                                                   uint8_t aLen)
{
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void
BluetoothServiceBluedroid::LeTestModeNotification(BluetoothStatus aStatus,
                                                  uint16_t aNumPackets)
{
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void
BluetoothServiceBluedroid::EnergyInfoNotification(
  const BluetoothActivityEnergyInfo& aInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  // FIXME: This will be implemented in the later patchset
}

void
BluetoothServiceBluedroid::BackendErrorNotification(bool aCrashed)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCrashed) {
    return;
  }

  /*
   * Reset following profile manager states for unexpected backend crash.
   * - HFP: connection state and audio state
   * - A2DP: connection state
   */
  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE_VOID(hfp);
  hfp->HandleBackendError();
  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  a2dp->HandleBackendError();

  mIsRestart = true;
  BT_LOGR("Recovery step2: stop bluetooth");
  StopBluetooth(false, nullptr);
}

void
BluetoothServiceBluedroid::CompleteToggleBt(bool aEnabled)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mIsRestart && !aEnabled && mIsFirstTimeToggleOffBt) {
    // Both StopBluetooth and AdapterStateChangedNotification
    // trigger CompleteToggleBt. We don't need to call CompleteToggleBt again
  } else if (mIsRestart && !aEnabled && !mIsFirstTimeToggleOffBt) {
    // Recovery step 3: cleanup and deinit Profile managers
    BT_LOGR("CompleteToggleBt set mIsFirstTimeToggleOffBt = true");
    mIsFirstTimeToggleOffBt = true;
    BluetoothService::CompleteToggleBt(aEnabled);
    AdapterStateChangedNotification(false);
  } else {
    BluetoothService::CompleteToggleBt(aEnabled);
  }
}
