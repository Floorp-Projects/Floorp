/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamepadEventChannelParent.h"
#include "GamepadPlatformService.h"
#include "mozilla/dom/GamepadMonitoring.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

using namespace mozilla::ipc;

namespace {

class SendGamepadUpdateRunnable final : public Runnable
{
 private:
  ~SendGamepadUpdateRunnable() {}
  RefPtr<GamepadEventChannelParent> mParent;
  GamepadChangeEvent mEvent;
 public:
  SendGamepadUpdateRunnable(GamepadEventChannelParent* aParent,
                            GamepadChangeEvent aEvent)
    : mEvent(aEvent)
  {
    MOZ_ASSERT(aParent);
    mParent = aParent;
  }
  NS_IMETHOD Run() override
  {
    AssertIsOnBackgroundThread();
    if(mParent->HasGamepadListener()) {
      Unused << mParent->SendGamepadUpdate(mEvent);
    }
    return NS_OK;
  }
};

} // namespace

GamepadEventChannelParent::GamepadEventChannelParent()
  : mHasGamepadListener(false)
{
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);

  mBackgroundEventTarget = GetCurrentThreadEventTarget();
  service->AddChannelParent(this);
}

mozilla::ipc::IPCResult
GamepadEventChannelParent::RecvGamepadListenerAdded()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mHasGamepadListener);
  mHasGamepadListener = true;
  StartGamepadMonitoring();
  return IPC_OK();
}

mozilla::ipc::IPCResult
GamepadEventChannelParent::RecvGamepadListenerRemoved()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mHasGamepadListener);
  mHasGamepadListener = false;
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  MOZ_ASSERT(service);
  service->RemoveChannelParent(this);
  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GamepadEventChannelParent::RecvVibrateHaptic(const uint32_t& aControllerIdx,
                                   const uint32_t& aHapticIndex,
                                   const double& aIntensity,
                                   const double& aDuration,
                                   const uint32_t& aPromiseID)
{
  // TODO: Bug 680289, implement for standard gamepads

  if (SendReplyGamepadVibrateHaptic(aPromiseID)) {
    return IPC_OK();
  }

  return IPC_FAIL(this, "SendReplyGamepadVibrateHaptic fail.");
}

mozilla::ipc::IPCResult
GamepadEventChannelParent::RecvStopVibrateHaptic(const uint32_t& aGamepadIndex)
{
  // TODO: Bug 680289, implement for standard gamepads
  return IPC_OK();
}

void
GamepadEventChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  // It may be called because IPDL child side crashed, we'll
  // not receive RecvGamepadListenerRemoved in that case
  if (mHasGamepadListener) {
    mHasGamepadListener = false;
    RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
    MOZ_ASSERT(service);
    service->RemoveChannelParent(this);
  }
  MaybeStopGamepadMonitoring();
}

void
GamepadEventChannelParent::DispatchUpdateEvent(const GamepadChangeEvent& aEvent)
{
  mBackgroundEventTarget->Dispatch(new SendGamepadUpdateRunnable(this, aEvent),
                                   NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla
