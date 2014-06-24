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

#include <hardware/hardware.h>

#include "BluetoothA2dpManager.h"
#include "BluetoothHfpManager.h"
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

#define ERR_SET_PROPERTY  "SetPropertyError"

#define ENSURE_BLUETOOTH_IS_READY(runnable, result)                    \
  do {                                                                 \
    if (!sBtInterface || !IsEnabled()) {                               \
      NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth is not ready");     \
      DispatchBluetoothReply(runnable, BluetoothValue(), errorStr);    \
      return result;                                                   \
    }                                                                  \
  } while(0)

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

// TODO: Non thread-safe static variables
static nsString sAdapterBdAddress;
static nsString sAdapterBdName;
static InfallibleTArray<nsString> sAdapterBondedAddressArray;

// Static variables below should only be used on *main thread*
static const bt_interface_t* sBtInterface;
static nsTArray<nsRefPtr<BluetoothProfileController> > sControllerArray;
static nsTArray<int> sRequestedDeviceCountArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sChangeAdapterStateRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sChangeDiscoveryRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sSetPropertyRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sGetDeviceRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sBondingRunnableArray;
static nsTArray<nsRefPtr<BluetoothReplyRunnable> > sUnbondingRunnableArray;

// Static variables below should only be used on *callback thread*


// Atomic static variables
static Atomic<bool> sAdapterDiscoverable(false);
static Atomic<bool> sAdapterDiscovering(false);
static Atomic<bool> sAdapterEnabled(false);
static Atomic<uint32_t> sAdapterDiscoverableTimeout(0);

/**
 *  Classes only used in this file
 */
class DistributeBluetoothSignalTask MOZ_FINAL : public nsRunnable
{
public:
  DistributeBluetoothSignalTask(const BluetoothSignal& aSignal) :
    mSignal(aSignal)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

    bs->DistributeSignal(mSignal);

    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class SetupAfterEnabledTask MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Bluetooth just enabled, clear profile controllers and runnable arrays.
    sControllerArray.Clear();
    sBondingRunnableArray.Clear();
    sChangeDiscoveryRunnableArray.Clear();
    sGetDeviceRunnableArray.Clear();
    sSetPropertyRunnableArray.Clear();
    sUnbondingRunnableArray.Clear();

    // Bluetooth scan mode is NONE by default
    bt_scan_mode_t mode = BT_SCAN_MODE_CONNECTABLE;
    bt_property_t prop;
    prop.type = BT_PROPERTY_ADAPTER_SCAN_MODE;
    prop.val = (void*)&mode;
    prop.len = sizeof(mode);

    NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

    int ret = sBtInterface->set_adapter_property(&prop);
    if (ret != BT_STATUS_SUCCESS) {
      BT_LOGR("Fail to set: BT_SCAN_MODE_CONNECTABLE");
    }

    // Trigger BluetoothOppManager to listen
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_LOGR("Fail to start BluetoothOppManager listening");
    }

    return NS_OK;
  }
};

class CleanupTask MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Return error if BluetoothService is unavailable
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

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

    BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                           NS_LITERAL_STRING(KEY_ADAPTER), props);
    bs->DistributeSignal(signal);

    // Cleanup bluetooth interfaces after BT state becomes BT_STATE_OFF.
    BluetoothHfpManager::DeinitHfpInterface();
    BluetoothA2dpManager::DeinitA2dpInterface();
    sBtInterface->cleanup();

    return NS_OK;
  }
};

/**
 *  Static callback functions
 */
static void
ClassToIcon(uint32_t aClass, nsAString& aRetIcon)
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

static ControlPlayStatus
PlayStatusStringToControlPlayStatus(const nsAString& aPlayStatus)
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

class AdapterStateChangedCallbackTask MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Resolve promise if existed
    if (!sChangeAdapterStateRunnableArray.IsEmpty()) {
      BluetoothValue values(true);
      DispatchBluetoothReply(sChangeAdapterStateRunnableArray[0],
                             values, EmptyString());

      sChangeAdapterStateRunnableArray.RemoveElementAt(0);
    }

    return NS_OK;
  }
};

