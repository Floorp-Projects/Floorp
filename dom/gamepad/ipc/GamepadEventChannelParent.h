/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/PGamepadEventChannelParent.h"

#ifndef mozilla_dom_GamepadEventChannelParent_h_
#  define mozilla_dom_GamepadEventChannelParent_h_

namespace mozilla::dom {

class GamepadEventChannelParent final : public PGamepadEventChannelParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadEventChannelParent, override)

  static already_AddRefed<GamepadEventChannelParent> Create();
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvVibrateHaptic(
      const Tainted<GamepadHandle>& aHandle,
      const Tainted<uint32_t>& aHapticIndex, const Tainted<double>& aIntensity,
      const Tainted<double>& aDuration, const uint32_t& aPromiseID);
  mozilla::ipc::IPCResult RecvStopVibrateHaptic(
      const Tainted<GamepadHandle>& aHandle);
  mozilla::ipc::IPCResult RecvLightIndicatorColor(
      const Tainted<GamepadHandle>& aHandle,
      const Tainted<uint32_t>& aLightColorIndex, const uint8_t& aRed,
      const uint8_t& aGreen, const uint8_t& aBlue, const uint32_t& aPromiseID);
  void DispatchUpdateEvent(const GamepadChangeEvent& aEvent);

  mozilla::ipc::IPCResult RecvRequestAllGamepads(
      RequestAllGamepadsResolver&& aResolver);

  GamepadEventChannelParent(const GamepadEventChannelParent&) = delete;
  GamepadEventChannelParent(GamepadEventChannelParent&&) = delete;
  GamepadEventChannelParent& operator=(const GamepadEventChannelParent&) =
      delete;
  GamepadEventChannelParent& operator=(GamepadEventChannelParent&&) = delete;

 private:
  GamepadEventChannelParent();
  ~GamepadEventChannelParent() = default;

  bool mIsShutdown;
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
};

}  // namespace mozilla::dom

#endif
