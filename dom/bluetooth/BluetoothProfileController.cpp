/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothProfileController.h"
#include "BluetoothReplyRunnable.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"

#include "BluetoothUtils.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

USING_BLUETOOTH_NAMESPACE

#define BT_LOGR_PROFILE(mgr, msg, ...)               \
  do {                                               \
    nsCString name;                                  \
    mgr->GetName(name);                              \
    BT_LOGR("[%s] " msg, name.get(), ##__VA_ARGS__); \
  } while(0)

BluetoothProfileController::BluetoothProfileController(
                                   bool aConnect,
                                   const nsAString& aDeviceAddress,
                                   BluetoothReplyRunnable* aRunnable,
                                   BluetoothProfileControllerCallback aCallback,
                                   uint16_t aServiceUuid,
                                   uint32_t aCod)
  : mConnect(aConnect)
  , mDeviceAddress(aDeviceAddress)
  , mRunnable(aRunnable)
  , mCallback(aCallback)
  , mSuccess(false)
  , mProfilesIndex(-1)
{
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aCallback);

  mProfiles.Clear();

  /**
   * If the service uuid is not specified, either connect multiple profiles
   * based on Cod, or disconnect all connected profiles.
   */
  if (!aServiceUuid) {
    mTarget.cod = aCod;
    SetupProfiles(false);
  } else {
    BluetoothServiceClass serviceClass =
      BluetoothUuidHelper::GetBluetoothServiceClass(aServiceUuid);
    mTarget.service = serviceClass;
    SetupProfiles(true);
  }
}

BluetoothProfileController::~BluetoothProfileController()
{
  mProfiles.Clear();
  mRunnable = nullptr;
  mCallback = nullptr;
}

void
BluetoothProfileController::AddProfileWithServiceClass(
                                                   BluetoothServiceClass aClass)
{
  BluetoothProfileManagerBase* profile;
  switch (aClass) {
    case BluetoothServiceClass::HANDSFREE:
    case BluetoothServiceClass::HEADSET:
      profile = BluetoothHfpManager::Get();
      break;
    case BluetoothServiceClass::A2DP:
      profile = BluetoothA2dpManager::Get();
      break;
    case BluetoothServiceClass::HID:
      profile = BluetoothHidManager::Get();
      break;
    default:
      DispatchBluetoothReply(mRunnable, BluetoothValue(),
                             NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
      mCallback();
      return;
  }

  AddProfile(profile);
}

void
BluetoothProfileController::AddProfile(BluetoothProfileManagerBase* aProfile,
                                       bool aCheckConnected)
{
  if (!aProfile) {
    DispatchBluetoothReply(mRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    mCallback();
    return;
  }

  if (aCheckConnected && !aProfile->IsConnected()) {
    BT_WARNING("The profile is not connected.");
    return;
  }

  mProfiles.AppendElement(aProfile);
}

void
BluetoothProfileController::SetupProfiles(bool aAssignServiceClass)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * When a service class is assigned, only its corresponding profile is put
   * into array.
   */
  if (aAssignServiceClass) {
    AddProfileWithServiceClass(mTarget.service);
    return;
  }

  // For a disconnect request, all connected profiles are put into array.
  if (!mConnect) {
    AddProfile(BluetoothHidManager::Get(), true);
    AddProfile(BluetoothA2dpManager::Get(), true);
    AddProfile(BluetoothHfpManager::Get(), true);
    return;
  }

  /**
   * For a connect request, put multiple profiles into array and connect to
   * all of them sequencely.
   */
  bool hasAudio = HAS_AUDIO(mTarget.cod);
  bool hasRendering = HAS_RENDERING(mTarget.cod);
  bool isPeripheral = IS_PERIPHERAL(mTarget.cod);
  bool isRemoteControl = IS_REMOTE_CONTROL(mTarget.cod);
  bool isKeyboard = IS_KEYBOARD(mTarget.cod);
  bool isPointingDevice = IS_POINTING_DEVICE(mTarget.cod);

  NS_ENSURE_TRUE_VOID(hasAudio || hasRendering || isPeripheral);

  // Audio bit should be set if remote device supports HFP/HSP.
  if (hasAudio) {
    AddProfile(BluetoothHfpManager::Get());
  }

  // Rendering bit should be set if remote device supports A2DP.
  // A device which supports AVRCP should claim that it's a peripheral and it's
  // a remote control.
  if (hasRendering || (isPeripheral && isRemoteControl)) {
    AddProfile(BluetoothA2dpManager::Get());
  }

  // A device which supports HID should claim that it's a peripheral and it's
  // either a keyboard, a pointing device, or both.
  if (isPeripheral && (isKeyboard || isPointingDevice)) {
    AddProfile(BluetoothHidManager::Get());
  }
}

void
BluetoothProfileController::Start()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  MOZ_ASSERT(mProfilesIndex == -1);
  NS_ENSURE_TRUE_VOID(mProfiles.Length() > 0);

  ++mProfilesIndex;
  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "");

  if (mConnect) {
    mProfiles[mProfilesIndex]->Connect(mDeviceAddress, this);
  } else {
    mProfiles[mProfilesIndex]->Disconnect(this);
  }
}

void
BluetoothProfileController::Next()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  MOZ_ASSERT(mProfilesIndex < mProfiles.Length());

  if (++mProfilesIndex < mProfiles.Length()) {
    BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "");

    if (mConnect) {
      mProfiles[mProfilesIndex]->Connect(mDeviceAddress, this);
    } else {
      mProfiles[mProfilesIndex]->Disconnect(this);
    }
    return;
  }

  MOZ_ASSERT(mRunnable && mCallback);

  // The action has been completed, so the dom request is replied and then
  // the callback is invoked
  if (mSuccess) {
    DispatchBluetoothReply(mRunnable, BluetoothValue(true), EmptyString());
  } else {
    DispatchBluetoothReply(mRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
  }
  mCallback();
}

void
BluetoothProfileController::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "<%s>",
    NS_ConvertUTF16toUTF8(aErrorStr).get());

  if (!aErrorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(aErrorStr).get());
  } else {
    mSuccess = true;
  }

  Next();
}

void
BluetoothProfileController::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "<%s>",
    NS_ConvertUTF16toUTF8(aErrorStr).get());

  if (!aErrorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(aErrorStr).get());
  } else {
    mSuccess = true;
  }

  Next();
}