/**
 *  Bluedroid HAL callback functions
 *
 *  Several callbacks are dispatched to main thread to avoid racing issues.
 */
static void
AdapterStateChangeCallback(bt_state_t aStatus)
{
  MOZ_ASSERT(!NS_IsMainThread());

  BT_LOGR("BT_STATE: %d", aStatus);
  sAdapterEnabled = (aStatus == BT_STATE_ON);

  if (!sAdapterEnabled &&
      NS_FAILED(NS_DispatchToMainThread(new CleanupTask()))) {
    BT_WARNING("Failed to dispatch to main thread!");
    return;
  }

  nsRefPtr<nsRunnable> runnable =
    new BluetoothService::ToggleBtAck(sAdapterEnabled);
  if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
    BT_WARNING("Failed to dispatch to main thread!");
    return;
  }

  if (sAdapterEnabled &&
      NS_FAILED(NS_DispatchToMainThread(new SetupAfterEnabledTask()))) {
    BT_WARNING("Failed to dispatch to main thread!");
    return;
  }

  // Redirect to main thread to avoid racing problem
  NS_DispatchToMainThread(new AdapterStateChangedCallbackTask());
}

class AdapterPropertiesCallbackTask MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!sSetPropertyRunnableArray.IsEmpty()) {
      DispatchBluetoothReply(sSetPropertyRunnableArray[0],
                             BluetoothValue(true), EmptyString());
      sSetPropertyRunnableArray.RemoveElementAt(0);
    }

    return NS_OK;
  }
};

/**
 * AdapterPropertiesCallback will be called after enable() but before
 * AdapterStateChangeCallback is called. At that moment, both
 * BluetoothManager/BluetoothAdapter does not register observer yet.
 */
static void
AdapterPropertiesCallback(bt_status_t aStatus, int aNumProperties,
                          bt_property_t *aProperties)
{
  MOZ_ASSERT(!NS_IsMainThread());

  BluetoothValue propertyValue;
  InfallibleTArray<BluetoothNamedValue> props;

  for (int i = 0; i < aNumProperties; i++) {
    bt_property_t p = aProperties[i];

    if (p.type == BT_PROPERTY_BDADDR) {
      BdAddressTypeToString((bt_bdaddr_t*)p.val, sAdapterBdAddress);
      propertyValue = sAdapterBdAddress;
      BT_APPEND_NAMED_VALUE(props, "Address", propertyValue);
    } else if (p.type == BT_PROPERTY_BDNAME) {
      // Construct nsCString here because Bd name returned from bluedroid
      // is missing a null terminated character after SetProperty.
      propertyValue = sAdapterBdName = NS_ConvertUTF8toUTF16(
        nsCString((char*)p.val, p.len));
      BT_APPEND_NAMED_VALUE(props, "Name", propertyValue);
    } else if (p.type == BT_PROPERTY_ADAPTER_SCAN_MODE) {
      bt_scan_mode_t newMode = *(bt_scan_mode_t*)p.val;
      propertyValue = sAdapterDiscoverable =
        (newMode == BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
      BT_APPEND_NAMED_VALUE(props, "Discoverable", propertyValue);
    } else if (p.type == BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT) {
      propertyValue = sAdapterDiscoverableTimeout = *(uint32_t*)p.val;
      BT_APPEND_NAMED_VALUE(props, "DiscoverableTimeout", propertyValue);
    } else if (p.type == BT_PROPERTY_ADAPTER_BONDED_DEVICES) {
      // We have to cache addresses of bonded devices. Unlike BlueZ,
      // bluedroid would not send an another BT_PROPERTY_ADAPTER_BONDED_DEVICES
      // event after bond completed
      bt_bdaddr_t* deviceBdAddressTypes = (bt_bdaddr_t*)p.val;
      int numOfAddresses = p.len / BLUETOOTH_ADDRESS_BYTES;
      BT_LOGD("Adapter property: BONDED_DEVICES. Count: %d", numOfAddresses);

      // Whenever reloading paired devices, force refresh
      sAdapterBondedAddressArray.Clear();

      for (int index = 0; index < numOfAddresses; index++) {
        nsAutoString deviceBdAddress;
        BdAddressTypeToString(deviceBdAddressTypes + index, deviceBdAddress);
        sAdapterBondedAddressArray.AppendElement(deviceBdAddress);
      }

      propertyValue = sAdapterBondedAddressArray;
      BT_APPEND_NAMED_VALUE(props, "Devices", propertyValue);
    } else if (p.type == BT_PROPERTY_UUIDS) {
      // FIXME: Later patchset will implement this.
      continue;
    } else {
      BT_LOGD("Unhandled adapter property type: %d", p.type);
      continue;
    }
  }

  NS_ENSURE_TRUE_VOID(props.Length() > 0);

  BluetoothValue value(props);
  BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                         NS_LITERAL_STRING(KEY_ADAPTER), value);
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }

  // Redirect to main thread to avoid racing problem
  NS_DispatchToMainThread(new AdapterPropertiesCallbackTask());
}

