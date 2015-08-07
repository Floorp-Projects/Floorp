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
#ifndef MOZ_B2G_BT_API_V1
#include "BluetoothGattManager.h"
#else
// TODO: Support GATT
#endif
#include "BluetoothHfpManager.h"
#ifndef MOZ_B2G_BT_API_V1
#include "BluetoothHidManager.h"
#else
// TODO: Support HID
#endif
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

#ifndef MOZ_B2G_BT_API_V1
#define ENSURE_GATT_MGR_IS_READY_VOID(gatt, runnable)                  \
  do {                                                                 \
    if (!gatt) {                                                       \
      DispatchReplyError(runnable,                                     \
        NS_LITERAL_STRING("GattManager is not ready"));                \
      return;                                                          \
    }                                                                  \
  } while(0)
#else
  // Missing in Bluetooth v1
#endif

#ifndef MOZ_B2G_BT_API_V1
// Missing in Bluetooth v2
#else
// Audio: Major service class = 0x100 (Bit 21 is set)
#define SET_AUDIO_BIT(cod)               (cod |= 0x200000)
// Rendering: Major service class = 0x20 (Bit 18 is set)
#define SET_RENDERING_BIT(cod)           (cod |= 0x40000)
#endif

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

#ifndef MOZ_B2G_BT_API_V1
// Missing in Bluetooth v2
#else
void
BluetoothServiceBluedroid::ClassToIcon(uint32_t aClass, nsAString& aRetIcon)
{
  switch ((aClass & 0x1f00) >> 8) {
    case 0x01:
      aRetIcon.AssignLiteral("computer");
      break;
    case 0x02:
      switch ((aClass & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x05:
          aRetIcon.AssignLiteral("phone");
          break;
        case 0x04:
          aRetIcon.AssignLiteral("modem");
          break;
      }
      break;
    case 0x03:
      aRetIcon.AssignLiteral("network-wireless");
      break;
    case 0x04:
      switch ((aClass & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x06:
          aRetIcon.AssignLiteral("audio-card");
          break;
        case 0x0b:
        case 0x0c:
        case 0x0d:
          aRetIcon.AssignLiteral("camera-video");
          break;
        default:
          aRetIcon.AssignLiteral("audio-card");
          break;
      }
      break;
    case 0x05:
      switch ((aClass & 0xc0) >> 6) {
        case 0x00:
          switch ((aClass && 0x1e) >> 2) {
            case 0x01:
            case 0x02:
              aRetIcon.AssignLiteral("input-gaming");
              break;
          }
          break;
        case 0x01:
          aRetIcon.AssignLiteral("input-keyboard");
          break;
        case 0x02:
          switch ((aClass && 0x1e) >> 2) {
            case 0x05:
              aRetIcon.AssignLiteral("input-tablet");
              break;
            default:
              aRetIcon.AssignLiteral("input-mouse");
              break;
          }
      }
      break;
    case 0x06:
      if (aClass & 0x80) {
        aRetIcon.AssignLiteral("printer");
        break;
      }
      if (aClass & 0x20) {
        aRetIcon.AssignLiteral("camera-photo");
        break;
      }
      break;
  }

  if (aRetIcon.IsEmpty()) {
    if (HAS_AUDIO(aClass)) {
      /**
       * Property 'Icon' may be missed due to CoD of major class is TOY(0x08).
       * But we need to assign Icon as audio-card if service class is 'Audio'.
       * This is for PTS test case TC_AG_COD_BV_02_I. As HFP specification
       * defines that service class is 'Audio' can be considered as HFP HF.
       */
      aRetIcon.AssignLiteral("audio-card");
    } else {
      BT_LOGR("No icon to match class: %x", aClass);
    }
  }
}

static void
ReplyStatusError(BluetoothReplyRunnable* aBluetoothReplyRunnable,
                 BluetoothStatus aStatusCode, const nsAString& aCustomMsg)
{
  MOZ_ASSERT(aBluetoothReplyRunnable, "Reply runnable is nullptr");

  BT_LOGR("error code(%d)", aStatusCode);

  nsAutoString replyError;
  replyError.Assign(aCustomMsg);

  if (aStatusCode == STATUS_BUSY) {
    replyError.AppendLiteral(":BT_STATUS_BUSY");
  } else if (aStatusCode == STATUS_NOT_READY) {
    replyError.AppendLiteral(":BT_STATUS_NOT_READY");
  } else if (aStatusCode == STATUS_DONE) {
    replyError.AppendLiteral(":BT_STATUS_DONE");
  } else if (aStatusCode == STATUS_AUTH_FAILURE) {
    replyError.AppendLiteral(":BT_STATUS_AUTH_FAILURE");
  } else if (aStatusCode == STATUS_RMT_DEV_DOWN) {
    replyError.AppendLiteral(":BT_STATUS_RMT_DEV_DOWN");
  } else if (aStatusCode == STATUS_FAIL) {
    replyError.AppendLiteral(":BT_STATUS_FAIL");
  }

  DispatchReplyError(aBluetoothReplyRunnable, replyError);
}
#endif

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
#ifndef MOZ_B2G_BT_API_V1
      BluetoothGattManager::InitGattInterface
#else
      // Missing in Bluetooth v1
#endif
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
#ifndef MOZ_B2G_BT_API_V1
  // Missing in Bluetooth v2
#else
  , mDiscoverableTimeout(0)
#endif
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

#ifndef MOZ_B2G_BT_API_V1
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
    BluetoothA2dpManager::Get(),
    BluetoothOppManager::Get(),
    BluetoothPbapManager::Get(),
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
               !profileName.EqualsLiteral("PBAP")) {
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
#else
nsresult
BluetoothServiceBluedroid::StartInternal()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult ret = StartGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(false);
    BT_LOGR("Error");
  }

  return ret;
}

