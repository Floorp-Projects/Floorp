/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothprofilecontroller_h__
#define mozilla_dom_bluetooth_bluetoothprofilecontroller_h__

#include "BluetoothUuid.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"

BEGIN_BLUETOOTH_NAMESPACE

/*
 * Class of Device(CoD): 32-bit unsigned integer
 *
 *  31   24  23    13 12     8 7      2 1 0
 * |       | Major   | Major  | Minor  |   |
 * |       | service | device | device |   |
 * |       | class   | class  | class  |   |
 * |       |<- 11  ->|<- 5  ->|<- 6  ->|   |
 *
 * https://www.bluetooth.org/en-us/specification/assigned-numbers/baseband
 */

// Bit 23 ~ Bit 13: Major service class
#define GET_MAJOR_SERVICE_CLASS(cod) ((cod & 0xffe000) >> 13)

// Bit 12 ~ Bit 8: Major device class
#define GET_MAJOR_DEVICE_CLASS(cod)  ((cod & 0x1f00) >> 8)

// Bit 7 ~ Bit 2: Minor device class
#define GET_MINOR_DEVICE_CLASS(cod)  ((cod & 0xfc) >> 2)

// Bit 21: Major service class = 0x100, Audio
#define HAS_AUDIO(cod)               (cod & 0x200000)

// Bit 20: Major service class = 0x80, Object Transfer
#define HAS_OBJECT_TRANSFER(cod)     (cod & 0x100000)

// Bit 18: Major service class = 0x20, Rendering
#define HAS_RENDERING(cod)           (cod & 0x40000)

// Major device class = 0xA, Peripheral
#define IS_PERIPHERAL(cod)           (GET_MAJOR_DEVICE_CLASS(cod) == 0xa)

// Major device class = 0x4, Audio/Video
// Minor device class = 0x1, Wearable Headset device
#define IS_HEADSET(cod)              ((GET_MAJOR_SERVICE_CLASS(cod) == 0x4) && \
                                     (GET_MINOR_DEVICE_CLASS(cod) == 0x1))

class BluetoothProfileManagerBase;
class BluetoothReplyRunnable;
typedef void (*BluetoothProfileControllerCallback)();

class BluetoothProfileController : public RefCounted<BluetoothProfileController>
{
public:
  BluetoothProfileController(const nsAString& aDeviceAddress,
                             BluetoothReplyRunnable* aRunnable,
                             BluetoothProfileControllerCallback aCallback);
  ~BluetoothProfileController();

  // Connect to a specific service UUID.
  void Connect(BluetoothServiceClass aClass);

  // Based on the CoD, connect to multiple profiles sequencely.
  void Connect(uint32_t aCod);

  /**
   * If aClass is assigned with specific service class, disconnect its
   * corresponding profile. Otherwise, disconnect all profiles connected to the
   * remote device.
   */
  void Disconnect(BluetoothServiceClass aClass = BluetoothServiceClass::UNKNOWN);

  void OnConnect(const nsAString& aErrorStr);
  void OnDisconnect(const nsAString& aErrorStr);

  uint32_t GetCod() const
  {
    return mCod;
  }

private:
  void ConnectNext();
  void DisconnectNext();
  bool AddProfile(BluetoothProfileManagerBase* aProfile,
                  bool aCheckConnected = false);
  bool AddProfileWithServiceClass(BluetoothServiceClass aClass);

  int8_t mProfilesIndex;
  nsTArray<BluetoothProfileManagerBase*> mProfiles;

  BluetoothProfileControllerCallback mCallback;
  uint32_t mCod;
  nsString mDeviceAddress;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  bool mSuccess;
};

END_BLUETOOTH_NAMESPACE

#endif