class RemoteDevicePropertiesCallbackTask : public nsRunnable
{
  const InfallibleTArray<BluetoothNamedValue> mProps;
  nsString mRemoteDeviceBdAddress;
public:
  RemoteDevicePropertiesCallbackTask(
    const InfallibleTArray<BluetoothNamedValue>& aProps,
    const nsAString& aRemoteDeviceBdAddress)
  : mProps(aProps)
  , mRemoteDeviceBdAddress(aRemoteDeviceBdAddress)
  { }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (sRequestedDeviceCountArray.IsEmpty()) {
      // This is possible because the callback would be called after turning
      // Bluetooth on.
      return NS_OK;
    }

    // Update to registered BluetoothDevice objects
    BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                           mRemoteDeviceBdAddress, mProps);
    nsRefPtr<DistributeBluetoothSignalTask>
      t = new DistributeBluetoothSignalTask(signal);
    if (NS_FAILED(NS_DispatchToMainThread(t))) {
      BT_WARNING("Failed to dispatch to main thread!");
      return NS_OK;
    }

    static InfallibleTArray<BluetoothNamedValue> sRemoteDevicesPack;

    // Use address as the index
    sRemoteDevicesPack.AppendElement(
      BluetoothNamedValue(mRemoteDeviceBdAddress, mProps));

    if (--sRequestedDeviceCountArray[0] == 0) {
      if (!sGetDeviceRunnableArray.IsEmpty()) {
        DispatchBluetoothReply(sGetDeviceRunnableArray[0],
                               sRemoteDevicesPack, EmptyString());
        sGetDeviceRunnableArray.RemoveElementAt(0);
      }

      sRequestedDeviceCountArray.RemoveElementAt(0);
      sRemoteDevicesPack.Clear();
    }

    return NS_OK;
  }
};

/**
 * RemoteDevicePropertiesCallback will be called, as the following conditions:
 * 1. When BT is turning on, bluedroid automatically execute this callback
 * 2. When get_remote_device_properties()
 */
