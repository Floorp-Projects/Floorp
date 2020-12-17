/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadTestChannelParent.h"

#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"

namespace mozilla::dom {

already_AddRefed<GamepadTestChannelParent> GamepadTestChannelParent::Create() {
  return RefPtr<GamepadTestChannelParent>(new GamepadTestChannelParent())
      .forget();
}

GamepadTestChannelParent::GamepadTestChannelParent() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  service->GetMonitoringState().AddObserver(this);
}

GamepadTestChannelParent::~GamepadTestChannelParent() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  service->GetMonitoringState().RemoveObserver(this);
}

void GamepadTestChannelParent::AddGamepadToPlatformService(
    uint32_t aPromiseId, const GamepadAdded& aGamepadAdded) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  const GamepadAdded& a = aGamepadAdded;
  nsCString gamepadID;
  LossyCopyUTF16toASCII(a.id(), gamepadID);
  uint32_t index = service->AddGamepad(
      gamepadID.get(), static_cast<GamepadMappingType>(a.mapping()), a.hand(),
      a.num_buttons(), a.num_axes(), a.num_haptics(), a.num_lights(),
      a.num_touches());

  Unused << SendReplyGamepadIndex(aPromiseId, index);
}

void GamepadTestChannelParent::OnMonitoringStateChanged(bool aNewState) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  if (aNewState) {
    for (auto& deferredGamepadAdd : mDeferredGamepadAdded) {
      AddGamepadToPlatformService(deferredGamepadAdd.promiseId,
                                  deferredGamepadAdd.gamepadAdded);
    }
    mDeferredGamepadAdded.Clear();
  }
}

mozilla::ipc::IPCResult GamepadTestChannelParent::RecvGamepadTestEvent(
    const uint32_t& aID, const GamepadChangeEvent& aEvent) {
  mozilla::ipc::AssertIsOnBackgroundThread();

  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  const GamepadChangeEventBody& body = aEvent.body();

  // GamepadAdded is special because it will be deferred if monitoring hasn't
  // started. Everything else won't be deferred and will fail if monitoring
  // isn't running
  if (body.type() == GamepadChangeEventBody::TGamepadAdded) {
    if (service->GetMonitoringState().IsMonitoring()) {
      AddGamepadToPlatformService(aID, body.get_GamepadAdded());
    } else {
      mDeferredGamepadAdded.AppendElement(
          DeferredGamepadAdded{aID, body.get_GamepadAdded()});
    }
    return IPC_OK();
  }

  if (!service->GetMonitoringState().IsMonitoring()) {
    return IPC_FAIL(this, "Simulated message received while not monitoring");
  }

  const uint32_t index = aEvent.index();

  if (body.type() == GamepadChangeEventBody::TGamepadRemoved) {
    service->RemoveGamepad(index);
    return IPC_OK();
  }
  if (body.type() == GamepadChangeEventBody::TGamepadButtonInformation) {
    const GamepadButtonInformation& a = body.get_GamepadButtonInformation();
    service->NewButtonEvent(index, a.button(), a.pressed(), a.touched(),
                            a.value());
    return IPC_OK();
  }
  if (body.type() == GamepadChangeEventBody::TGamepadAxisInformation) {
    const GamepadAxisInformation& a = body.get_GamepadAxisInformation();
    service->NewAxisMoveEvent(index, a.axis(), a.value());
    return IPC_OK();
  }
  if (body.type() == GamepadChangeEventBody::TGamepadPoseInformation) {
    const GamepadPoseInformation& a = body.get_GamepadPoseInformation();
    service->NewPoseEvent(index, a.pose_state());
    return IPC_OK();
  }
  if (body.type() == GamepadChangeEventBody::TGamepadTouchInformation) {
    const GamepadTouchInformation& a = body.get_GamepadTouchInformation();
    service->NewMultiTouchEvent(index, a.index(), a.touch_state());
    return IPC_OK();
  }

  NS_WARNING("Unknown event type.");
  return IPC_FAIL_NO_REASON(this);
}

}  // namespace mozilla::dom