nsresult
BluetoothServiceBluedroid::StopInternal()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult ret = StopGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(true);
    BT_LOGR("Error");
  }

  return ret;
}
#endif

//
// GATT Client
//

#ifndef MOZ_B2G_BT_API_V1
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
#else
// Missing in Bluetooth v1
#endif

#ifndef MOZ_B2G_BT_API_V1
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
  BluetoothValue adaptersProperties = InfallibleTArray<BluetoothNamedValue>();
  uint32_t numAdapters = 1; // Bluedroid supports single adapter only

  for (uint32_t i = 0; i < numAdapters; i++) {
    BluetoothValue properties = InfallibleTArray<BluetoothNamedValue>();

    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "State", mEnabled);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Address", mBdAddress);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Name", mBdName);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Discoverable", mDiscoverable);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Discovering", mDiscovering);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "PairedDevices", mBondedAddresses);

    BT_APPEND_NAMED_VALUE(adaptersProperties.get_ArrayOfBluetoothNamedValue(),
                          "Adapter", properties);
  }

  DispatchReplySuccess(aRunnable, adaptersProperties);
  return NS_OK;
}
#else
nsresult
BluetoothServiceBluedroid::GetDefaultAdapterPathInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothValue v = InfallibleTArray<BluetoothNamedValue>();

  BT_APPEND_NAMED_VALUE(v.get_ArrayOfBluetoothNamedValue(),
                        "Address", mBdAddress);

  BT_APPEND_NAMED_VALUE(v.get_ArrayOfBluetoothNamedValue(),
                        "Name", mBdName);

  BT_APPEND_NAMED_VALUE(v.get_ArrayOfBluetoothNamedValue(),
                        "Discoverable", mDiscoverable);

  BT_APPEND_NAMED_VALUE(v.get_ArrayOfBluetoothNamedValue(),
                        "DiscoverableTimeout", mDiscoverableTimeout);

  BT_APPEND_NAMED_VALUE(v.get_ArrayOfBluetoothNamedValue(),
                        "Devices", mBondedAddresses);

  DispatchReplySuccess(aRunnable, v);

  return NS_OK;
}
#endif

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
#ifndef MOZ_B2G_BT_API_V1
        DispatchReplyError(mRequests[0].mRunnable,
          NS_LITERAL_STRING("GetRemoteDeviceProperties failed"));
#else
        DispatchReplySuccess(mRequests[0].mRunnable,
                             mRequests[0].mDevicesPack);
#endif
      }
      mRequests.RemoveElementAt(0);
    }
  }

private:
  nsTArray<GetDeviceRequest> mRequests;
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
#ifndef MOZ_B2G_BT_API_V1
    DispatchReplySuccess(aRunnable);
#else
    DispatchReplySuccess(aRunnable, InfallibleTArray<BluetoothNamedValue>());
#endif
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

class BluetoothServiceBluedroid::StartDiscoveryResultHandler final
  : public BluetoothResultHandler
{
public:
  StartDiscoveryResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  { }

#ifndef MOZ_B2G_BT_API_V1
  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }
#else
  void StartDiscovery() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    DispatchReplySuccess(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("StartDiscovery"));
  }
#endif

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  BluetoothReplyRunnable* mRunnable;
};

void
BluetoothServiceBluedroid::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

#ifndef MOZ_B2G_BT_API_V1
  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtInterface->StartDiscovery(
    new StartDiscoveryResultHandler(mChangeDiscoveryRunnables, aRunnable));
#else
  sBtInterface->StartDiscovery(
    new StartDiscoveryResultHandler(nullptr, aRunnable));
#endif
}

class BluetoothServiceBluedroid::CancelDiscoveryResultHandler final
  : public BluetoothResultHandler
{
public:
  CancelDiscoveryResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  { }

#ifndef MOZ_B2G_BT_API_V1
  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }
