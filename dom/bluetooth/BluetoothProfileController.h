/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothprofilecontroller_h__
#define mozilla_dom_bluetooth_bluetoothprofilecontroller_h__

#include "BluetoothUuid.h"
#include "mozilla/RefPtr.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"

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

// Audio: Major service class = 0x100 (Bit 21 is set)
#define HAS_AUDIO(cod)               (cod & 0x200000)

// Rendering: Major service class = 0x20 (Bit 18 is set)
#define HAS_RENDERING(cod)           (cod & 0x40000)

// Peripheral: Major device class = 0x5
#define IS_PERIPHERAL(cod)           (GET_MAJOR_DEVICE_CLASS(cod) == 0x5)

// Remote Control: sub-field of minor device class, Bit 5 ~ Bit 2 = 0x3
#define IS_REMOTE_CONTROL(cod)       ((GET_MINOR_DEVICE_CLASS(cod) & 0xf) == 0x3)

// Keyboard: sub-field of minor device class (Bit 6)
#define IS_KEYBOARD(cod)             ((GET_MINOR_DEVICE_CLASS(cod) & 0x10) >> 4)

// Pointing device: sub-field of minor device class (Bit 7)
#define IS_POINTING_DEVICE(cod)      ((GET_MINOR_DEVICE_CLASS(cod) & 0x20) >> 5)

class BluetoothProfileManagerBase;
class BluetoothReplyRunnable;
typedef void (*BluetoothProfileControllerCallback)();

class BluetoothProfileController : public RefCounted<BluetoothProfileController>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(BluetoothProfileController)
  /**
   * @param aConnect:       If it's a connect request, the value should be set
   *                        to true. For disconnect request, set it to false.
   * @param aDeviceAddress: The address of remote device.
   * @param aRunnable:      Once the controller has done, the runnable will be
   *                        replied. When all connection/disconnection attemps
   *                        have failed, an error is fired. In other words,
   *                        reply a success if any attemp successes.
   * @param aCallback:      The callback will be invoked after the runnable is
   *                        replied.
   * @param aServiceUuid:   Connect/Disconnect to the specified profile. Please
   *                        see enum BluetoothServiceClass for valid value.
   * @param aCod:           If aServiceUuid is not assigned, i.e. the value is
   *                        0, the controller connect multiple profiles based on
   *                        aCod or disconnect all connected profiles.
   */
  BluetoothProfileController(bool aConnect,
                             const nsAString& aDeviceAddress,
                             BluetoothReplyRunnable* aRunnable,
                             BluetoothProfileControllerCallback aCallback,
                             uint16_t aServiceUuid,
                             uint32_t aCod = 0);
  ~BluetoothProfileController();

  /**
   * The controller starts connecting/disconnecting profiles one by one
   * according to the order in array mProfiles.
   */
  void StartSession();

  /**
   * The original DOM request would be fired in this function.
   */
  void EndSession();

  /**
   * It would be invoked after connect/disconnect operation is completed.
   * An error string would be returned when it fails.
   */
  void NotifyCompletion(const nsAString& aErrorStr);

  /**
   * It is invoked after a profile has reached timeout, reset mProfiles.
   */
  void GiveupAndContinue();

private:
  // Setup data member mProfiles
  void SetupProfiles(bool aAssignServiceClass);

  // Add profiles into array with/without checking connection status
  void AddProfile(BluetoothProfileManagerBase* aProfile,
                  bool aCheckConnected = false);

  // Add specified profile into array
  void AddProfileWithServiceClass(BluetoothServiceClass aClass);

  // Connect/Disconnect next profile in the array
  void Next();

  const bool mConnect;
  nsString mDeviceAddress;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  BluetoothProfileControllerCallback mCallback;

  bool mCurrentProfileFinished;
  bool mSuccess;
  int8_t mProfilesIndex;
  nsTArray<BluetoothProfileManagerBase*> mProfiles;

  // Either CoD or BluetoothServiceClass is assigned.
  union {
    uint32_t cod;
    BluetoothServiceClass service;
  } mTarget;

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimerCallback> mCheckProfileStatusCallback;
};

END_BLUETOOTH_NAMESPACE

#endif
