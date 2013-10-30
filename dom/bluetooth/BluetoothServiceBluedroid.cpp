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

#include <hardware/bluetooth.h>
#include <hardware/hardware.h>

#include "BluetoothReplyRunnable.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/UnixSocket.h"

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

typedef char bdstr_t[18];

/**
 *  Classes only used in this file
 */
class DistributeBluetoothSignalTask : public nsRunnable {
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
    bs->DistributeSignal(mSignal);

    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

/**
 *  Static variables
 */

static bluetooth_device_t* sBtDevice;
static const bt_interface_t* sBtInterface;
static bool sIsBtEnabled = false;
static bool sAdapterDiscoverable = false;
static nsString sAdapterBdAddress;
static nsString sAdapterBdName;
static uint32_t sAdapterDiscoverableTimeout;

/**
 *  Static callback functions
 */
static void
AdapterStateChangeCallback(bt_state_t aStatus)
{
  MOZ_ASSERT(!NS_IsMainThread());

  BT_LOGD("%s, BT_STATE:%d", __FUNCTION__, aStatus);
  nsAutoString signalName;
  if (aStatus == BT_STATE_ON) {
    sIsBtEnabled = true;
    signalName = NS_LITERAL_STRING("AdapterAdded");
  } else {
    sIsBtEnabled = false;
    signalName = NS_LITERAL_STRING("Disabled");
  }

  BluetoothSignal signal(signalName, NS_LITERAL_STRING(KEY_MANAGER), BluetoothValue(true));
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    NS_WARNING("Failed to dispatch to main thread!");
  }
}

static void
BdAddressTypeToString(bt_bdaddr_t* aBdAddressType, nsAString& aRetBdAddress)
{
  uint8_t* addr = aBdAddressType->address;
  bdstr_t bdstr;

  sprintf((char*)bdstr, "%02x:%02x:%02x:%02x:%02x:%02x",
          (int)addr[0],(int)addr[1],(int)addr[2],
          (int)addr[3],(int)addr[4],(int)addr[5]);

  aRetBdAddress = NS_ConvertUTF8toUTF16((char*)bdstr);
}

static void
AdapterPropertiesChangeCallback(bt_status_t aStatus, int aNumProperties,
                                bt_property_t *aProperties)
{
  MOZ_ASSERT(!NS_IsMainThread());

  BluetoothValue propertyValue;
  InfallibleTArray<BluetoothNamedValue> propertiesArray;

  for (int i = 0; i < aNumProperties; i++) {
    bt_property_t p = aProperties[i];

    if (p.type == BT_PROPERTY_BDADDR) {
      BdAddressTypeToString((bt_bdaddr_t*)p.val, sAdapterBdAddress);
      propertyValue = sAdapterBdAddress;
      propertiesArray.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Address"), propertyValue));
    } else if (p.type == BT_PROPERTY_BDNAME) {
      // Construct nsCString here because Bd name returned from bluedroid
      // is missing a null terminated character after SetProperty.
      propertyValue = sAdapterBdName = NS_ConvertUTF8toUTF16(
        nsCString((char*)p.val, p.len));
      propertiesArray.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Name"), propertyValue));
    } else if (p.type == BT_PROPERTY_ADAPTER_SCAN_MODE) {
      propertyValue = sAdapterDiscoverable = *(uint32_t*)p.val;
      propertiesArray.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Discoverable"), propertyValue));
    } else if (p.type == BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT) {
      propertyValue = sAdapterDiscoverableTimeout = *(uint32_t*)p.val;
      propertiesArray.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("DiscoverableTimeout"),
                            propertyValue));
    } else if (p.type == BT_PROPERTY_ADAPTER_BONDED_DEVICES) {
      //FIXME: This will be implemented in the later patchset
      return;
    } else if (p.type == BT_PROPERTY_UUIDS) {
      //FIXME: This will be implemented in the later patchset
      return;
    } else {
      BT_LOGR("Unhandled adapter property type: %d", p.type);
      return;
    }

    BluetoothValue value(propertiesArray);
    BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                           NS_LITERAL_STRING(KEY_ADAPTER), value);
    nsRefPtr<DistributeBluetoothSignalTask>
      t = new DistributeBluetoothSignalTask(signal);
    if (NS_FAILED(NS_DispatchToMainThread(t))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }
}

bt_callbacks_t sBluetoothCallbacks = {
  sizeof(sBluetoothCallbacks),
  AdapterStateChangeCallback,
  AdapterPropertiesChangeCallback
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
    BT_LOGR("Error: %s ", strerror(err));
    return false;
  }
  module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  sBtDevice = (bluetooth_device_t *)device;
  sBtInterface = sBtDevice->get_bluetooth_interface();
  BT_LOGD("Bluetooth HAL loaded");

  return true;
}