#else
  void CancelDiscovery() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    DispatchReplySuccess(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("StopDiscovery"));
  }
#endif

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  BluetoothReplyRunnable* mRunnable;
};

#ifndef MOZ_B2G_BT_API_V1
class BluetoothServiceBluedroid::GetRemoteServicesResultHandler final
  : public BluetoothResultHandler
{
public:
  GetRemoteServicesResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  BluetoothReplyRunnable* mRunnable;
};

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
    sBtInterface->CancelDiscovery(
      new CancelDiscoveryResultHandler(mChangeDiscoveryRunnables, aRunnable));
  }

  mFetchUuidsRunnables.AppendElement(aRunnable);

  sBtInterface->GetRemoteServices(aDeviceAddress,
    new GetRemoteServicesResultHandler(mFetchUuidsRunnables, aRunnable));

  return NS_OK;
}
#else
// Missing in bluetooth1
#endif

void
BluetoothServiceBluedroid::StopDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_BLUETOOTH_IS_READY_VOID(aRunnable);

#ifndef MOZ_B2G_BT_API_V1
  mChangeDiscoveryRunnables.AppendElement(aRunnable);
  sBtInterface->CancelDiscovery(
    new CancelDiscoveryResultHandler(mChangeDiscoveryRunnables,
                                     aRunnable));
#else
  sBtInterface->CancelDiscovery(
    new CancelDiscoveryResultHandler(nullptr, aRunnable));
#endif
}

class BluetoothServiceBluedroid::SetAdapterPropertyResultHandler final
  : public BluetoothResultHandler
{
public:
  SetAdapterPropertyResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

#ifndef MOZ_B2G_BT_API_V1
    mRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
#else
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("SetProperty"));
#endif
  }
private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  BluetoothReplyRunnable* mRunnable;
};

nsresult
BluetoothServiceBluedroid::SetProperty(BluetoothObjectType aType,
                                       const BluetoothNamedValue& aValue,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mSetAdapterPropertyRunnables.AppendElement(aRunnable);

  sBtInterface->SetAdapterProperty(aValue,
    new SetAdapterPropertyResultHandler(mSetAdapterPropertyRunnables,
                                        aRunnable));

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetServiceChannel(
  const nsAString& aDeviceAddress,
  const nsAString& aServiceUuid,
  BluetoothProfileManagerBase* aManager)
{
  return NS_OK;
}

bool
BluetoothServiceBluedroid::UpdateSdpRecords(
  const nsAString& aDeviceAddress,
  BluetoothProfileManagerBase* aManager)
{
  return true;
}

class BluetoothServiceBluedroid::CreateBondResultHandler final
  : public BluetoothResultHandler
{
public:
  CreateBondResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    mRunnableArray.RemoveElement(mRunnable);

#ifndef MOZ_B2G_BT_API_V1
    DispatchReplyError(mRunnable, aStatus);
#else
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("CreatedPairedDevice"));
#endif
  }

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothServiceBluedroid::CreatePairedDeviceInternal(
  const nsAString& aDeviceAddress, int aTimeout,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mCreateBondRunnables.AppendElement(aRunnable);

  sBtInterface->CreateBond(aDeviceAddress, TRANSPORT_AUTO,
    new CreateBondResultHandler(mCreateBondRunnables, aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::RemoveBondResultHandler final
  : public BluetoothResultHandler
{
public:
  RemoveBondResultHandler(
    nsTArray<nsRefPtr<BluetoothReplyRunnable>>& aRunnableArray,
    BluetoothReplyRunnable* aRunnable)
  : mRunnableArray(aRunnableArray)
  , mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    mRunnableArray.RemoveElement(mRunnable);

#ifndef MOZ_B2G_BT_API_V1
    DispatchReplyError(mRunnable, aStatus);
#else
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("RemoveDevice"));
#endif
  }

private:
  nsTArray<nsRefPtr<BluetoothReplyRunnable>> mRunnableArray;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothServiceBluedroid::RemoveDeviceInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  mRemoveBondRunnables.AppendElement(aRunnable);

  sBtInterface->RemoveBond(aDeviceAddress,
    new RemoveBondResultHandler(mRemoveBondRunnables, aRunnable));

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
#ifndef MOZ_B2G_BT_API_V1
    DispatchReplyError(mRunnable, aStatus);
#else
    ReplyStatusError(mRunnable, aStatus, NS_LITERAL_STRING("SetPinCode"));
#endif
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

#ifndef MOZ_B2G_BT_API_V1
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
  // Lecagy method used by BlueZ only.
}
#else
bool
BluetoothServiceBluedroid::SetPinCodeInternal(
  const nsAString& aDeviceAddress, const nsAString& aPinCode,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, false);

  sBtInterface->PinReply(aDeviceAddress, true, aPinCode,
    new PinReplyResultHandler(aRunnable));

  return true;
}
#endif

