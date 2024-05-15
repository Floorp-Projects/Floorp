/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamepadEventChannelParent.h"
#include "GamepadPlatformService.h"
#include "mozilla/dom/GamepadMonitoring.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

using namespace mozilla::ipc;

namespace {

class SendGamepadUpdateRunnable final : public Runnable {
 private:
  ~SendGamepadUpdateRunnable() = default;
  RefPtr<GamepadEventChannelParent> mParent;
  GamepadChangeEvent mEvent;

 public:
  SendGamepadUpdateRunnable(GamepadEventChannelParent* aParent,
                            GamepadChangeEvent aEvent)
      : Runnable("dom::SendGamepadUpdateRunnable"),
        mParent(aParent),
        mEvent(aEvent) {
    MOZ_ASSERT(mParent);
  }
  NS_IMETHOD Run() override {
    AssertIsOnBackgroundThread();
    Unused << mParent->SendGamepadUpdate(mEvent);
    return NS_OK;
  }
};

}  // namespace

already_AddRefed<GamepadEventChannelParent>
GamepadEventChannelParent::Create() {
  return RefPtr<GamepadEventChannelParent>(new GamepadEventChannelParent())
      .forget();
}

GamepadEventChannelParent::GamepadEventChannelParent() : mIsShutdown{false} {
  MOZ_DIAGNOSTIC_ASSERT(IsOnBackgroundThread());

  mBackgroundEventTarget = GetCurrentSerialEventTarget();

  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  service->AddChannelParent(this);
}

void GamepadEventChannelParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_DIAGNOSTIC_ASSERT(IsOnBackgroundThread());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutdown);

  mIsShutdown = true;

  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);
  service->RemoveChannelParent(this);
}

mozilla::ipc::IPCResult GamepadEventChannelParent::RecvVibrateHaptic(
    const Tainted<GamepadHandle>& aHandle,
    const Tainted<uint32_t>& aHapticIndex, const Tainted<double>& aIntensity,
    const Tainted<double>& aDuration, const uint32_t& aPromiseID) {
  // TODO: Bug 680289, implement for standard gamepads

  if (SendReplyGamepadPromise(aPromiseID)) {
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyGamepadPromise fail.");
}

mozilla::ipc::IPCResult GamepadEventChannelParent::RecvStopVibrateHaptic(
    const Tainted<GamepadHandle>& aHandle) {
  // TODO: Bug 680289, implement for standard gamepads
  return IPC_OK();
}

mozilla::ipc::IPCResult GamepadEventChannelParent::RecvLightIndicatorColor(
    const Tainted<GamepadHandle>& aHandle,
    const Tainted<uint32_t>& aLightColorIndex, const uint8_t& aRed,
    const uint8_t& aGreen, const uint8_t& aBlue, const uint32_t& aPromiseID) {
  SetGamepadLightIndicatorColor(aHandle, aLightColorIndex, aRed, aGreen, aBlue);

  if (SendReplyGamepadPromise(aPromiseID)) {
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyGamepadPromise fail.");
}

mozilla::ipc::IPCResult GamepadEventChannelParent::RecvRequestAllGamepads(
    RequestAllGamepadsResolver&& aResolver) {
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  aResolver(service->GetAllGamePads());
  return IPC_OK();
}

void GamepadEventChannelParent::DispatchUpdateEvent(
    const GamepadChangeEvent& aEvent) {
  mBackgroundEventTarget->Dispatch(new SendGamepadUpdateRunnable(this, aEvent),
                                   NS_DISPATCH_NORMAL);
}

}  // namespace mozilla::dom
