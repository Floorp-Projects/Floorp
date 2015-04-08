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
#include "BluetoothGattManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/UnixSocket.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "nsDataHashtable.h"

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

static nsString sAdapterBdAddress;
static nsString sAdapterBdName;
static bool sAdapterDiscoverable(false);
static bool sAdapterDiscovering(false);
static bool sAdapterEnabled(false);
// InfallibleTArray is an alias for nsTArray.
static InfallibleTArray<nsString> sAdapterBondedAddressArray;

// Use a static hash table to keep the name of remote device during the pairing
// procedure. In this manner, BT service and adapter can get the name of paired
// device name when bond state changed.
// The hash Key is BD address, the Value is remote BD name.
static nsDataHashtable<nsStringHashKey, nsString> sPairingNameTable;

static BluetoothInterface* sBtInterface;
static nsTArray<nsRefPtr<BluetoothProfileController> > sControllerArray;
static InfallibleTArray<BluetoothNamedValue> sRemoteDevicesPack;
static nsTArray<int> sRequestedDeviceCountArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sChangeAdapterStateRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sChangeDiscoveryRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sSetPropertyRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sGetDeviceRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sFetchUuidsRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sBondingRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sUnbondingRunnableArray;

/**
 *  Static callback functions
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

/**
 *  Static functions
 */
bool
BluetoothServiceBluedroid::EnsureBluetoothHalLoad()
{
  sBtInterface = BluetoothInterface::GetInstance();
  NS_ENSURE_TRUE(sBtInterface, false);

  return true;
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
      BluetoothGattManager::InitGattInterface
    };

    MOZ_ASSERT(NS_IsMainThread());

    // Register all the bluedroid callbacks before enable() get called
    // It is required to register a2dp callbacks before a2dp media task starts up.
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

    BluetoothService::AcknowledgeToggleBt(true);
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

/**
 *  Member functions
 */
BluetoothServiceBluedroid::BluetoothServiceBluedroid()
{
  if (!EnsureBluetoothHalLoad()) {
    BT_LOGR("Error! Failed to load bluedroid library.");
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
  if(aRunnable) {
    sChangeAdapterStateRunnableArray.AppendElement(aRunnable);
  }

  nsresult ret = StartGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(false);

    // Reject Promise
    if(aRunnable) {
      DispatchReplyError(aRunnable, NS_LITERAL_STRING("StartBluetoothError"));
      sChangeAdapterStateRunnableArray.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
}

nsresult
BluetoothServiceBluedroid::StopInternal(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothProfileManagerBase* profile;
  profile = BluetoothHfpManager::Get();
  NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
  if (profile->IsConnected()) {
    profile->Disconnect(nullptr);
  } else {
    profile->Reset();
  }

  profile = BluetoothOppManager::Get();
  NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
  if (profile->IsConnected()) {
    profile->Disconnect(nullptr);
  }

  profile = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
  if (profile->IsConnected()) {
    profile->Disconnect(nullptr);
  } else {
    profile->Reset();
  }

  profile = BluetoothHidManager::Get();
  NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
  if (profile->IsConnected()) {
    profile->Disconnect(nullptr);
  } else {
    profile->Reset();
  }

  // aRunnable will be a nullptr during starup and shutdown
  if(aRunnable) {
    sChangeAdapterStateRunnableArray.AppendElement(aRunnable);
  }

  nsresult ret = StopGonkBluetooth();
  if (NS_FAILED(ret)) {
    BluetoothService::AcknowledgeToggleBt(true);

    // Reject Promise
    if(aRunnable) {
      DispatchReplyError(aRunnable, NS_LITERAL_STRING("StopBluetoothError"));
      sChangeAdapterStateRunnableArray.RemoveElement(aRunnable);
    }

    BT_LOGR("Error");
  }

  return ret;
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
  BluetoothValue adaptersProperties = InfallibleTArray<BluetoothNamedValue>();
  uint32_t numAdapters = 1; // Bluedroid supports single adapter only

  for (uint32_t i = 0; i < numAdapters; i++) {
    BluetoothValue properties = InfallibleTArray<BluetoothNamedValue>();

    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "State", sAdapterEnabled);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Address", sAdapterBdAddress);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Name", sAdapterBdName);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Discoverable", sAdapterDiscoverable);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "Discovering", sAdapterDiscovering);
    BT_APPEND_NAMED_VALUE(properties.get_ArrayOfBluetoothNamedValue(),
                          "PairedDevices", sAdapterBondedAddressArray);

    BT_APPEND_NAMED_VALUE(adaptersProperties.get_ArrayOfBluetoothNamedValue(),
                          "Adapter", properties);
  }

  DispatchReplySuccess(aRunnable, adaptersProperties);
  return NS_OK;
}