#ifndef MOZ_B2G_BT_API_V1
void
BluetoothServiceBluedroid::SetPasskeyInternal(
  const nsAString& aDeviceAddress, uint32_t aPasskey,
  BluetoothReplyRunnable* aRunnable)
{
  // Lecagy method used by BlueZ only.
}
#else
bool
BluetoothServiceBluedroid::SetPasskeyInternal(
  const nsAString& aDeviceAddress, uint32_t aPasskey,
  BluetoothReplyRunnable* aRunnable)
{
  return true;
}
#endif

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
#ifndef MOZ_B2G_BT_API_V1
    DispatchReplyError(mRunnable, aStatus);
#else
    ReplyStatusError(mRunnable, aStatus,
                     NS_LITERAL_STRING("SetPairingConfirmation"));
#endif
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

#ifndef MOZ_B2G_BT_API_V1
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
  // Lecagy method used by BlueZ only.
}
#else
bool
BluetoothServiceBluedroid::SetPairingConfirmationInternal(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, false);

  sBtInterface->SspReply(aDeviceAddress,
                         SSP_VARIANT_PASSKEY_CONFIRMATION,
                         aConfirm, 0, new SspReplyResultHandler(aRunnable));
  return true;
}
#endif

#ifndef MOZ_B2G_BT_API_V1
// Missing in bluetooth2
#else
bool
BluetoothServiceBluedroid::SetAuthorizationInternal(
  const nsAString& aDeviceAddress, bool aAllow,
  BluetoothReplyRunnable* aRunnable)
{
  return true;
}

nsresult
BluetoothServiceBluedroid::PrepareAdapterInternal()
{
  return NS_OK;
}
#endif

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

#ifndef MOZ_B2G_BT_API_V1
  // Missing in bluetooth2