static void
RemoteDevicePropertiesCallback(bt_status_t aStatus, bt_bdaddr_t *aBdAddress,
                               int aNumProperties, bt_property_t *aProperties)
{
  MOZ_ASSERT(!NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> props;

  nsString remoteDeviceBdAddress;
  BdAddressTypeToString(aBdAddress, remoteDeviceBdAddress);
  BT_APPEND_NAMED_VALUE(props, "Address", remoteDeviceBdAddress);

  for (int i = 0; i < aNumProperties; ++i) {
    bt_property_t p = aProperties[i];

    if (p.type == BT_PROPERTY_BDNAME) {
      BluetoothValue propertyValue = NS_ConvertUTF8toUTF16((char*)p.val);
      BT_APPEND_NAMED_VALUE(props, "Name", propertyValue);
    } else if (p.type == BT_PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = *(uint32_t*)p.val;
      BT_APPEND_NAMED_VALUE(props, "Class", cod);

      nsString icon;
      ClassToIcon(cod, icon);
      BT_APPEND_NAMED_VALUE(props, "Icon", icon);
    } else {
      BT_LOGD("Other non-handled device properties. Type: %d", p.type);
    }
  }

  // Redirect to main thread to avoid racing problem
  NS_DispatchToMainThread(
    new RemoteDevicePropertiesCallbackTask(props, remoteDeviceBdAddress));
}

static void
DeviceFoundCallback(int aNumProperties, bt_property_t *aProperties)
{
  MOZ_ASSERT(!NS_IsMainThread());

  BluetoothValue propertyValue;
  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {
    bt_property_t p = aProperties[i];

    if (p.type == BT_PROPERTY_BDADDR) {
      nsString remoteDeviceBdAddress;
      BdAddressTypeToString((bt_bdaddr_t*)p.val, remoteDeviceBdAddress);
      propertyValue = remoteDeviceBdAddress;

      BT_APPEND_NAMED_VALUE(propertiesArray, "Address", propertyValue);
    } else if (p.type == BT_PROPERTY_BDNAME) {
      propertyValue = NS_ConvertUTF8toUTF16((char*)p.val);
      BT_APPEND_NAMED_VALUE(propertiesArray, "Name", propertyValue);
    } else if (p.type == BT_PROPERTY_CLASS_OF_DEVICE) {
      uint32_t cod = *(uint32_t*)p.val;
      propertyValue = cod;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Class", propertyValue);

      nsString icon;
      ClassToIcon(cod, icon);
      propertyValue = icon;
      BT_APPEND_NAMED_VALUE(propertiesArray, "Icon", propertyValue);
    } else {
      BT_LOGD("Not handled remote device property: %d", p.type);
    }
  }

  BluetoothValue value = propertiesArray;
  BluetoothSignal signal(NS_LITERAL_STRING("DeviceFound"),
                         NS_LITERAL_STRING(KEY_ADAPTER), value);
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }
}

class DiscoveryStateChangedCallbackTask MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Return error if BluetoothService is unavailable
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

    // Fire PropertyChanged of Discovering
    InfallibleTArray<BluetoothNamedValue> props;
    BT_APPEND_NAMED_VALUE(props, "Discovering", sAdapterDiscovering);

    BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                           NS_LITERAL_STRING(KEY_ADAPTER), props);
    bs->DistributeSignal(signal);

    // Reply that Promise is resolved
    if (!sChangeDiscoveryRunnableArray.IsEmpty()) {
      BluetoothValue values(true);
      DispatchBluetoothReply(sChangeDiscoveryRunnableArray[0],
                             values, EmptyString());

      sChangeDiscoveryRunnableArray.RemoveElementAt(0);
    }

    return NS_OK;
  }
};

static void
DiscoveryStateChangedCallback(bt_discovery_state_t aState)
{
  MOZ_ASSERT(!NS_IsMainThread());

  sAdapterDiscovering = (aState == BT_DISCOVERY_STARTED);

  // Redirect to main thread to avoid racing problem
  NS_DispatchToMainThread(new DiscoveryStateChangedCallbackTask());
}

static void
PinRequestCallback(bt_bdaddr_t* aRemoteBdAddress,
                   bt_bdname_t* aRemoteBdName, uint32_t aRemoteClass)
{
  MOZ_ASSERT(!NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  nsAutoString remoteAddress;
  BdAddressTypeToString(aRemoteBdAddress, remoteAddress);

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", remoteAddress);
  BT_APPEND_NAMED_VALUE(propertiesArray, "method",
                        NS_LITERAL_STRING("pincode"));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name",
                        NS_ConvertUTF8toUTF16(
                          (const char*)aRemoteBdName->name));

  BluetoothValue value = propertiesArray;
  BluetoothSignal signal(NS_LITERAL_STRING("RequestPinCode"),
                         NS_LITERAL_STRING(KEY_LOCAL_AGENT), value);
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }
}

