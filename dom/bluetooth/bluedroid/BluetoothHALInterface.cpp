/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHALInterface.h"
#include "BluetoothHALHelpers.h"
#include "BluetoothA2dpHALInterface.h"
#include "BluetoothAvrcpHALInterface.h"
#include "BluetoothGattHALInterface.h"
#include "BluetoothHandsfreeHALInterface.h"
#include "BluetoothSocketHALInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

template<class T>
struct interface_traits
{ };

template<>
struct interface_traits<BluetoothSocketHALInterface>
{
  typedef const btsock_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_SOCKETS_ID;
  }
};

template<>
struct interface_traits<BluetoothHandsfreeHALInterface>
{
  typedef const bthf_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_HANDSFREE_ID;
  }
};

template<>
struct interface_traits<BluetoothA2dpHALInterface>
{
  typedef const btav_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_ADVANCED_AUDIO_ID;
  }
};

#if ANDROID_VERSION >= 18
template<>
struct interface_traits<BluetoothAvrcpHALInterface>
{
  typedef const btrc_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_AV_RC_ID;
  }
};
#endif

#if ANDROID_VERSION >= 19
template<>
struct interface_traits<BluetoothGattHALInterface>
{
  typedef const btgatt_interface_t const_interface_type;

  static const char* profile_id()
  {
    return BT_PROFILE_GATT_ID;
  }
};
#endif

typedef
  BluetoothHALInterfaceRunnable0<BluetoothResultHandler, void>
  BluetoothHALResultRunnable;

typedef
  BluetoothHALInterfaceRunnable1<BluetoothResultHandler, void,
                                 BluetoothStatus, BluetoothStatus>
  BluetoothHALErrorRunnable;