#else
void
BluetoothServiceBluedroid::IsConnected(const uint16_t aServiceUuid,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  BluetoothProfileManagerBase* profile =
    BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (profile) {
    DispatchReplySuccess(aRunnable, profile->IsConnected());
  } else {
    BT_WARNING("Can't find profile manager with uuid: %x", aServiceUuid);
    DispatchReplySuccess(aRunnable, false);
  }
}
#endif

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
BluetoothServiceBluedroid::SendMetaData(const nsAString& aTitle,
                                        const nsAString& aArtist,
                                        const nsAString& aAlbum,
                                        int64_t aMediaNumber,
                                        int64_t aTotalMediaCount,
                                        int64_t aDuration,
                                        BluetoothReplyRunnable* aRunnable)
{
  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  if (a2dp) {
    a2dp->UpdateMetaData(aTitle, aArtist, aAlbum, aMediaNumber,
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
  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  if (a2dp) {
    ControlPlayStatus playStatus =
      PlayStatusStringToControlPlayStatus(aPlayStatus);
    a2dp->UpdatePlayStatus(aDuration, aPosition, playStatus);
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

#ifndef MOZ_B2G_BT_API_V1
// Missing in bluetooth2
#else
uint16_t
BluetoothServiceBluedroid::UuidToServiceClassInt(const BluetoothUuid& mUuid)
{
  // extract short UUID 0000xxxx-0000-1000-8000-00805f9b34fb
  uint16_t shortUuid;
  memcpy(&shortUuid, mUuid.mUuid + 2, sizeof(uint16_t));
  return ntohs(shortUuid);
}

bool
BluetoothServiceBluedroid::IsConnected(const nsAString& aRemoteBdAddr)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsString connectedAddress;

  // Check whether HFP/HSP are connected.
  BluetoothProfileManagerBase* profile;
  profile = BluetoothHfpManager::Get();
  if (profile && profile->IsConnected()) {
    profile->GetAddress(connectedAddress);
    if (aRemoteBdAddr.Equals(connectedAddress)) {
      return true;
    }
  }

  // Check whether OPP is connected.
  profile = BluetoothOppManager::Get();
  if (profile->IsConnected()) {
    profile->GetAddress(connectedAddress);
    if (aRemoteBdAddr.Equals(connectedAddress)) {
      return true;
    }
  }

  // Check whether A2DP is connected.
  profile = BluetoothA2dpManager::Get();
  if (profile->IsConnected()) {
    profile->GetAddress(connectedAddress);
    if (aRemoteBdAddr.Equals(connectedAddress)) {
      return true;
    }
  }

  return false;
}
#endif

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
#ifndef MOZ_B2G_BT_API_V1
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
    BT_APPEND_NAMED_VALUE(props, "Name", mBdName);
    BT_APPEND_NAMED_VALUE(props, "Address", mBdAddress);
    if (mDiscoverable) {
      mDiscoverable = false;
      BT_APPEND_NAMED_VALUE(props, "Discoverable", false);
    }
    if (mDiscovering) {
      mDiscovering = false;
      BT_APPEND_NAMED_VALUE(props, "Discovering", false);
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

#else
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("BT_STATE: %d", aState);

  if (mIsRestart && aState) {
    // daemon restarted, reset flag
    BT_LOGR("daemon restarted, reset flag");
    mIsRestart = false;
    mIsFirstTimeToggleOffBt = false;
  }
  bool isBtEnabled = (aState == true);

  if (!isBtEnabled) {
    static void (* const sDeinitManager[])(BluetoothProfileResultHandler*) = {
      BluetoothHfpManager::DeinitHfpInterface,
      BluetoothA2dpManager::DeinitA2dpInterface
    };

    // Set discoverable cache to default value after state becomes BT_STATE_OFF.
    if (mDiscoverable) {
      mDiscoverable = false;
    }

    // Cleanup bluetooth interfaces after BT state becomes BT_STATE_OFF.
    nsRefPtr<ProfileDeinitResultHandler> res =
      new ProfileDeinitResultHandler(MOZ_ARRAY_LENGTH(sDeinitManager));

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sDeinitManager); ++i) {
      sDeinitManager[i](res);
    }
  }

  BluetoothService::AcknowledgeToggleBt(isBtEnabled);

  if (isBtEnabled) {
    // Bluetooth just enabled, clear profile controllers and runnable arrays.
    sControllerArray.Clear();
    mCreateBondRunnables.Clear();
    mGetDeviceRunnables.Clear();
    mSetAdapterPropertyRunnables.Clear();
    mRemoveBondRunnables.Clear();

    // Bluetooth scan mode is SCAN_MODE_CONNECTABLE by default, i.e., It should
    // be connectable and non-discoverable.
    NS_ENSURE_TRUE_VOID(sBtInterface);
    sBtInterface->SetAdapterProperty(
      BluetoothNamedValue(NS_ConvertUTF8toUTF16("Discoverable"), false),
      new SetAdapterPropertyDiscoverableResultHandler());

    // Try to fire event 'AdapterAdded' to fit the original behaviour when
    // we used BlueZ as backend.
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    bs->AdapterAddedReceived();
    bs->TryFiringAdapterAdded();

    // Trigger OPP & PBAP managers to listen
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_LOGR("Fail to start BluetoothOppManager listening");
    }

    BluetoothPbapManager* pbap = BluetoothPbapManager::Get();
    if (!pbap || !pbap->Listen()) {
      BT_LOGR("Fail to start BluetoothPbapManager listening");
    }
  }

  // After ProfileManagers deinit and cleanup, now restarts bluetooth daemon
  if (mIsRestart && !aState) {
    BT_LOGR("mIsRestart and off, now restart");
    StartBluetooth(false);
  }
#endif
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
#ifndef MOZ_B2G_BT_API_V1
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      mBdAddress = p.mString;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", mBdAddress);

    } else if (p.mType == PROPERTY_BDNAME) {
      mBdName = p.mString;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", mBdName);

    } else if (p.mType == PROPERTY_ADAPTER_SCAN_MODE) {

      // If BT is not enabled, Bluetooth scan mode should be non-discoverable
      // by defalut. |AdapterStateChangedNotification| would set default
      // properties to bluetooth backend once Bluetooth is enabled.
      if (IsEnabled()) {
        mDiscoverable = (p.mScanMode == SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        BT_APPEND_NAMED_VALUE(propertiesArray, "Discoverable", mDiscoverable);
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

      BT_APPEND_NAMED_VALUE(propertiesArray, "PairedDevices",
                            mBondedAddresses);
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
#else
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> props;

  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      mBdAddress = p.mString;
      BT_APPEND_NAMED_VALUE(props, "Address", mBdAddress);

    } else if (p.mType == PROPERTY_BDNAME) {
      mBdName = p.mString;
      BT_APPEND_NAMED_VALUE(props, "Name", mBdName);

    } else if (p.mType == PROPERTY_ADAPTER_SCAN_MODE) {
      BluetoothScanMode newMode = p.mScanMode;

      // If BT is not enabled, Bluetooth scan mode should be non-discoverable
      // by defalut. 'AdapterStateChangedNotification' would set the default
      // properties to bluetooth backend once Bluetooth is enabled.
      mDiscoverable =
        (newMode == SCAN_MODE_CONNECTABLE_DISCOVERABLE && IsEnabled());
      BT_APPEND_NAMED_VALUE(props, "Discoverable", mDiscoverable);

    } else if (p.mType == PROPERTY_ADAPTER_DISCOVERY_TIMEOUT) {
      mDiscoverableTimeout = p.mUint32;
      BT_APPEND_NAMED_VALUE(props, "DiscoverableTimeout",
                            mDiscoverableTimeout);

    } else if (p.mType == PROPERTY_ADAPTER_BONDED_DEVICES) {
      // We have to cache addresses of bonded devices. Unlike BlueZ,
      // Bluedroid would not send another PROPERTY_ADAPTER_BONDED_DEVICES
      // event after bond completed.
      BT_LOGD("Adapter property: BONDED_DEVICES. Count: %d",
              p.mStringArray.Length());

      // Whenever reloading paired devices, force refresh
      mBondedAddresses.Clear();
      mBondedAddresses.AppendElements(p.mStringArray);

      BT_APPEND_NAMED_VALUE(props, "Devices", mBondedAddresses);

    } else if (p.mType == PROPERTY_UUIDS) {
      //FIXME: This will be implemented in the later patchset
      continue;
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
      continue;
    } else {
      BT_LOGD("Unhandled adapter property type: %d", p.mType);
      continue;
    }
  }

  NS_ENSURE_TRUE_VOID(props.Length() > 0);

  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("PropertyChanged"),
                                   NS_LITERAL_STRING(KEY_ADAPTER),
                                   BluetoothValue(props)));

  // Send reply for SetProperty
  if (!mSetAdapterPropertyRunnables.IsEmpty()) {
    DispatchReplySuccess(mSetAdapterPropertyRunnables[0]);
    mSetAdapterPropertyRunnables.RemoveElementAt(0);
  }
