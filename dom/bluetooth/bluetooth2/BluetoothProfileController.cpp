/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothProfileController.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsComponentManagerUtils.h"

USING_BLUETOOTH_NAMESPACE

#define BT_LOGR_PROFILE(mgr, msg, ...)               \
  do {                                               \
    nsCString name;                                  \
    mgr->GetName(name);                              \
    BT_LOGR("[%s] " msg, name.get(), ##__VA_ARGS__); \
  } while(0)

#define CONNECTION_TIMEOUT_MS 15000

class CheckProfileStatusCallback : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  CheckProfileStatusCallback(BluetoothProfileController* aController)
    : mController(aController)
  {
    MOZ_ASSERT(aController);
  }

private:
  virtual ~CheckProfileStatusCallback()
  {
    mController = nullptr;
  }

  nsRefPtr<BluetoothProfileController> mController;
};

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
  , mCurrentProfileFinished(false)
  , mSuccess(false)
  , mProfilesIndex(-1)
{
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aCallback);

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mTimer);

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

  if (mTimer) {
    mTimer->Cancel();
  }
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
      DispatchReplyError(mRunnable, NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
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
    DispatchReplyError(mRunnable, NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
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
  bool isInvalid = IS_INVALID_COD(mTarget.cod);

  // The value of CoD is invalid. Since the device didn't declare its class of
  // device properly, we assume the device may support all of these profiles.
  if (isInvalid) {
    AddProfile(BluetoothHfpManager::Get());
    AddProfile(BluetoothA2dpManager::Get());
    AddProfile(BluetoothHidManager::Get());
    return;
  }

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

NS_IMPL_ISUPPORTS(CheckProfileStatusCallback, nsITimerCallback)

NS_IMETHODIMP
CheckProfileStatusCallback::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(mController);
  // Continue on the next profile since we haven't got the callback after
  // timeout.
  mController->GiveupAndContinue();

  return NS_OK;
}

void
BluetoothProfileController::StartSession()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  MOZ_ASSERT(mProfilesIndex == -1);
  MOZ_ASSERT(mTimer);

  if (!IsBtServiceAvailable()) {
    EndSession();
    return;
  }

  if (mProfiles.Length() < 1) {
    BT_LOGR("No queued profile.");
    EndSession();
    return;
  }

  if (mTimer) {
    mTimer->InitWithCallback(new CheckProfileStatusCallback(this),
                             CONNECTION_TIMEOUT_MS, nsITimer::TYPE_ONE_SHOT);
  }

  BT_LOGR("%s", mConnect ? "connecting" : "disconnecting");

  Next();
}

void
BluetoothProfileController::EndSession()
{
  MOZ_ASSERT(mRunnable && mCallback);

  BT_LOGR("mSuccess %d", mSuccess);

  // Don't have to check profile status and retrigger session after connection
  // timeout, since session is end.
  if (mTimer) {
    mTimer->Cancel();
  }

  // The action has completed, so the DOM request should be replied then invoke
  // the callback.
  if (mSuccess) {
    DispatchReplySuccess(mRunnable);
  } else if (mConnect) {
    DispatchReplyError(mRunnable, NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
  } else {
    DispatchReplyError(mRunnable, NS_LITERAL_STRING(ERR_DISCONNECTION_FAILED));
  }

  mCallback();
}

void
BluetoothProfileController::Next()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  MOZ_ASSERT(mProfilesIndex < (int)mProfiles.Length());
  MOZ_ASSERT(mTimer);

  mCurrentProfileFinished = false;

  if (!IsBtServiceAvailable()) {
    EndSession();
    return;
  }

  if (++mProfilesIndex >= (int)mProfiles.Length()) {
    EndSession();
    return;
  }

  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "");

  if (mConnect) {
    mProfiles[mProfilesIndex]->Connect(mDeviceAddress, this);
  } else {
    mProfiles[mProfilesIndex]->Disconnect(this);
  }
}

bool
BluetoothProfileController::IsBtServiceAvailable() const
{
  BluetoothService* bs = BluetoothService::Get();
  return (bs && bs->IsEnabled() && !bs->IsToggling());
}

void
BluetoothProfileController::NotifyCompletion(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(mProfiles.Length() > 0);

  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], "<%s>",
                  NS_ConvertUTF16toUTF8(aErrorStr).get());

  mCurrentProfileFinished = true;

  if (mTimer) {
    mTimer->Cancel();
  }

  mSuccess |= aErrorStr.IsEmpty();

  Next();
}

void
BluetoothProfileController::GiveupAndContinue()
{
  MOZ_ASSERT(!mCurrentProfileFinished);
  MOZ_ASSERT(mProfilesIndex < (int)mProfiles.Length());

  BT_LOGR_PROFILE(mProfiles[mProfilesIndex], ERR_OPERATION_TIMEOUT);
  mProfiles[mProfilesIndex]->Reset();

  if (IsBtServiceAvailable()) {
    Next();
  } else {
    EndSession();
  }
}