static void
SspRequestCallback(bt_bdaddr_t* aRemoteBdAddress, bt_bdname_t* aRemoteBdName,
                   uint32_t aRemoteClass, bt_ssp_variant_t aPairingVariant,
                   uint32_t aPasskey)
{
  MOZ_ASSERT(!NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> propertiesArray;
  nsAutoString remoteAddress;
  BdAddressTypeToString(aRemoteBdAddress, remoteAddress);

  BT_APPEND_NAMED_VALUE(propertiesArray, "address", remoteAddress);
  BT_APPEND_NAMED_VALUE(propertiesArray, "method",
                        NS_LITERAL_STRING("confirmation"));
  BT_APPEND_NAMED_VALUE(propertiesArray, "name",
                        NS_ConvertUTF8toUTF16(
                          (const char*)aRemoteBdName->name));
  BT_APPEND_NAMED_VALUE(propertiesArray, "passkey", aPasskey);

  BluetoothValue value = propertiesArray;
  BluetoothSignal signal(NS_LITERAL_STRING("RequestConfirmation"),
                         NS_LITERAL_STRING(KEY_LOCAL_AGENT), value);
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }
}

class BondStateChangedCallbackTask : public nsRunnable
{
  nsString mRemoteDeviceBdAddress;
  bool mBonded;
public:
  BondStateChangedCallbackTask(const nsAString& aRemoteDeviceBdAddress,
                               bool aBonded)
  : mRemoteDeviceBdAddress(aRemoteDeviceBdAddress)
  , mBonded(aBonded)
  { }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mBonded && !sBondingRunnableArray.IsEmpty()) {
      DispatchBluetoothReply(sBondingRunnableArray[0],
                             BluetoothValue(true), EmptyString());

      sBondingRunnableArray.RemoveElementAt(0);
    } else if (!mBonded && !sUnbondingRunnableArray.IsEmpty()) {
      DispatchBluetoothReply(sUnbondingRunnableArray[0],
                             BluetoothValue(true), EmptyString());

      sUnbondingRunnableArray.RemoveElementAt(0);
    }

    // Update bonding status to gaia
    InfallibleTArray<BluetoothNamedValue> propertiesArray;
    BT_APPEND_NAMED_VALUE(propertiesArray, "address", mRemoteDeviceBdAddress);
    BT_APPEND_NAMED_VALUE(propertiesArray, "status", mBonded);

    BluetoothSignal signal(NS_LITERAL_STRING(PAIRED_STATUS_CHANGED_ID),
                           NS_LITERAL_STRING(KEY_ADAPTER),
                           BluetoothValue(propertiesArray));
    NS_DispatchToMainThread(new DistributeBluetoothSignalTask(signal));

    return NS_OK;
  }
};

static void
BondStateChangedCallback(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                         bt_bond_state_t aState)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (aState == BT_BOND_STATE_BONDING) {
    // No need to handle bonding state
    return;
  }

  nsAutoString remoteBdAddress;
  BdAddressTypeToString(aRemoteBdAddress, remoteBdAddress);

  bool bonded = (aState == BT_BOND_STATE_BONDED);
  if (bonded) {
    if (sAdapterBondedAddressArray.Contains(remoteBdAddress)) {
      // See bug 940271 for more details about this case.
      return;
    }
    sAdapterBondedAddressArray.AppendElement(remoteBdAddress);
  } else {
    sAdapterBondedAddressArray.RemoveElement(remoteBdAddress);
  }

  // Update bonded address list to BluetoothAdapter
  InfallibleTArray<BluetoothNamedValue> propertiesChangeArray;
  BT_APPEND_NAMED_VALUE(propertiesChangeArray, "Devices",
                        sAdapterBondedAddressArray);

  BluetoothValue value(propertiesChangeArray);
  BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         BluetoothValue(propertiesChangeArray));
  NS_DispatchToMainThread(new DistributeBluetoothSignalTask(signal));

  // Redirect to main thread to avoid racing problem
  NS_DispatchToMainThread(
    new BondStateChangedCallbackTask(remoteBdAddress, bonded));
}

static void
AclStateChangedCallback(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                        bt_acl_state_t aState)
{
  //FIXME: This will be implemented in the later patchset
}

static void
CallbackThreadEvent(bt_cb_thread_evt evt)
{
  //FIXME: This will be implemented in the later patchset
}

bt_callbacks_t sBluetoothCallbacks =
{
  sizeof(sBluetoothCallbacks),
  AdapterStateChangeCallback,
  AdapterPropertiesCallback,
  RemoteDevicePropertiesCallback,
  DeviceFoundCallback,
  DiscoveryStateChangedCallback,
  PinRequestCallback,
  SspRequestCallback,
  BondStateChangedCallback,
  AclStateChangedCallback,
  CallbackThreadEvent
};