#endif
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
#ifndef MOZ_B2G_BT_API_V1
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  nsString bdAddr(aBdAddr);
  BT_APPEND_NAMED_VALUE(propertiesArray, "Address", bdAddr);

  for (int i = 0; i < aNumProperties; ++i) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", p.mString);

      // Update <address, name> mapping
      mDeviceNameMap.Remove(bdAddr);
      mDeviceNameMap.Put(bdAddr, p.mString);
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = p.mUint32;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Cod", cod);

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
      BT_APPEND_NAMED_VALUE(propertiesArray, "UUIDs", uuids);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Type",
                            static_cast<uint32_t>(p.mTypeOfDevice));

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
#else
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> props;

  BT_APPEND_NAMED_VALUE(props, "Address", BluetoothValue(nsString(aBdAddr)));

  bool isCodInvalid = false;
  for (int i = 0; i < aNumProperties; ++i) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(props, "Name", p.mString);
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = p.mUint32;
      nsString icon;
      ClassToIcon(cod, icon);
      if (!icon.IsEmpty()) {
        // Valid CoD
        BT_APPEND_NAMED_VALUE(props, "Class", cod);
        BT_APPEND_NAMED_VALUE(props, "Icon", icon);
      } else {
        // If Cod is invalid, fallback to check UUIDs. It usually happens due to
        // NFC directly trigger pairing. bluedroid sends wrong CoD due to missing
        // EIR query records.
        isCodInvalid = true;
      }
    } else if (p.mType == PROPERTY_UUIDS) {
      InfallibleTArray<nsString> uuidsArray;
      uint32_t cod = 0;

      for (size_t i = 0; i < p.mUuidArray.Length(); i++) {
        uint16_t uuidServiceClass = UuidToServiceClassInt(p.mUuidArray[i]);
        BluetoothServiceClass serviceClass =
          BluetoothUuidHelper::GetBluetoothServiceClass(uuidServiceClass);

        // Get Uuid string from BluetoothServiceClass
        nsString uuid;
        BluetoothUuidHelper::GetString(serviceClass, uuid);
        uuidsArray.AppendElement(uuid);

        // Restore CoD value
        if (isCodInvalid) {
          if (serviceClass == BluetoothServiceClass::HANDSFREE ||
              serviceClass == BluetoothServiceClass::HEADSET) {
            BT_LOGD("Restore Class Of Device to Audio bit");
            SET_AUDIO_BIT(cod);
          } else if (serviceClass == BluetoothServiceClass::A2DP_SINK) {
            BT_LOGD("Restore Class of Device to Rendering bit");
            SET_RENDERING_BIT(cod);
          }
        }
      }

      if (isCodInvalid) {
        BT_APPEND_NAMED_VALUE(props, "Class", cod);
        // 'audio-card' refers to 'Audio' device
        BT_APPEND_NAMED_VALUE(props, "Icon", NS_LITERAL_STRING("audio-card"));
      }
      BT_APPEND_NAMED_VALUE(props, "UUIDS", uuidsArray);
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Other non-handled device properties. Type: %d", p.mType);
    }
  }

  // BlueDroid wouldn't notify the status of connection, therefore, query the
  // connection state and append to properties array
  BT_APPEND_NAMED_VALUE(props, "Connected", IsConnected(aBdAddr));

  if (mGetDeviceRequests.IsEmpty()) {
    /**
     * This is possible when
     *
     *  (1) the callback is called when BT is turning on, or
     *  (2) remote device properties get updated during discovery, or
     *  (3) as result of CreateBond
     */
    if (mDiscovering) {
      // Fire 'devicefound' again to update device name for (2).
      // See bug 1076553 for more information.
      DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("DeviceFound"),
                                       NS_LITERAL_STRING(KEY_ADAPTER),
                                       BluetoothValue(props)));
    }
    return;
  }

  // Use address as the index
  mGetDeviceRequests[0].mDevicesPack.AppendElement(
    BluetoothNamedValue(nsString(aBdAddr), propertiesArray));

  if (--mGetDeviceRequests[0].mDeviceCount == 0) {
    if (mGetDeviceRequests[0].mRunnable) {
      DispatchReplySuccess(mGetDeviceRequests[0].mRunnable,
                           mGetDeviceRequests[0].mDevicesPack);
    }
    mGetDeviceRequests.RemoveElementAt(0);
  }

  // Update to registered BluetoothDevice objects
  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("PropertyChanged"),
                                   nsString(aBdAddr),
                                   BluetoothValue(props)));