class BluetoothServiceBluedroid::GetRemoteDevicePropertiesResultHandler
  final
  : public BluetoothResultHandler
{
public:
  GetRemoteDevicePropertiesResultHandler(const nsAString& aDeviceAddress)
  : mDeviceAddress(aDeviceAddress)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("GetRemoteDeviceProperties(%s) failed: %d",
               NS_ConvertUTF16toUTF8(mDeviceAddress).get(), aStatus);

    /* dispatch result after final pending operation */
    if (--sRequestedDeviceCountArray[0] == 0) {
      if (!sGetDeviceRunnableArray.IsEmpty()) {
        DispatchReplyError(sGetDeviceRunnableArray[0],
          NS_LITERAL_STRING("GetRemoteDeviceProperties failed"));
        sGetDeviceRunnableArray.RemoveElementAt(0);
      }

      sRequestedDeviceCountArray.RemoveElementAt(0);
      sRemoteDevicesPack.Clear();
    }
  }

private:
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

  nsTArray<nsString> deviceAddresses;
  if (profile->IsConnected()) {
    nsString address;
    profile->GetAddress(address);
    deviceAddresses.AppendElement(address);
  }

  int requestedDeviceCount = deviceAddresses.Length();
  if (requestedDeviceCount == 0) {
    InfallibleTArray<BluetoothNamedValue> emptyArr;
    DispatchReplySuccess(aRunnable, emptyArr);
    return NS_OK;
  }

  sRequestedDeviceCountArray.AppendElement(requestedDeviceCount);
  sGetDeviceRunnableArray.AppendElement(aRunnable);

  for (int i = 0; i < requestedDeviceCount; i++) {
    // Retrieve all properties of devices
    sBtInterface->GetRemoteDeviceProperties(deviceAddresses[i],
      new GetRemoteDevicePropertiesResultHandler(deviceAddresses[i]));
  }

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetPairedDevicePropertiesInternal(
  const nsTArray<nsString>& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  int requestedDeviceCount = aDeviceAddress.Length();
  if (requestedDeviceCount == 0) {
    DispatchReplySuccess(aRunnable);
    return NS_OK;
  }

  for (int i = 0; i < requestedDeviceCount; i++) {
    // Retrieve all properties of devices
    sBtInterface->GetRemoteDeviceProperties(aDeviceAddress[i],
      new GetRemoteDevicePropertiesResultHandler(aDeviceAddress[i]));
  }

  return NS_OK;
}