static nsresult
StartStopGonkBluetooth(bool aShouldEnable)
{
  MOZ_ASSERT(!NS_IsMainThread());

  static bool sIsBtInterfaceInitialized = false;

  if (!EnsureBluetoothHalLoad()) {
    BT_LOGR("Failed to load bluedroid library.\n");
    return NS_ERROR_FAILURE;
  }

  if (sIsBtEnabled == aShouldEnable)
    return NS_OK;

  if (sBtInterface && !sIsBtInterfaceInitialized) {
    int ret = sBtInterface->init(&sBluetoothCallbacks);
    if (ret != BT_STATUS_SUCCESS) {
      BT_LOGR("Error while setting the callbacks %s", __FUNCTION__);
      sBtInterface = nullptr;
      return NS_ERROR_FAILURE;
    }
    sIsBtInterfaceInitialized = true;
  }
  int ret = aShouldEnable ? sBtInterface->enable() : sBtInterface->disable();

  return (ret == BT_STATUS_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;
}

/**
 *  Member functions
 */
nsresult
BluetoothServiceBluedroid::StartInternal()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult ret = StartStopGonkBluetooth(true);
  if (NS_FAILED(ret)) {
    BT_LOGR("Error: %s", __FUNCTION__);
  }

  return ret;
}

nsresult
BluetoothServiceBluedroid::StopInternal()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult ret = StartStopGonkBluetooth(false);
  if (NS_FAILED(ret)) {
    BT_LOGR("Error: %s", __FUNCTION__);
  }

  return ret;
}

bool
BluetoothServiceBluedroid::IsEnabledInternal()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!EnsureBluetoothHalLoad()) {
    NS_ERROR("Failed to load bluedroid library.\n");
    return false;
  }

  return sIsBtEnabled;
}

nsresult
BluetoothServiceBluedroid::GetDefaultAdapterPathInternal(
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<BluetoothReplyRunnable> runnable(aRunnable);

  BluetoothValue v = InfallibleTArray<BluetoothNamedValue>();
  v.get_ArrayOfBluetoothNamedValue().AppendElement(
    BluetoothNamedValue(NS_LITERAL_STRING("Name"), sAdapterBdName));

  nsAutoString replyError;
  DispatchBluetoothReply(runnable.get(), v, replyError);

  runnable.forget();

  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetConnectedDevicePropertiesInternal(
  uint16_t aProfileId, BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;

}

nsresult
BluetoothServiceBluedroid::GetPairedDevicePropertiesInternal(
  const nsTArray<nsString>& aDeviceAddress, BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::StartDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::StopDiscoveryInternal(
  BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::GetDevicePropertiesInternal(
  const BluetoothSignal& aSignal)
{
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::SetProperty(BluetoothObjectType aType,
                                       const BluetoothNamedValue& aValue,
                                       BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;
}

bool
BluetoothServiceBluedroid::GetDevicePath(const nsAString& aAdapterPath,
                                         const nsAString& aDeviceAddress,
                                         nsAString& aDevicePath)
{
  return true;
}

bool
BluetoothServiceBluedroid::AddServiceRecords(const char* serviceName,
                                             unsigned long long uuidMsb,
                                             unsigned long long uuidLsb,
                                             int channel)
{
  return true;
}

bool
BluetoothServiceBluedroid::RemoveServiceRecords(const char* serviceName,
                                                unsigned long long uuidMsb,
                                                unsigned long long uuidLsb,
                                                int channel)
{
  return true;
}

bool
BluetoothServiceBluedroid::AddReservedServicesInternal(
  const nsTArray<uint32_t>& aServices,
  nsTArray<uint32_t>& aServiceHandlesContainer)
{
  return true;

}

bool
BluetoothServiceBluedroid::RemoveReservedServicesInternal(
  const nsTArray<uint32_t>& aServiceHandles)
{
  return true;
}

nsresult
BluetoothServiceBluedroid::GetScoSocket(
  const nsAString& aObjectPath, bool aAuth, bool aEncrypt,
  mozilla::ipc::UnixSocketConsumer* aConsumer)
{
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
  return NS_OK;
}

nsresult
BluetoothServiceBluedroid::RemoveDeviceInternal(
  const nsAString& aDeviceObjectPath,
  BluetoothReplyRunnable* aRunnable)
{
  return NS_OK;
}

bool
BluetoothServiceBluedroid::SetPinCodeInternal(
  const nsAString& aDeviceAddress, const nsAString& aPinCode,
  BluetoothReplyRunnable* aRunnable)
{
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
  return true;
}

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

void
BluetoothServiceBluedroid::Connect(const nsAString& aDeviceAddress,
                                   uint32_t aCod,
                                   uint16_t aServiceUuid,
                                   BluetoothReplyRunnable* aRunnable)
{

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

}

void
BluetoothServiceBluedroid::SendFile(const nsAString& aDeviceAddress,
                                    BlobParent* aBlobParent,
                                    BlobChild* aBlobChild,
                                    BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::StopSendingFile(const nsAString& aDeviceAddress,
                                           BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::ConfirmReceivingFile(
  const nsAString& aDeviceAddress, bool aConfirm,
  BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::ConnectSco(BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::DisconnectSco(BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::IsScoConnected(BluetoothReplyRunnable* aRunnable)
{

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

}

void
BluetoothServiceBluedroid::SendPlayStatus(
  int64_t aDuration, int64_t aPosition,
  const nsAString& aPlayStatus,
  BluetoothReplyRunnable* aRunnable)
{

}

void
BluetoothServiceBluedroid::UpdatePlayStatus(
  uint32_t aDuration, uint32_t aPosition, ControlPlayStatus aPlayStatus)
{

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