#endif
}

void
BluetoothServiceBluedroid::DeviceFoundNotification(
  int aNumProperties, const BluetoothProperty* aProperties)
{
#ifndef MOZ_B2G_BT_API_V1
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  nsString bdAddr, bdName;
  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", p.mString);
      bdAddr = p.mString;
    } else if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", p.mString);
      bdName = p.mString;
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Cod", p.mUint32);

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
      BT_APPEND_NAMED_VALUE(propertiesArray, "UUIDs", uuids);

    } else if (p.mType == PROPERTY_TYPE_OF_DEVICE) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Type",
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
#else
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothValue propertyValue;
  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      propertyValue = p.mString;

      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", propertyValue);
    } else if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", p.mString);
    } else if (p.mType == PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = p.mUint32;
      propertyValue = cod;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Class", propertyValue);

      nsString icon;
      ClassToIcon(cod, icon);
      propertyValue = icon;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Icon", propertyValue);
    } else if (p.mType == PROPERTY_UNKNOWN) {
      /* Bug 1065999: working around unknown properties */
    } else {
      BT_LOGD("Not handled remote device property: %d", p.mType);
    }
  }

  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("DeviceFound"),
                                   NS_LITERAL_STRING(KEY_ADAPTER),
                                   BluetoothValue(propertiesArray)));
#endif
}

void
BluetoothServiceBluedroid::DiscoveryStateChangedNotification(bool aState)
{
#ifndef MOZ_B2G_BT_API_V1
  MOZ_ASSERT(NS_IsMainThread());

  mDiscovering = aState;

  // Fire PropertyChanged of Discovering
  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  BT_APPEND_NAMED_VALUE(propertiesArray, "Discovering", mDiscovering);

  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Reply that Promise is resolved
  if (!mChangeDiscoveryRunnables.IsEmpty()) {
    DispatchReplySuccess(mChangeDiscoveryRunnables[0]);
    mChangeDiscoveryRunnables.RemoveElementAt(0);
  }
#else
  MOZ_ASSERT(NS_IsMainThread());

  mDiscovering = aState;

  DistributeSignal(
    BluetoothSignal(NS_LITERAL_STRING(DISCOVERY_STATE_CHANGED_ID),
                    NS_LITERAL_STRING(KEY_ADAPTER), mDiscovering));

  // Distribute "PropertyChanged" signal to notice adapter this change since
  // Bluedroid don' treat "discovering" as a property of adapter.
  InfallibleTArray<BluetoothNamedValue> props;
  BT_APPEND_NAMED_VALUE(props, "Discovering", mDiscovering);
  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("PropertyChanged"),
                                   NS_LITERAL_STRING(KEY_ADAPTER),
                                   BluetoothValue(props)));
#endif
}

void
BluetoothServiceBluedroid::PinRequestNotification(const nsAString& aRemoteBdAddr,
                                                  const nsAString& aBdName,
                                                  uint32_t aCod)
{
#ifndef MOZ_B2G_BT_API_V1
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

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", bdAddr);
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", bdName);
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", EmptyString());
  BT_APPEND_NAMED_VALUE(propertiesArray, "type",
                        NS_LITERAL_STRING(PAIRING_REQ_TYPE_ENTERPINCODE));

  DistributeSignal(NS_LITERAL_STRING("PairingRequest"),
                   NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                   BluetoothValue(propertiesArray));
#else
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", nsString(aRemoteBdAddr));
  BT_APPEND_NAMED_VALUE(propertiesArray, "method",
                        NS_LITERAL_STRING("pincode"));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", nsString(aBdName));

  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("RequestPinCode"),
                                   NS_LITERAL_STRING(KEY_LOCAL_AGENT),
                                   BluetoothValue(propertiesArray)));
#endif
}

