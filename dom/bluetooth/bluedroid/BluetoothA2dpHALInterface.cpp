/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothA2dpHALInterface.h"
#include "BluetoothHALHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

typedef
  BluetoothHALInterfaceRunnable0<BluetoothA2dpResultHandler, void>
  BluetoothA2dpHALResultRunnable;

typedef
  BluetoothHALInterfaceRunnable1<BluetoothA2dpResultHandler, void,
                                 BluetoothStatus, BluetoothStatus>
  BluetoothA2dpHALErrorRunnable;

static nsresult
DispatchBluetoothA2dpHALResult(
  BluetoothA2dpResultHandler* aRes,
  void (BluetoothA2dpResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRefPtr<nsRunnable> runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothA2dpHALResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothA2dpHALErrorRunnable(aRes,
      &BluetoothA2dpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

static BluetoothA2dpNotificationHandler* sA2dpNotificationHandler;

struct BluetoothA2dpHALCallback
{
  class A2dpNotificationHandlerWrapper
  {
  public:
    typedef BluetoothA2dpNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sA2dpNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationHALRunnable2<
    A2dpNotificationHandlerWrapper, void,
    BluetoothA2dpConnectionState, nsString,
    BluetoothA2dpConnectionState, const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationHALRunnable2<
    A2dpNotificationHandlerWrapper, void,
    BluetoothA2dpAudioState, nsString,
    BluetoothA2dpAudioState, const nsAString&>
    AudioStateNotification;

  typedef BluetoothNotificationHALRunnable3<
    A2dpNotificationHandlerWrapper, void,
    nsString, uint32_t, uint8_t,
    const nsAString&, uint32_t, uint8_t>
    AudioConfigNotification;

  // Bluedroid A2DP callbacks

  static void
  ConnectionState(btav_connection_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    ConnectionStateNotification::Dispatch(
      &BluetoothA2dpNotificationHandler::ConnectionStateNotification,
      aState, aBdAddr);
  }

  static void
  AudioState(btav_audio_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    AudioStateNotification::Dispatch(
      &BluetoothA2dpNotificationHandler::AudioStateNotification,
      aState, aBdAddr);
  }

#if ANDROID_VERSION >= 21
  static void
  AudioConfig(bt_bdaddr_t *aBdAddr, uint32_t aSampleRate, uint8_t aChannelCount)
  {
    AudioConfigNotification::Dispatch(
      &BluetoothA2dpNotificationHandler::AudioConfigNotification,
      aBdAddr, aSampleRate, aChannelCount);
  }
#endif
};

// Interface
//

BluetoothA2dpHALInterface::BluetoothA2dpHALInterface(
  const btav_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothA2dpHALInterface::~BluetoothA2dpHALInterface()
{ }

void
BluetoothA2dpHALInterface::Init(
  BluetoothA2dpNotificationHandler* aNotificationHandler,
  BluetoothA2dpResultHandler* aRes)
{
  static btav_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
    BluetoothA2dpHALCallback::ConnectionState,
    BluetoothA2dpHALCallback::AudioState,
#if ANDROID_VERSION >= 21
    BluetoothA2dpHALCallback::AudioConfig
#endif
  };

  sA2dpNotificationHandler = aNotificationHandler;

  bt_status_t status = mInterface->init(&sCallbacks);

  if (aRes) {
    DispatchBluetoothA2dpHALResult(aRes,
                                   &BluetoothA2dpResultHandler::Init,
                                   ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothA2dpHALInterface::Cleanup(BluetoothA2dpResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothA2dpHALResult(aRes,
                                   &BluetoothA2dpResultHandler::Cleanup,
                                   STATUS_SUCCESS);
  }
}

void
BluetoothA2dpHALInterface::Connect(const nsAString& aBdAddr,
                                   BluetoothA2dpResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothA2dpHALResult(
      aRes, &BluetoothA2dpResultHandler::Connect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothA2dpHALInterface::Disconnect(const nsAString& aBdAddr,
                                      BluetoothA2dpResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothA2dpHALResult(
      aRes, &BluetoothA2dpResultHandler::Disconnect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

END_BLUETOOTH_NAMESPACE