/**
 *  Static functions
 */
static bool
EnsureBluetoothHalLoad()
{
  hw_module_t* module;
  hw_device_t* device;

  int err = hw_get_module(BT_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
  if (err != 0) {
    BT_LOGR("Error: %s", strerror(err));
    return false;
  }
  module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  bluetooth_device_t* btDevice = (bluetooth_device_t *)device;
  NS_ENSURE_TRUE(btDevice, false);

  sBtInterface = btDevice->get_bluetooth_interface();
  NS_ENSURE_TRUE(sBtInterface, false);

  return true;
}

static bool
EnableInternal()
{
  int ret = sBtInterface->init(&sBluetoothCallbacks);
  if (ret != BT_STATUS_SUCCESS) {
    BT_LOGR("Error while setting the callbacks");
    sBtInterface = nullptr;
    return false;
  }

  // Register all the bluedroid callbacks before enable() get called
  // It is required to register a2dp callbacks before a2dp media task starts up.
  // If any interface cannot be initialized, turn on bluetooth core anyway.
  BluetoothHfpManager::InitHfpInterface();
  BluetoothA2dpManager::InitA2dpInterface();
  return sBtInterface->enable();
}

static nsresult
StartStopGonkBluetooth(bool aShouldEnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(sBtInterface, NS_ERROR_FAILURE);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

  if (bs->IsEnabled() == aShouldEnable) {
    // Keep current enable status
    nsRefPtr<nsRunnable> runnable =
      new BluetoothService::ToggleBtAck(aShouldEnable);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
    return NS_OK;
  }

  int ret = aShouldEnable ? EnableInternal() : sBtInterface->disable();
  NS_ENSURE_TRUE(ret == BT_STATUS_SUCCESS, NS_ERROR_FAILURE);

  return NS_OK;
}

static void
ReplyStatusError(BluetoothReplyRunnable* aBluetoothReplyRunnable,
                 int aStatusCode, const nsAString& aCustomMsg)
{
  MOZ_ASSERT(aBluetoothReplyRunnable, "Reply runnable is nullptr");

  BT_LOGR("error code(%d)", aStatusCode);

  nsAutoString replyError;
  replyError.Assign(aCustomMsg);

  if (aStatusCode == BT_STATUS_BUSY) {
    replyError.AppendLiteral(":BT_STATUS_BUSY");
  } else if (aStatusCode == BT_STATUS_NOT_READY) {
    replyError.AppendLiteral(":BT_STATUS_NOT_READY");
  } else if (aStatusCode == BT_STATUS_DONE) {
    replyError.AppendLiteral(":BT_STATUS_DONE");
  } else if (aStatusCode == BT_STATUS_AUTH_FAILURE) {
    replyError.AppendLiteral(":BT_STATUS_AUTH_FAILURE");
  } else if (aStatusCode == BT_STATUS_RMT_DEV_DOWN) {
    replyError.AppendLiteral(":BT_STATUS_RMT_DEV_DOWN");
  } else if (aStatusCode == BT_STATUS_FAIL) {
    replyError.AppendLiteral(":BT_STATUS_FAIL");
  }

  DispatchBluetoothReply(aBluetoothReplyRunnable, BluetoothValue(true),
                         replyError);
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

  nsresult ret = StartStopGonkBluetooth(true);
  if (NS_FAILED(ret)) {
    nsRefPtr<nsRunnable> runnable =
      new BluetoothService::ToggleBtAck(false);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
    BT_LOGR("Error");
  }

  return ret;
}

nsresult
BluetoothServiceBluedroid::StopInternal(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // aRunnable will be a nullptr during starup and shutdown
  if(aRunnable) {
    sChangeAdapterStateRunnableArray.AppendElement(aRunnable);
  }

  nsresult ret = StartStopGonkBluetooth(false);
  if (NS_FAILED(ret)) {
    nsRefPtr<nsRunnable> runnable =
      new BluetoothService::ToggleBtAck(true);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
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

    BT_APPEND_NAMED_VALUE(adaptersProperties.get_ArrayOfBluetoothNamedValue(),
                          "Adapter", properties);
  }

  DispatchBluetoothReply(aRunnable, adaptersProperties, EmptyString());
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetConnectedDevicePropertiesInternal(
  uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  BluetoothProfileManagerBase* profile =
    BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    InfallibleTArray<BluetoothNamedValue> emptyArr;
    DispatchBluetoothReply(aRunnable, emptyArr,
                           NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
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
    DispatchBluetoothReply(aRunnable, emptyArr, EmptyString());
    return NS_OK;
  }

  for (int i = 0; i < requestedDeviceCount; i++) {
    // Retrieve all properties of devices
    bt_bdaddr_t addressType;
    StringToBdAddressType(deviceAddresses[i], &addressType);

    int ret = sBtInterface->get_remote_device_properties(&addressType);
    if (ret != BT_STATUS_SUCCESS) {
      DispatchBluetoothReply(aRunnable, BluetoothValue(true),
                             NS_LITERAL_STRING("GetConnectedDeviceFailed"));
      return NS_OK;
    }
  }

  sRequestedDeviceCountArray.AppendElement(requestedDeviceCount);
  sGetDeviceRunnableArray.AppendElement(aRunnable);

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
    InfallibleTArray<BluetoothNamedValue> emptyArr;
    DispatchBluetoothReply(aRunnable, BluetoothValue(emptyArr), EmptyString());
    return NS_OK;
  }

  for (int i = 0; i < requestedDeviceCount; i++) {
    // Retrieve all properties of devices
    bt_bdaddr_t addressType;
    StringToBdAddressType(aDeviceAddress[i], &addressType);
    int ret = sBtInterface->get_remote_device_properties(&addressType);
    if (ret != BT_STATUS_SUCCESS) {
      DispatchBluetoothReply(aRunnable, BluetoothValue(true),
                             NS_LITERAL_STRING("GetPairedDeviceFailed"));
      return NS_OK;
    }
  }

  sRequestedDeviceCountArray.AppendElement(requestedDeviceCount);
  sGetDeviceRunnableArray.AppendElement(aRunnable);

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  int ret = sBtInterface->start_discovery();
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret, NS_LITERAL_STRING("StartDiscovery"));

    return NS_OK;
  }

  sChangeDiscoveryRunnableArray.AppendElement(aRunnable);
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::StopDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  int ret = sBtInterface->cancel_discovery();
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret, NS_LITERAL_STRING("StopDiscovery"));
    return NS_OK;
  }

  sChangeDiscoveryRunnableArray.AppendElement(aRunnable);

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::SetProperty(BluetoothObjectType aType,
                                       const BluetoothNamedValue& aValue,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  const nsString propName = aValue.name();
  bt_property_t prop;
  bt_scan_mode_t scanMode;
  nsCString str;

  // For Bluedroid, it's necessary to check property name for SetProperty
  if (propName.EqualsLiteral("Name")) {
    prop.type = BT_PROPERTY_BDNAME;
  } else if (propName.EqualsLiteral("Discoverable")) {
    prop.type = BT_PROPERTY_ADAPTER_SCAN_MODE;
  } else if (propName.EqualsLiteral("DiscoverableTimeout")) {
    prop.type = BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
  } else {
    BT_LOGR("Warning: Property type is not supported yet, type: %s",
            NS_ConvertUTF16toUTF8(propName).get());
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_SET_PROPERTY));
    return NS_OK;
  }

  if (aValue.value().type() == BluetoothValue::Tuint32_t) {
    // Set discoverable timeout
    prop.val = (void*)aValue.value().get_uint32_t();
  } else if (aValue.value().type() == BluetoothValue::TnsString) {
    // Set name
    str = NS_ConvertUTF16toUTF8(aValue.value().get_nsString());
    const char* name = str.get();
    prop.val = (void*)name;
    prop.len = strlen(name);
  } else if (aValue.value().type() == BluetoothValue::Tbool) {
    scanMode = aValue.value().get_bool() ?
                 BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE :
                 BT_SCAN_MODE_CONNECTABLE;

    prop.val = (void*)&scanMode;
    prop.len = sizeof(scanMode);
  } else {
    BT_LOGR("SetProperty but the property cannot be recognized correctly.");
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_SET_PROPERTY));
    return NS_OK;
  }

  sSetPropertyRunnableArray.AppendElement(aRunnable);

  int ret = sBtInterface->set_adapter_property(&prop);
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret, NS_LITERAL_STRING(ERR_SET_PROPERTY));
    sSetPropertyRunnableArray.RemoveElement(aRunnable);
  }

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

