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
#include "BluetoothOppManager.h"

#include "BluetoothUtils.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

USING_BLUETOOTH_NAMESPACE

BluetoothProfileController::BluetoothProfileController(
                                   const nsAString& aDeviceAddress,
                                   BluetoothReplyRunnable* aRunnable,
                                   BluetoothProfileControllerCallback aCallback)
  : mCallback(aCallback)
  , mDeviceAddress(aDeviceAddress)
  , mRunnable(aRunnable)
  , mSuccess(false)
{
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aCallback);

  mProfilesIndex = -1;
  mProfiles.Clear();
}

BluetoothProfileController::~BluetoothProfileController()
{
  mProfiles.Clear();
  mRunnable = nullptr;
  mCallback = nullptr;
}

bool
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
    case BluetoothServiceClass::OBJECT_PUSH:
      profile = BluetoothOppManager::Get();
      break;
    case BluetoothServiceClass::HID:
      profile = BluetoothHidManager::Get();
      break;
    default:
      DispatchBluetoothReply(mRunnable, BluetoothValue(),
                             NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
      mCallback();
      return false;
  }

  return AddProfile(profile);
}

bool
BluetoothProfileController::AddProfile(BluetoothProfileManagerBase* aProfile,
                                       bool aCheckConnected)
{
  if (!aProfile) {
    DispatchBluetoothReply(mRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    mCallback();
    return false;
  }

  if (aCheckConnected && !aProfile->IsConnected()) {
    return false;
  }

  mProfiles.AppendElement(aProfile);
  return true;
}

void
BluetoothProfileController::Connect(BluetoothServiceClass aClass)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(AddProfileWithServiceClass(aClass));

  ConnectNext();
}

void
BluetoothProfileController::Connect(uint32_t aCod)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Put multiple profiles into array and connect to all of them sequencely
  bool hasAudio = HAS_AUDIO(aCod);
  bool hasObjectTransfer = HAS_OBJECT_TRANSFER(aCod);
  bool hasRendering = HAS_RENDERING(aCod);
  bool isPeripheral = IS_PERIPHERAL(aCod);

  NS_ENSURE_TRUE_VOID(hasAudio || hasObjectTransfer ||
                      hasRendering || isPeripheral);

  mCod = aCod;

  /**
   * Connect to HFP/HSP first. Then, connect A2DP if Rendering bit is set.
   * It's almost impossible to send file to a remote device which is an Audio
   * device or a Rendering device, so we won't connect OPP in that case.
   */
  BluetoothProfileManagerBase* profile;
  if (hasAudio) {
    AddProfile(BluetoothHfpManager::Get());
  }
  if (hasRendering) {
    AddProfile(BluetoothA2dpManager::Get());
  }
  if (hasObjectTransfer && !hasAudio && !hasRendering) {
    AddProfile(BluetoothOppManager::Get());
  }
  if (isPeripheral) {
    AddProfile(BluetoothHidManager::Get());
  }

  ConnectNext();
}

void
BluetoothProfileController::ConnectNext()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (++mProfilesIndex < mProfiles.Length()) {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());

    mProfiles[mProfilesIndex]->Connect(mDeviceAddress, this);
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

  if (!aErrorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(aErrorStr).get());
  } else {
    mSuccess = true;
  }

  ConnectNext();
}

void
BluetoothProfileController::Disconnect(BluetoothServiceClass aClass)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aClass != BluetoothServiceClass::UNKNOWN) {
    NS_ENSURE_TRUE_VOID(AddProfileWithServiceClass(aClass));

    DisconnectNext();
    return;
  }

  // Put all connected profiles into array and disconnect all of them
  AddProfile(BluetoothHidManager::Get(), true);
  AddProfile(BluetoothOppManager::Get(), true);
  AddProfile(BluetoothA2dpManager::Get(), true);
  AddProfile(BluetoothHfpManager::Get(), true);

  DisconnectNext();
}

void
BluetoothProfileController::DisconnectNext()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (++mProfilesIndex < mProfiles.Length()) {
    mProfiles[mProfilesIndex]->Disconnect(this);
    return;
  }

  MOZ_ASSERT(mRunnable && mCallback);

  // The action has been completed, so the dom request is replied and then
  // the callback is invoked
  if (mSuccess) {
    DispatchBluetoothReply(mRunnable, BluetoothValue(true), EmptyString());
  } else {
    DispatchBluetoothReply(mRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_DISCONNECTION_FAILED));
  }
  mCallback();
}

void
BluetoothProfileController::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aErrorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(aErrorStr).get());
  } else {
    mSuccess = true;
  }

  DisconnectNext();
}