class BluetoothServiceBluedroid::StartDiscoveryResultHandler final
  : public BluetoothResultHandler
{
public:
  StartDiscoveryResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    sChangeDiscoveryRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

nsresult
BluetoothServiceBluedroid::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  sChangeDiscoveryRunnableArray.AppendElement(aRunnable);
  sBtInterface->StartDiscovery(new StartDiscoveryResultHandler(aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::CancelDiscoveryResultHandler final
  : public BluetoothResultHandler
{
public:
  CancelDiscoveryResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    sChangeDiscoveryRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  BluetoothReplyRunnable* mRunnable;
};

nsresult
BluetoothServiceBluedroid::StopDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  sChangeDiscoveryRunnableArray.AppendElement(aRunnable);
  sBtInterface->CancelDiscovery(new CancelDiscoveryResultHandler(aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::GetRemoteServicesResultHandler final
  : public BluetoothResultHandler
{
public:
  GetRemoteServicesResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    sFetchUuidsRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
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
  if (sAdapterDiscovering) {
    sBtInterface->CancelDiscovery(new CancelDiscoveryResultHandler(aRunnable));
  }

  sFetchUuidsRunnableArray.AppendElement(aRunnable);

  sBtInterface->GetRemoteServices(aDeviceAddress,
    new GetRemoteServicesResultHandler(aRunnable));

  return NS_OK;
}

class BluetoothServiceBluedroid::SetAdapterPropertyResultHandler final
  : public BluetoothResultHandler
{
public:
  SetAdapterPropertyResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    sSetPropertyRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }
private:
  BluetoothReplyRunnable* mRunnable;
};

nsresult
BluetoothServiceBluedroid::SetProperty(BluetoothObjectType aType,
                                       const BluetoothNamedValue& aValue,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  sSetPropertyRunnableArray.AppendElement(aRunnable);

  sBtInterface->SetAdapterProperty(aValue,
    new SetAdapterPropertyResultHandler(aRunnable));

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
  CreateBondResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    sBondingRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothServiceBluedroid::CreatePairedDeviceInternal(
  const nsAString& aDeviceAddress, int aTimeout,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  sBondingRunnableArray.AppendElement(aRunnable);

  sBtInterface->CreateBond(aDeviceAddress,
                           new CreateBondResultHandler(aRunnable));
  return NS_OK;
}

class BluetoothServiceBluedroid::RemoveBondResultHandler final
  : public BluetoothResultHandler
{
public:
  RemoveBondResultHandler(BluetoothReplyRunnable* aRunnable)
  : mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    sUnbondingRunnableArray.RemoveElement(mRunnable);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothServiceBluedroid::RemoveDeviceInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  sUnbondingRunnableArray.AppendElement(aRunnable);

  sBtInterface->RemoveBond(aDeviceAddress,
                           new RemoveBondResultHandler(aRunnable));

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
BluetoothServiceBluedroid::SetPinCodeInternal(
  const nsAString& aDeviceAddress, const nsAString& aPinCode,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by bluez only.
}

void
BluetoothServiceBluedroid::SetPasskeyInternal(
  const nsAString& aDeviceAddress, uint32_t aPasskey,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by bluez only.
}

void
BluetoothServiceBluedroid::SetPairingConfirmationInternal(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  // Legacy method used by bluez only.
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
   * If the request is the first element of the quene, start from here. Note
   * that other request is pushed into the quene and is popped out after the
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

bool
BluetoothServiceBluedroid::IsConnected(uint16_t aProfileId)
{
  return true;
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
                                    nsIDOMBlob* aBlob,
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

//
// GATT Client
//

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

//
// Bluetooth notifications
//

/* |ProfileDeinitResultHandler| collects the results of all profile
 * result handlers and calls |Proceed| after all results handlers
 * have been run.
 */
class BluetoothServiceBluedroid::ProfileDeinitResultHandler final
  : public BluetoothProfileResultHandler
{
public:
  ProfileDeinitResultHandler(unsigned char aNumProfiles)
    : mNumProfiles(aNumProfiles)
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
    sBtInterface->Cleanup(nullptr);
  }

  unsigned char mNumProfiles;
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

  sAdapterEnabled = aState;

  if (!sAdapterEnabled) {
    static void (* const sDeinitManager[])(BluetoothProfileResultHandler*) = {
      BluetoothHfpManager::DeinitHfpInterface,
      BluetoothA2dpManager::DeinitA2dpInterface,
      BluetoothGattManager::DeinitGattInterface
    };

    // Return error if BluetoothService is unavailable
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Cleanup static adapter properties and notify adapter.
    sAdapterBdAddress.Truncate();
    sAdapterBdName.Truncate();

    InfallibleTArray<BluetoothNamedValue> props;
    BT_APPEND_NAMED_VALUE(props, "Name", sAdapterBdName);
    BT_APPEND_NAMED_VALUE(props, "Address", sAdapterBdAddress);
    if (sAdapterDiscoverable) {
      sAdapterDiscoverable = false;
      BT_APPEND_NAMED_VALUE(props, "Discoverable", false);
    }
    if (sAdapterDiscovering) {
      sAdapterDiscovering = false;
      BT_APPEND_NAMED_VALUE(props, "Discovering", false);
    }

    bs->DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         BluetoothValue(props));

    // Cleanup bluetooth interfaces after BT state becomes BT_STATE_OFF.
    nsRefPtr<ProfileDeinitResultHandler> res =
      new ProfileDeinitResultHandler(MOZ_ARRAY_LENGTH(sDeinitManager));

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sDeinitManager); ++i) {
      sDeinitManager[i](res);
    }
  }

  BluetoothService::AcknowledgeToggleBt(sAdapterEnabled);

  if (sAdapterEnabled) {
    // Bluetooth just enabled, clear profile controllers and runnable arrays.
    sControllerArray.Clear();
    sChangeDiscoveryRunnableArray.Clear();
    sSetPropertyRunnableArray.Clear();
    sGetDeviceRunnableArray.Clear();
    sFetchUuidsRunnableArray.Clear();
    sBondingRunnableArray.Clear();
    sUnbondingRunnableArray.Clear();
    sPairingNameTable.Clear();

    // Bluetooth scan mode is SCAN_MODE_CONNECTABLE by default, i.e., it should
    // be connectable and non-discoverable.
    NS_ENSURE_TRUE_VOID(sBtInterface);
    sBtInterface->SetAdapterProperty(
      BluetoothNamedValue(NS_ConvertUTF8toUTF16("Discoverable"), false),
      new SetAdapterPropertyDiscoverableResultHandler());

    // Trigger BluetoothOppManager to listen
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_LOGR("Fail to start BluetoothOppManager listening");
    }
  }

  // Resolve promise if existed
  if (!sChangeAdapterStateRunnableArray.IsEmpty()) {
    DispatchReplySuccess(sChangeAdapterStateRunnableArray[0]);
    sChangeAdapterStateRunnableArray.RemoveElementAt(0);
  }
}

/**
 * AdapterPropertiesNotification will be called after enable() but
 * before AdapterStateChangeNotification is called. At that moment,
 * BluetoothManager and BluetoothAdapter, do not register observer
 * yet.
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
      sAdapterBdAddress = p.mString;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", sAdapterBdAddress);

    } else if (p.mType == PROPERTY_BDNAME) {
      sAdapterBdName = p.mString;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", sAdapterBdName);

    } else if (p.mType == PROPERTY_ADAPTER_SCAN_MODE) {
      sAdapterDiscoverable =
        (p.mScanMode == SCAN_MODE_CONNECTABLE_DISCOVERABLE);
      BT_APPEND_NAMED_VALUE(propertiesArray, "Discoverable",
                            sAdapterDiscoverable);

    } else if (p.mType == PROPERTY_ADAPTER_BONDED_DEVICES) {
      // We have to cache addresses of bonded devices. Unlike BlueZ,
      // Bluedroid would not send another PROPERTY_ADAPTER_BONDED_DEVICES
      // event after bond completed.
      BT_LOGD("Adapter property: BONDED_DEVICES. Count: %d",
              p.mStringArray.Length());

      // Whenever reloading paired devices, force refresh
      sAdapterBondedAddressArray.Clear();
      sAdapterBondedAddressArray.AppendElements(p.mStringArray);

      BT_APPEND_NAMED_VALUE(propertiesArray, "PairedDevices",
                            sAdapterBondedAddressArray);
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
  if (!sSetPropertyRunnableArray.IsEmpty()) {
    DispatchReplySuccess(sSetPropertyRunnableArray[0]);
    sSetPropertyRunnableArray.RemoveElementAt(0);
  }
}

/**
 * RemoteDevicePropertiesNotification will be called
 *
 *   (1) automatically by Bluedroid when BT is turning on,
 *   (2) as result of GetRemoteDeviceProperties, or
 *   (3) as result of GetRemoteServices.
 */
void
BluetoothServiceBluedroid::RemoteDevicePropertiesNotification(
  BluetoothStatus aStatus, const nsAString& aBdAddr,
  int aNumProperties, const BluetoothProperty* aProperties)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  BT_APPEND_NAMED_VALUE(propertiesArray, "Address", nsString(aBdAddr));

  for (int i = 0; i < aNumProperties; ++i) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", p.mString);

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
                         nsString(aBdAddr), propertiesArray);

  // FetchUuids task
  if (!sFetchUuidsRunnableArray.IsEmpty()) {
    // propertiesArray contains Address and Uuids only
    DispatchReplySuccess(sFetchUuidsRunnableArray[0],
                         propertiesArray[1].value()); /* Uuids */
    sFetchUuidsRunnableArray.RemoveElementAt(0);
    DistributeSignal(signal);
    return;
  }

  // GetDevices task
  if (sRequestedDeviceCountArray.IsEmpty()) {
    // This is possible because the callback would be called after turning
    // Bluetooth on.
    DistributeSignal(signal);
    return;
  }

  // Use address as the index
  sRemoteDevicesPack.AppendElement(
    BluetoothNamedValue(nsString(aBdAddr), propertiesArray));

  if (--sRequestedDeviceCountArray[0] == 0) {
    if (!sGetDeviceRunnableArray.IsEmpty()) {
      DispatchReplySuccess(sGetDeviceRunnableArray[0], sRemoteDevicesPack);
      sGetDeviceRunnableArray.RemoveElementAt(0);
    }

    sRequestedDeviceCountArray.RemoveElementAt(0);
    sRemoteDevicesPack.Clear();
  }

  DistributeSignal(signal);
}

void
BluetoothServiceBluedroid::DeviceFoundNotification(
  int aNumProperties, const BluetoothProperty* aProperties)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothValue propertyValue;
  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {

    const BluetoothProperty& p = aProperties[i];

    if (p.mType == PROPERTY_BDADDR) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", p.mString);

    } else if (p.mType == PROPERTY_BDNAME) {
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", p.mString);

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

  DistributeSignal(NS_LITERAL_STRING("DeviceFound"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));
}

void
BluetoothServiceBluedroid::DiscoveryStateChangedNotification(bool aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  sAdapterDiscovering = aState;

  // Fire PropertyChanged of Discovering
  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  BT_APPEND_NAMED_VALUE(propertiesArray, "Discovering", sAdapterDiscovering);

  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Reply that Promise is resolved
  if (!sChangeDiscoveryRunnableArray.IsEmpty()) {
    DispatchReplySuccess(sChangeDiscoveryRunnableArray[0]);
    sChangeDiscoveryRunnableArray.RemoveElementAt(0);
  }
}

void
BluetoothServiceBluedroid::PinRequestNotification(const nsAString& aRemoteBdAddr,
                                                  const nsAString& aBdName,
                                                  uint32_t aCod)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", nsString(aRemoteBdAddr));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", nsString(aBdName));
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", EmptyString());
  BT_APPEND_NAMED_VALUE(propertiesArray, "type",
                        NS_LITERAL_STRING(PAIRING_REQ_TYPE_ENTERPINCODE));

  sPairingNameTable.Put(nsString(aRemoteBdAddr), nsString(aBdName));

  DistributeSignal(NS_LITERAL_STRING("PairingRequest"),
                   NS_LITERAL_STRING(KEY_PAIRING_LISTENER),
                   BluetoothValue(propertiesArray));
}

