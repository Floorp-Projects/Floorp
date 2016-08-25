/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadTestChannelParent.h"

#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

bool
GamepadTestChannelParent::RecvGamepadTestEvent(const uint32_t& aID,
                                               const GamepadChangeEvent& aEvent)
{
  mozilla::ipc::AssertIsOnBackgroundThread();
  RefPtr<GamepadPlatformService>  service =
    GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);
  if (aEvent.type() == GamepadChangeEvent::TGamepadAdded) {
    const GamepadAdded& a = aEvent.get_GamepadAdded();
    nsCString gamepadID;
    LossyCopyUTF16toASCII(a.id(), gamepadID);
    uint32_t index = service->AddGamepad(gamepadID.get(),
                                         (GamepadMappingType)a.mapping(),
                                         a.num_buttons(),
                                         a.num_axes());
    if (!mShuttingdown) {
      Unused << SendReplyGamepadIndex(aID, index);
    }
    return true;
  }
  if (aEvent.type() == GamepadChangeEvent::TGamepadRemoved) {
    const GamepadRemoved& a = aEvent.get_GamepadRemoved();
    service->RemoveGamepad(a.index());
    return true;
  }
  if (aEvent.type() == GamepadChangeEvent::TGamepadButtonInformation) {
    const GamepadButtonInformation& a = aEvent.get_GamepadButtonInformation();
    service->NewButtonEvent(a.index(), a.button(), a.pressed(), a.value());
    return true;
  }
  if (aEvent.type() == GamepadChangeEvent::TGamepadAxisInformation) {
    const GamepadAxisInformation& a = aEvent.get_GamepadAxisInformation();
    service->NewAxisMoveEvent(a.index(), a.axis(), a.value());
    return true;
  }

  NS_WARNING("Unknown event type.");
  return false;
}

bool
GamepadTestChannelParent::RecvShutdownChannel()
{
  mShuttingdown = true;
  Unused << Send__delete__(this);
  return true;
}

} // namespace dom
} // namespace mozilla