void
BluetoothServiceBluedroid::SspRequestNotification(
  const nsAString& aRemoteBdAddr, const nsAString& aBdName, uint32_t aCod,
  BluetoothSspVariant aPairingVariant, uint32_t aPassKey)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifndef MOZ_B2G_BT_API_V1
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

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", bdAddr);
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", bdName);
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", passkey);
  BT_APPEND_NAMED_VALUE(propertiesArray, "type", pairingType);

  DistributeSignal(NS_LITERAL_STRING("PairingRequest"),
                   NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                   BluetoothValue(propertiesArray));
#else
  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", nsString(aRemoteBdAddr));
  BT_APPEND_NAMED_VALUE(propertiesArray, "method",
                        NS_LITERAL_STRING("confirmation"));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", nsString(aBdName));
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", aPassKey);

  DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("RequestConfirmation"),
                                   NS_LITERAL_STRING(KEY_LOCAL_AGENT),
                                   BluetoothValue(propertiesArray)));
#endif
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

#ifndef MOZ_B2G_BT_API_V1
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
    BT_APPEND_NAMED_VALUE(propertiesArray, "Name", remotebdName);
  }

  // Notify device of attribute changed
  BT_APPEND_NAMED_VALUE(propertiesArray, "Paired", bonded);
  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   remoteBdAddr,
                   BluetoothValue(propertiesArray));

  // Notify adapter of device paired/unpaired
  BT_INSERT_NAMED_VALUE(propertiesArray, 0, "Address", remoteBdAddr);
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
#else
  if (aState == BOND_STATE_BONDED &&
      mBondedAddresses.Contains(aRemoteBdAddr)) {
    // See bug 940271 for more details about this case.
    return;
  }

  switch (aStatus) {
    case STATUS_SUCCESS:
    {
      bool bonded;
      if (aState == BOND_STATE_NONE) {
        bonded = false;
        mBondedAddresses.RemoveElement(aRemoteBdAddr);
      } else if (aState == BOND_STATE_BONDED) {
        bonded = true;
        mBondedAddresses.AppendElement(aRemoteBdAddr);
      } else {
        return;
      }

      // Update bonded address list to BluetoothAdapter
      InfallibleTArray<BluetoothNamedValue> propertiesChangeArray;
      BT_APPEND_NAMED_VALUE(propertiesChangeArray, "Devices",
                            mBondedAddresses);

      DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("PropertyChanged"),
                                       NS_LITERAL_STRING(KEY_ADAPTER),
                                       BluetoothValue(propertiesChangeArray)));

      if (bonded && !mCreateBondRunnables.IsEmpty()) {
        DispatchReplySuccess(mCreateBondRunnables[0]);

        mCreateBondRunnables.RemoveElementAt(0);
      } else if (!bonded && !mRemoveBondRunnables.IsEmpty()) {
        DispatchReplySuccess(mRemoveBondRunnables[0]);

        mRemoveBondRunnables.RemoveElementAt(0);
      }

      // Update bonding status to gaia
      InfallibleTArray<BluetoothNamedValue> propertiesArray;
      BT_APPEND_NAMED_VALUE(propertiesArray, "address", nsString(aRemoteBdAddr));
      BT_APPEND_NAMED_VALUE(propertiesArray, "status", bonded);

      DistributeSignal(
        BluetoothSignal(NS_LITERAL_STRING(PAIRED_STATUS_CHANGED_ID),
                        NS_LITERAL_STRING(KEY_ADAPTER),
                        BluetoothValue(propertiesArray)));
      break;
    }
    case STATUS_BUSY:
    case STATUS_AUTH_FAILURE:
    case STATUS_RMT_DEV_DOWN:
    {
      InfallibleTArray<BluetoothNamedValue> propertiesArray;
      DistributeSignal(BluetoothSignal(NS_LITERAL_STRING("Cancel"),
                                 NS_LITERAL_STRING(KEY_LOCAL_AGENT),
                                 BluetoothValue(propertiesArray)));

      if (!mCreateBondRunnables.IsEmpty()) {
        DispatchReplyError(mCreateBondRunnables[0],
                           NS_LITERAL_STRING("Authentication failure"));
        mCreateBondRunnables.RemoveElementAt(0);
      }
      break;
    }
    default:
      BT_WARNING("Got an unhandled status of BondStateChangedCallback!");
      // Dispatch a reply to unblock the waiting status of pairing.
      if (!mCreateBondRunnables.IsEmpty()) {
        DispatchReplyError(mCreateBondRunnables[0],
                           NS_LITERAL_STRING("Internal failure"));
        mCreateBondRunnables.RemoveElementAt(0);
      }
      break;
  }
#endif
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
#ifndef MOZ_B2G_BT_API_V1
  StopBluetooth(false, nullptr);
#else
  StopBluetooth(false);
#endif
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