void
BluetoothServiceBluedroid::SspRequestNotification(
  const nsAString& aRemoteBdAddr, const nsAString& aBdName, uint32_t aCod,
  BluetoothSspVariant aPairingVariant, uint32_t aPasskey)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  nsAutoString passkey;
  nsAutoString pairingType;

  /**
   * Assign pairing request type and passkey based on the pairing variant.
   *
   * passkey value based on pairing request type:
   * 1) aPasskey: PAIRING_REQ_TYPE_CONFIRMATION and
   *              PAIRING_REQ_TYPE_DISPLAYPASSKEY
   * 2) empty string: PAIRING_REQ_TYPE_CONSENT
   */
  switch (aPairingVariant) {
    case SSP_VARIANT_PASSKEY_CONFIRMATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONFIRMATION);
      passkey.AppendInt(aPasskey);
      break;
    case SSP_VARIANT_PASSKEY_NOTIFICATION:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_DISPLAYPASSKEY);
      passkey.AppendInt(aPasskey);
      break;
    case SSP_VARIANT_CONSENT:
      pairingType.AssignLiteral(PAIRING_REQ_TYPE_CONSENT);
      break;
    default:
      BT_WARNING("Unhandled SSP Bonding Variant: %d", aPairingVariant);
      return;
  }

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", nsString(aRemoteBdAddr));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name", nsString(aBdName));
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", passkey);
  BT_APPEND_NAMED_VALUE(propertiesArray, "type", pairingType);

  sPairingNameTable.Put(nsString(aRemoteBdAddr), nsString(aBdName));

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
      if (!sBondingRunnableArray.IsEmpty()) {
        DispatchReplyError(sBondingRunnableArray[0], aStatus);
        sBondingRunnableArray.RemoveElementAt(0);
      }
    } else if (!sUnbondingRunnableArray.IsEmpty()) { // Active unpair failed
      // Reject unpair promise
      DispatchReplyError(sUnbondingRunnableArray[0], aStatus);
      sUnbondingRunnableArray.RemoveElementAt(0);
    }

    return;
  }

  // Retrieve and remove pairing device name from hash table
  nsString deviceName;
  bool nameExists = sPairingNameTable.Get(aRemoteBdAddr, &deviceName);
  if (nameExists) {
    sPairingNameTable.Remove(aRemoteBdAddr);
  }

  // Update bonded address array and append pairing device name
  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  nsString remoteBdAddr = nsString(aRemoteBdAddr);
  if (!bonded) {
    sAdapterBondedAddressArray.RemoveElement(remoteBdAddr);
  } else {
    if (!sAdapterBondedAddressArray.Contains(remoteBdAddr)) {
      sAdapterBondedAddressArray.AppendElement(remoteBdAddr);
    }

    // We don't assert |!deviceName.IsEmpty()| here since empty string is
    // also a valid name. According to Bluetooth Core Spec. v3.0 - Sec. 6.22,
    // "a valid Bluetooth name is a UTF-8 encoding string which is up to 248
    // bytes in length."
    MOZ_ASSERT(nameExists);
    BT_APPEND_NAMED_VALUE(propertiesArray, "Name", deviceName);
  }

  // Notify device of attribute changed
  BT_APPEND_NAMED_VALUE(propertiesArray, "Paired", bonded);
  DistributeSignal(NS_LITERAL_STRING("PropertyChanged"),
                   aRemoteBdAddr,
                   BluetoothValue(propertiesArray));

  // Notify adapter of device paired/unpaired
  BT_INSERT_NAMED_VALUE(propertiesArray, 0, "Address", remoteBdAddr);
  DistributeSignal(bonded ? NS_LITERAL_STRING(DEVICE_PAIRED_ID)
                          : NS_LITERAL_STRING(DEVICE_UNPAIRED_ID),
                   NS_LITERAL_STRING(KEY_ADAPTER),
                   BluetoothValue(propertiesArray));

  // Resolve existing pair/unpair promise
  if (bonded && !sBondingRunnableArray.IsEmpty()) {
    DispatchReplySuccess(sBondingRunnableArray[0]);
    sBondingRunnableArray.RemoveElementAt(0);
  } else if (!bonded && !sUnbondingRunnableArray.IsEmpty()) {
    DispatchReplySuccess(sUnbondingRunnableArray[0]);
    sUnbondingRunnableArray.RemoveElementAt(0);
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