nsresult
BluetoothServiceBluedroid::CreatePairedDeviceInternal(
  const nsAString& aDeviceAddress, int aTimeout,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(aDeviceAddress, &remoteAddress);

  int ret = sBtInterface->create_bond(&remoteAddress);
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret, NS_LITERAL_STRING("CreatedPairedDevice"));
  } else {
    sBondingRunnableArray.AppendElement(aRunnable);
  }

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::RemoveDeviceInternal(
  const nsAString& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, NS_OK);

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(aDeviceAddress, &remoteAddress);

  int ret = sBtInterface->remove_bond(&remoteAddress);
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret,
                     NS_LITERAL_STRING("RemoveDevice"));
  } else {
    sUnbondingRunnableArray.AppendElement(aRunnable);
  }

  return NS_OK;
}

bool
BluetoothServiceBluedroid::SetPinCodeInternal(
  const nsAString& aDeviceAddress, const nsAString& aPinCode,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, false);

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(aDeviceAddress, &remoteAddress);

  int ret = sBtInterface->pin_reply(
      &remoteAddress, true, aPinCode.Length(),
      (bt_pin_code_t*)NS_ConvertUTF16toUTF8(aPinCode).get());

  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret, NS_LITERAL_STRING("SetPinCode"));
  } else {
    DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
  }

  return true;
}

