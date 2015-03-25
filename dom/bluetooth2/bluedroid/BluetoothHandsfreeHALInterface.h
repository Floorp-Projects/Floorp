/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothhandsfreehalinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothhandsfreehalinterface_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_hf.h>
#include "BluetoothCommon.h"
#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHALInterface;

class BluetoothHandsfreeHALInterface final
  : public BluetoothHandsfreeInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothHandsfreeNotificationHandler* aNotificationHandler,
            BluetoothHandsfreeResultHandler* aRes);
  void Cleanup(BluetoothHandsfreeResultHandler* aRes);

  /* Connect / Disconnect */

  void Connect(const nsAString& aBdAddr,
               BluetoothHandsfreeResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothHandsfreeResultHandler* aRes);
  void ConnectAudio(const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void DisconnectAudio(const nsAString& aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes);

  /* Voice Recognition */

  void StartVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);
  void StopVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                     BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  void DeviceStatusNotification(BluetoothHandsfreeNetworkState aNtkState,
                                BluetoothHandsfreeServiceType aSvcType,
                                int aSignal, int aBattChg,
                                BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  void CopsResponse(const char* aCops,
                    BluetoothHandsfreeResultHandler* aRes);
  void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                    BluetoothHandsfreeCallState aCallSetupState, int aSignal,
                    int aRoam, int aBattChg,
                    BluetoothHandsfreeResultHandler* aRes);
  void FormattedAtResponse(const char* aRsp,
                           BluetoothHandsfreeResultHandler* aRes);
  void AtResponse(BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
                  BluetoothHandsfreeResultHandler* aRes);
  void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                    BluetoothHandsfreeCallState aState,
                    BluetoothHandsfreeCallMode aMode,
                    BluetoothHandsfreeCallMptyType aMpty,
                    const nsAString& aNumber,
                    BluetoothHandsfreeCallAddressType aType,
                    BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  void PhoneStateChange(int aNumActive, int aNumHeld,
                        BluetoothHandsfreeCallState aCallSetupState,
                        const nsAString& aNumber,
                        BluetoothHandsfreeCallAddressType aType,
                        BluetoothHandsfreeResultHandler* aRes);

protected:
  BluetoothHandsfreeHALInterface(const bthf_interface_t* aInterface);
  ~BluetoothHandsfreeHALInterface();

private:
  const bthf_interface_t* mInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
