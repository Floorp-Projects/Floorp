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
     : Runnable("dom::GamepadUpdateRunnable")
     , mEvent(aGamepadEvent)
   {
   }
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

mozilla::ipc::IPCResult
GamepadEventChannelChild::RecvGamepadUpdate(
                                       const GamepadChangeEvent& aGamepadEvent)
{
  DebugOnly<nsresult> rv =
    NS_DispatchToMainThread(new GamepadUpdateRunnable(aGamepadEvent));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
  return IPC_OK();
}

void
GamepadEventChannelChild::AddPromise(const uint32_t& aID, dom::Promise* aPromise)
{
  MOZ_ASSERT(!mPromiseList.Get(aID, nullptr));
  mPromiseList.Put(aID, aPromise);
}

mozilla::ipc::IPCResult
GamepadEventChannelChild::RecvReplyGamepadVibrateHaptic(const uint32_t& aPromiseID)
{
  RefPtr<dom::Promise> p;
  if (!mPromiseList.Get(aPromiseID, getter_AddRefs(p))) {
    MOZ_CRASH("We should always have a promise.");
  }

  p->MaybeResolve(true);
  mPromiseList.Remove(aPromiseID);
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