bool
BluetoothServiceBluedroid::SetPasskeyInternal(
  const nsAString& aDeviceAddress, uint32_t aPasskey,
  BluetoothReplyRunnable* aRunnable)
{
  return true;
}

bool
BluetoothServiceBluedroid::SetPairingConfirmationInternal(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_BLUETOOTH_IS_READY(aRunnable, false);

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(aDeviceAddress, &remoteAddress);

  int ret = sBtInterface->ssp_reply(&remoteAddress, (bt_ssp_variant_t)0,
                                    aConfirm, 0);
  if (ret != BT_STATUS_SUCCESS) {
    ReplyStatusError(aRunnable, ret,
                     NS_LITERAL_STRING("SetPairingConfirmation"));
  } else {
    DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
  }

  return true;
}

static void
NextBluetoothProfileController()
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

static void
ConnectDisconnect(bool aConnect, const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable,
                  uint16_t aServiceUuid, uint32_t aCod = 0)
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

const bt_interface_t*
BluetoothServiceBluedroid::GetBluetoothInterface()
{
  return sBtInterface;
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
  nsAutoString errorStr;
  if (!opp || !opp->SendFile(aDeviceAddress, aBlobParent)) {
    errorStr.AssignLiteral("Calling SendFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
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
  nsAutoString errorStr;
  if (!opp || !opp->SendFile(aDeviceAddress, aBlob)) {
    errorStr.AssignLiteral("Calling SendFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
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
    errorStr.AssignLiteral("Calling StopSendingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothServiceBluedroid::ConfirmReceivingFile(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be called from main thread!");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->ConfirmReceivingFile(aConfirm)) {
    errorStr.AssignLiteral("Calling ConfirmReceivingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothServiceBluedroid::ConnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->ConnectSco()) {
    NS_NAMED_LITERAL_STRING(replyError, "Calling ConnectSco() failed");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
    return;
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}

void
BluetoothServiceBluedroid::DisconnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->DisconnectSco()) {
    NS_NAMED_LITERAL_STRING(replyError, "Calling DisconnectSco() failed");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
    return;
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}

void
BluetoothServiceBluedroid::IsScoConnected(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp) {
    NS_NAMED_LITERAL_STRING(replyError, "Fail to get BluetoothHfpManager");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
    return;
  }

  DispatchBluetoothReply(aRunnable, hfp->IsScoConnected(), EmptyString());
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
  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
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
  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
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

