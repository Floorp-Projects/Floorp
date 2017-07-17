/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadTestChannelParent.h"

#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

mozilla::ipc::IPCResult
GamepadTestChannelParent::RecvGamepadTestEvent(const uint32_t& aID,
                                               const GamepadChangeEvent& aEvent)
{
  mozilla::ipc::AssertIsOnBackgroundThread();
  RefPtr<GamepadPlatformService>  service =
    GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);
  const uint32_t index = aEvent.index();
  const GamepadChangeEventBody& body = aEvent.body();
  if (body.type() == GamepadChangeEventBody::TGamepadAdded) {
    const GamepadAdded& a = body.get_GamepadAdded();
    nsCString gamepadID;
    LossyCopyUTF16toASCII(a.id(), gamepadID);
    uint32_t index = service->AddGamepad(gamepadID.get(),
                                         static_cast<GamepadMappingType>(a.mapping()),
                                         a.hand(),
                                         a.num_buttons(),
                                         a.num_axes(),
                                         a.num_haptics());
    if (!mShuttingdown) {
      Unused << SendReplyGamepadIndex(aID, index);
    }
    return IPC_OK();
  }
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

  NS_WARNING("Unknown event type.");
  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult
GamepadTestChannelParent::RecvShutdownChannel()
{
  mShuttingdown = true;
  Unused << Send__delete__(this);
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
