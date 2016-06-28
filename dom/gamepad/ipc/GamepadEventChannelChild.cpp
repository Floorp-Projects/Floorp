/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamepadEventChannelChild.h"
#include "mozilla/dom/GamepadManager.h"

namespace mozilla {
namespace dom{

namespace {

class GamepadUpdateRunnable final : public Runnable
{
 public:
  explicit GamepadUpdateRunnable(const GamepadChangeEvent& aGamepadEvent)
             : mEvent(aGamepadEvent) {}
  NS_IMETHOD Run() override
  {
    RefPtr<GamepadManager> svc(GamepadManager::GetService());
    if (svc) {
      svc->Update(mEvent);
    }
    return NS_OK;
  }
 protected:
  GamepadChangeEvent mEvent;
};

} // namespace

bool
GamepadEventChannelChild::RecvGamepadUpdate(
                                       const GamepadChangeEvent& aGamepadEvent)
{
  nsresult rv;
  rv = NS_DispatchToMainThread(new GamepadUpdateRunnable(aGamepadEvent));
  NS_WARN_IF(NS_FAILED(rv));
  return true;
}

} // namespace dom
} // namespace mozilla