static nsresult
DispatchBluetoothHALResult(BluetoothResultHandler* aRes,
                           void (BluetoothResultHandler::*aMethod)(),
                           BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRefPtr<nsRunnable> runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothHALResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothHALErrorRunnable(
      aRes, &BluetoothResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

static BluetoothNotificationHandler* sNotificationHandler;

struct BluetoothCallback
{
  class NotificationHandlerWrapper
  {
  public:
    typedef BluetoothNotificationHandler  ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationHALRunnable1<NotificationHandlerWrapper, void,
                                            bool>
    AdapterStateChangedNotification;

  typedef BluetoothNotificationHALRunnable3<NotificationHandlerWrapper, void,
                                            BluetoothStatus, int,
                                            nsAutoArrayPtr<BluetoothProperty>,
                                            BluetoothStatus, int,
                                            const BluetoothProperty*>
    AdapterPropertiesNotification;

  typedef BluetoothNotificationHALRunnable4<NotificationHandlerWrapper, void,
                                            BluetoothStatus, nsString, int,
                                            nsAutoArrayPtr<BluetoothProperty>,
                                            BluetoothStatus, const nsAString&,
                                            int, const BluetoothProperty*>
    RemoteDevicePropertiesNotification;

  typedef BluetoothNotificationHALRunnable2<NotificationHandlerWrapper, void,
                                            int,
                                            nsAutoArrayPtr<BluetoothProperty>,
                                            int, const BluetoothProperty*>
    DeviceFoundNotification;

  typedef BluetoothNotificationHALRunnable1<NotificationHandlerWrapper, void,
                                         bool>
    DiscoveryStateChangedNotification;

  typedef BluetoothNotificationHALRunnable3<NotificationHandlerWrapper, void,
                                            nsString, nsString, uint32_t,
                                            const nsAString&, const nsAString&>
    PinRequestNotification;

  typedef BluetoothNotificationHALRunnable5<NotificationHandlerWrapper, void,
                                            nsString, nsString, uint32_t,
                                            BluetoothSspVariant, uint32_t,
                                            const nsAString&, const nsAString&>
    SspRequestNotification;

  typedef BluetoothNotificationHALRunnable3<NotificationHandlerWrapper, void,
                                            BluetoothStatus, nsString,
                                            BluetoothBondState,
                                            BluetoothStatus, const nsAString&>
    BondStateChangedNotification;

  typedef BluetoothNotificationHALRunnable3<NotificationHandlerWrapper, void,
                                            BluetoothStatus, nsString, bool,
                                            BluetoothStatus, const nsAString&>
    AclStateChangedNotification;

  typedef BluetoothNotificationHALRunnable3<NotificationHandlerWrapper, void,
                                            uint16_t, nsAutoArrayPtr<uint8_t>,
                                            uint8_t, uint16_t, const uint8_t*>
    DutModeRecvNotification;

  typedef BluetoothNotificationHALRunnable2<NotificationHandlerWrapper, void,
                                            BluetoothStatus, uint16_t>
    LeTestModeNotification;

  typedef BluetoothNotificationHALRunnable1<NotificationHandlerWrapper, void,
                                            BluetoothActivityEnergyInfo,
                                            const BluetoothActivityEnergyInfo&>
    EnergyInfoNotification;

  // Bluedroid callbacks

  static const bt_property_t*
  AlignedProperties(bt_property_t* aProperties, size_t aNumProperties,
                    nsAutoArrayPtr<bt_property_t>& aPropertiesArray)
  {
    // See Bug 989976: consider aProperties address is not aligned. If
    // it is aligned, we return the pointer directly; otherwise we make
    // an aligned copy. The argument |aPropertiesArray| keeps track of
    // the memory buffer.
    if (!(reinterpret_cast<uintptr_t>(aProperties) % sizeof(void*))) {
      return aProperties;
    }

    bt_property_t* properties = new bt_property_t[aNumProperties];
    memcpy(properties, aProperties, aNumProperties * sizeof(*properties));
    aPropertiesArray = properties;

    return properties;
  }

  static void
  AdapterStateChanged(bt_state_t aStatus)
  {
    AdapterStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterStateChangedNotification,
      aStatus);
  }

  static void
  AdapterProperties(bt_status_t aStatus, int aNumProperties,
                    bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    AdapterPropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::AdapterPropertiesNotification,
      ConvertDefault(aStatus, STATUS_FAIL), aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  RemoteDeviceProperties(bt_status_t aStatus, bt_bdaddr_t* aBdAddress,
                         int aNumProperties, bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    RemoteDevicePropertiesNotification::Dispatch(
      &BluetoothNotificationHandler::RemoteDevicePropertiesNotification,
      ConvertDefault(aStatus, STATUS_FAIL), aBdAddress, aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  DeviceFound(int aNumProperties, bt_property_t* aProperties)
  {
    nsAutoArrayPtr<bt_property_t> propertiesArray;

    DeviceFoundNotification::Dispatch(
      &BluetoothNotificationHandler::DeviceFoundNotification,
      aNumProperties,
      ConvertArray<bt_property_t>(
        AlignedProperties(aProperties, aNumProperties, propertiesArray),
      aNumProperties));
  }

  static void
  DiscoveryStateChanged(bt_discovery_state_t aState)
  {
    DiscoveryStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::DiscoveryStateChangedNotification,
      aState);
  }

  static void
  PinRequest(bt_bdaddr_t* aRemoteBdAddress,
             bt_bdname_t* aRemoteBdName, uint32_t aRemoteClass)
  {
    PinRequestNotification::Dispatch(
      &BluetoothNotificationHandler::PinRequestNotification,
      aRemoteBdAddress, aRemoteBdName, aRemoteClass);
  }

  static void
  SspRequest(bt_bdaddr_t* aRemoteBdAddress, bt_bdname_t* aRemoteBdName,
             uint32_t aRemoteClass, bt_ssp_variant_t aPairingVariant,
             uint32_t aPasskey)
  {
    SspRequestNotification::Dispatch(
      &BluetoothNotificationHandler::SspRequestNotification,
      aRemoteBdAddress, aRemoteBdName, aRemoteClass,
      aPairingVariant, aPasskey);
  }

  static void
  BondStateChanged(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                   bt_bond_state_t aState)
  {
    BondStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::BondStateChangedNotification,
      aStatus, aRemoteBdAddress, aState);
  }

  static void
  AclStateChanged(bt_status_t aStatus, bt_bdaddr_t* aRemoteBdAddress,
                  bt_acl_state_t aState)
  {
    AclStateChangedNotification::Dispatch(
      &BluetoothNotificationHandler::AclStateChangedNotification,
      aStatus, aRemoteBdAddress, aState);
  }

  static void
  ThreadEvt(bt_cb_thread_evt evt)
  {
    // This callback maintains internal state and is not exported.
  }

  static void
  DutModeRecv(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen)
  {
    DutModeRecvNotification::Dispatch(
      &BluetoothNotificationHandler::DutModeRecvNotification,
      aOpcode, ConvertArray<uint8_t>(aBuf, aLen), aLen);
  }

  static void
  LeTestMode(bt_status_t aStatus, uint16_t aNumPackets)
  {
    LeTestModeNotification::Dispatch(
      &BluetoothNotificationHandler::LeTestModeNotification,
      aStatus, aNumPackets);
  }

#if ANDROID_VERSION >= 21
  static void
  EnergyInfo(bt_activity_energy_info* aEnergyInfo)
  {
    if (NS_WARN_IF(!aEnergyInfo)) {
      return;
    }

    EnergyInfoNotification::Dispatch(
      &BluetoothNotificationHandler::EnergyInfoNotification,
      *aEnergyInfo);
  }
#endif
};

#if ANDROID_VERSION >= 21
struct BluetoothOsCallout
{
  static bool
  SetWakeAlarm(uint64_t aDelayMilliseconds,
               bool aShouldWake,
               void (* aAlarmCallback)(void*),
               void* aData)
  {
    // FIXME: need to be implemented in later patches
    // HAL wants to manage an wake_alarm but Gecko cannot fulfill it for now.
    // Simply pass the request until a proper implementation has been added.
    return true;
  }

  static int
  AcquireWakeLock(const char* aLockName)
  {
    // FIXME: need to be implemented in later patches
    // HAL wants to manage an wake_lock but Gecko cannot fulfill it for now.
    // Simply pass the request until a proper implementation has been added.
    return BT_STATUS_SUCCESS;
  }

  static int
  ReleaseWakeLock(const char* aLockName)
  {
    // FIXME: need to be implemented in later patches
    // HAL wants to manage an wake_lock but Gecko cannot fulfill it for now.
    // Simply pass the request until a proper implementation has been added.
    return BT_STATUS_SUCCESS;
  }
};
#endif

// Interface
//

/* returns the container structure of a variable; _t is the container's
 * type, _v the name of the variable, and _m is _v's field within _t
 */
#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )

BluetoothHALInterface*
BluetoothHALInterface::GetInstance()
{
  static BluetoothHALInterface* sBluetoothInterface;

  if (sBluetoothInterface) {
    return sBluetoothInterface;
  }

  /* get driver module */

  const hw_module_t* module;
  int err = hw_get_module(BT_HARDWARE_MODULE_ID, &module);
  if (err) {
    BT_WARNING("hw_get_module failed: %s", strerror(err));
    return nullptr;
  }

  /* get device */

  hw_device_t* device;
  err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  if (err) {
    BT_WARNING("open failed: %s", strerror(err));
    return nullptr;
  }

  const bluetooth_device_t* bt_device =
    container(bluetooth_device_t, device, common);

  /* get interface */

  const bt_interface_t* bt_interface = bt_device->get_bluetooth_interface();
  if (!bt_interface) {
    BT_WARNING("get_bluetooth_interface failed");
    goto err_get_bluetooth_interface;
  }

  if (bt_interface->size != sizeof(*bt_interface)) {
    BT_WARNING("interface of incorrect size");
    goto err_bt_interface_size;
  }

  sBluetoothInterface = new BluetoothHALInterface(bt_interface);

  return sBluetoothInterface;

err_bt_interface_size:
err_get_bluetooth_interface:
  err = device->close(device);
  if (err) {
    BT_WARNING("close failed: %s", strerror(err));
  }
  return nullptr;
}

BluetoothHALInterface::BluetoothHALInterface(
  const bt_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHALInterface::~BluetoothHALInterface()
{ }

void
BluetoothHALInterface::Init(
  BluetoothNotificationHandler* aNotificationHandler,
  BluetoothResultHandler* aRes)
{
  static bt_callbacks_t sBluetoothCallbacks = {
    sizeof(sBluetoothCallbacks),
    BluetoothCallback::AdapterStateChanged,
    BluetoothCallback::AdapterProperties,
    BluetoothCallback::RemoteDeviceProperties,
    BluetoothCallback::DeviceFound,
    BluetoothCallback::DiscoveryStateChanged,
    BluetoothCallback::PinRequest,
    BluetoothCallback::SspRequest,
    BluetoothCallback::BondStateChanged,
    BluetoothCallback::AclStateChanged,
    BluetoothCallback::ThreadEvt,
    BluetoothCallback::DutModeRecv,
#if ANDROID_VERSION >= 18
    BluetoothCallback::LeTestMode,
#endif
#if ANDROID_VERSION >= 21
    BluetoothCallback::EnergyInfo
#endif
  };

#if ANDROID_VERSION >= 21
  static bt_os_callouts_t sBluetoothOsCallouts = {
    sizeof(sBluetoothOsCallouts),
    BluetoothOsCallout::SetWakeAlarm,
    BluetoothOsCallout::AcquireWakeLock,
    BluetoothOsCallout::ReleaseWakeLock
  };
#endif

  sNotificationHandler = aNotificationHandler;

  int status = mInterface->init(&sBluetoothCallbacks);

#if ANDROID_VERSION >= 21
  if (status == BT_STATUS_SUCCESS) {
    status = mInterface->set_os_callouts(&sBluetoothOsCallouts);
    if (status != BT_STATUS_SUCCESS) {
      mInterface->cleanup();
    }
  }
#endif

  if (aRes) {
    DispatchBluetoothHALResult(aRes, &BluetoothResultHandler::Init,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::Cleanup(BluetoothResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothHALResult(aRes, &BluetoothResultHandler::Cleanup,
                               STATUS_SUCCESS);
  }

  sNotificationHandler = nullptr;
}

void
BluetoothHALInterface::Enable(BluetoothResultHandler* aRes)
{
  int status = mInterface->enable();

  if (aRes) {
    DispatchBluetoothHALResult(aRes, &BluetoothResultHandler::Enable,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::Disable(BluetoothResultHandler* aRes)
{
  int status = mInterface->disable();

  if (aRes) {
    DispatchBluetoothHALResult(aRes, &BluetoothResultHandler::Disable,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Adapter Properties */

void
BluetoothHALInterface::GetAdapterProperties(BluetoothResultHandler* aRes)
{
  int status = mInterface->get_adapter_properties();

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetAdapterProperties,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::GetAdapterProperty(const nsAString& aName,
                                          BluetoothResultHandler* aRes)
{
  int status;
  bt_property_type_t type;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any values for aName currently */) {
    status = mInterface->get_adapter_property(type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetAdapterProperties,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::SetAdapterProperty(
  const BluetoothNamedValue& aProperty, BluetoothResultHandler* aRes)
{
  int status;
  ConvertNamedValue convertProperty(aProperty);
  bt_property_t property;

  if (NS_SUCCEEDED(Convert(convertProperty, property))) {
    status = mInterface->set_adapter_property(&property);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::SetAdapterProperty,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Remote Device Properties */

void
BluetoothHALInterface::GetRemoteDeviceProperties(
  const nsAString& aRemoteAddr, BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t addr;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, addr))) {
    status = mInterface->get_remote_device_properties(&addr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetRemoteDeviceProperties,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::GetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const nsAString& aName,
  BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_property_type_t name;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      false /* TODO: we don't support any values for aName currently */) {
    status = mInterface->get_remote_device_property(&remoteAddr, name);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetRemoteDeviceProperty,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::SetRemoteDeviceProperty(
  const nsAString& aRemoteAddr, const BluetoothNamedValue& aProperty,
  BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_property_t property;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      false /* TODO: we don't support any values for aProperty currently */) {
    status = mInterface->set_remote_device_property(&remoteAddr, &property);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::SetRemoteDeviceProperty,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Remote Services */

void
BluetoothHALInterface::GetRemoteServiceRecord(const nsAString& aRemoteAddr,
                                              const uint8_t aUuid[16],
                                              BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;
  bt_uuid_t uuid;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr)) &&
      NS_SUCCEEDED(Convert(aUuid, uuid))) {
    status = mInterface->get_remote_service_record(&remoteAddr, &uuid);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetRemoteServiceRecord,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::GetRemoteServices(const nsAString& aRemoteAddr,
                                         BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t remoteAddr;

  if (NS_SUCCEEDED(Convert(aRemoteAddr, remoteAddr))) {
    status = mInterface->get_remote_services(&remoteAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetRemoteServices,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Discovery */

void
BluetoothHALInterface::StartDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->start_discovery();

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::StartDiscovery,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::CancelDiscovery(BluetoothResultHandler* aRes)
{
  int status = mInterface->cancel_discovery();

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::CancelDiscovery,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Bonds */

void
BluetoothHALInterface::CreateBond(const nsAString& aBdAddr,
                                  BluetoothTransport aTransport,
                                  BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

#if ANDROID_VERSION >= 21
  int transport = 0; /* TRANSPORT_AUTO */

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aTransport, transport))) {
    status = mInterface->create_bond(&bdAddr, transport);
#else
  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->create_bond(&bdAddr);
#endif
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::CreateBond,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::RemoveBond(const nsAString& aBdAddr,
                                  BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->remove_bond(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::RemoveBond,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::CancelBond(const nsAString& aBdAddr,
                                  BluetoothResultHandler* aRes)
{
  bt_bdaddr_t bdAddr;
  int status;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->cancel_bond(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::CancelBond,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Connection */

void
BluetoothHALInterface::GetConnectionState(const nsAString& aBdAddr,
                                          BluetoothResultHandler* aRes)
{
  int status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->get_connection_state(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::GetConnectionState,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Authentication */

void
BluetoothHALInterface::PinReply(const nsAString& aBdAddr, bool aAccept,
                                const nsAString& aPinCode,
                                BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t bdAddr;
  uint8_t accept;
  bt_pin_code_t pinCode;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aAccept, accept)) &&
      NS_SUCCEEDED(Convert(aPinCode, pinCode))) {
    status = mInterface->pin_reply(&bdAddr, accept, aPinCode.Length(),
                                   &pinCode);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::PinReply,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::SspReply(const nsAString& aBdAddr,
                                BluetoothSspVariant aVariant,
                                bool aAccept, uint32_t aPasskey,
                                BluetoothResultHandler* aRes)
{
  int status;
  bt_bdaddr_t bdAddr;
  bt_ssp_variant_t variant;
  uint8_t accept;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aVariant, variant)) &&
      NS_SUCCEEDED(Convert(aAccept, accept))) {
    status = mInterface->ssp_reply(&bdAddr, variant, accept, aPasskey);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::SspReply,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* DUT Mode */

void
BluetoothHALInterface::DutModeConfigure(bool aEnable,
                                        BluetoothResultHandler* aRes)
{
  int status;
  uint8_t enable;

  if (NS_SUCCEEDED(Convert(aEnable, enable))) {
    status = mInterface->dut_mode_configure(enable);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::DutModeConfigure,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHALInterface::DutModeSend(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                                   BluetoothResultHandler* aRes)
{
  int status = mInterface->dut_mode_send(aOpcode, aBuf, aLen);

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::DutModeSend,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* LE Mode */

void
BluetoothHALInterface::LeTestMode(uint16_t aOpcode, uint8_t* aBuf, uint8_t aLen,
                                  BluetoothResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  int status = mInterface->le_test_mode(aOpcode, aBuf, aLen);
#else
  int status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::LeTestMode,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Energy Information */
void
BluetoothHALInterface::ReadEnergyInfo(BluetoothResultHandler* aRes)
{
#if ANDROID_VERSION >= 21
  int status = mInterface->read_energy_info();
#else
  int status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothHALResult(aRes,
                               &BluetoothResultHandler::ReadEnergyInfo,
                               ConvertDefault(status, STATUS_FAIL));
  }
}

/* Profile Interfaces */

template <class T>
T*
BluetoothHALInterface::CreateProfileInterface()
{
  typename interface_traits<T>::const_interface_type* interface =
    reinterpret_cast<typename interface_traits<T>::const_interface_type*>(
      mInterface->get_profile_interface(interface_traits<T>::profile_id()));

  if (!interface) {
    BT_WARNING("Bluetooth profile '%s' is not supported",
               interface_traits<T>::profile_id());
    return nullptr;
  }

  if (interface->size != sizeof(*interface)) {
    BT_WARNING("interface of incorrect size");
    return nullptr;
  }

  return new T(interface);
}

#if ANDROID_VERSION < 18
/*
 * Bluedroid versions that don't support AVRCP will call this function
 * to create an AVRCP interface. All interface methods will fail with
 * the error constant STATUS_UNSUPPORTED.
 */
template <>
BluetoothAvrcpHALInterface*
BluetoothHALInterface::CreateProfileInterface<BluetoothAvrcpHALInterface>()
{
  BT_WARNING("Bluetooth profile 'avrcp' is not supported");

  return new BluetoothAvrcpHALInterface();
}
#endif

#if ANDROID_VERSION < 19
/*
 * Versions that we don't support GATT will call this function
 * to create an GATT interface. All interface methods will fail with
 * the error constant STATUS_UNSUPPORTED.
 */
template <>
BluetoothGattHALInterface*
BluetoothHALInterface::CreateProfileInterface<BluetoothGattHALInterface>()
{
  BT_WARNING("Bluetooth profile 'gatt' is not supported");

  return new BluetoothGattHALInterface();
}
#endif

template <class T>
T*
BluetoothHALInterface::GetProfileInterface()
{
  static T* sBluetoothProfileInterface;

  if (sBluetoothProfileInterface) {
    return sBluetoothProfileInterface;
  }

  sBluetoothProfileInterface = CreateProfileInterface<T>();

  return sBluetoothProfileInterface;
}

BluetoothSocketInterface*
BluetoothHALInterface::GetBluetoothSocketInterface()
{
  return GetProfileInterface<BluetoothSocketHALInterface>();
}

BluetoothHandsfreeInterface*
BluetoothHALInterface::GetBluetoothHandsfreeInterface()
{
  return GetProfileInterface<BluetoothHandsfreeHALInterface>();
}

BluetoothA2dpInterface*
BluetoothHALInterface::GetBluetoothA2dpInterface()
{
  return GetProfileInterface<BluetoothA2dpHALInterface>();
}

BluetoothAvrcpInterface*
BluetoothHALInterface::GetBluetoothAvrcpInterface()
{
  return GetProfileInterface<BluetoothAvrcpHALInterface>();
}

BluetoothGattInterface*
BluetoothHALInterface::GetBluetoothGattInterface()
{
  return GetProfileInterface<BluetoothGattHALInterface>();
}

END_BLUETOOTH_NAMESPACE
