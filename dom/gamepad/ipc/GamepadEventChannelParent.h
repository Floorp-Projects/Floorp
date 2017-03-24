/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PGamepadEventChannelParent.h"

#ifndef mozilla_dom_GamepadEventChannelParent_h_
#define mozilla_dom_GamepadEventChannelParent_h_

namespace mozilla{
namespace dom{

class GamepadEventChannelParent final : public PGamepadEventChannelParent
{
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadEventChannelParent)
  GamepadEventChannelParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult RecvGamepadListenerAdded() override;
  virtual mozilla::ipc::IPCResult RecvGamepadListenerRemoved() override;
  virtual mozilla::ipc::IPCResult RecvVibrateHaptic(const uint32_t& aControllerIdx,
                                                    const uint32_t& aHapticIndex,
                                                    const double& aIntensity,
                                                    const double& aDuration,
                                                    const uint32_t& aPromiseID) override;
  virtual mozilla::ipc::IPCResult RecvStopVibrateHaptic(
                                    const uint32_t& aGamepadIndex) override;
  void DispatchUpdateEvent(const GamepadChangeEvent& aEvent);
  bool HasGamepadListener() const { return mHasGamepadListener; }
 private:
  ~GamepadEventChannelParent() {}
  bool mHasGamepadListener;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

}// namespace dom
}// namespace mozilla

#endif
