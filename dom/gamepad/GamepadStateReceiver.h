/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the receiving half of a pair. See GamepadStateBroadcaster.h for the
// transmitting half of this.

// Implements single-broadcaster, multi-receiver shared memory for the gamepad
// system. Allows the gamepad monitor thread in the main process to directly
// update the shared memory to the latest state and trigger an event in all
// the content processes, causing them to read the shared memory and pass it
// to Javascript.

// This takes advantage of the fact that gamepad clients generally only care
// about the most recent state of the gamepad, not the stuff that happened
// while they weren't looking.

// This currently only does anything interesting on Windows, and it uses
// Pimpl idiom to avoid OS-specific types in this header.

#ifndef DOM_GAMEPAD_GAMEPADSTATERECEIVER_H_
#define DOM_GAMEPAD_GAMEPADSTATERECEIVER_H_

#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadTouchState.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include <functional>

namespace mozilla::ipc {
class IProtocol;
}  // namespace mozilla::ipc

namespace mozilla::dom {

// IPDL structure that knows how to initialize a receiver
class GamepadStateBroadcastReceiverInfo;

// The receiving half of the pair
//
// Spins up a thread that waits for changes to the shared memory to arrive
// from the other side, and generates GamepadChangeEvents that can be used to
// signal Javascript that the gamepad state has changed
//
// # Example
//
// mozilla::ipc::IPCResult RecvGamepadReceiverInfo(
//     const GamepadStateBroadcastReceiverInfo& aReceiverInfo) {
//   Maybe<GamepadStateReceiver> maybeReceiver =
//       GamepadStateReceiver::Create(aReceiverInfo);
//   MOZ_ASSERT(maybeReceiver);
// }
//
class GamepadStateReceiver {
 public:
  // Create the receiving side from info passed over IPDL
  //
  // The GamepadStateBroadcastReceiverInfo structure should have been obtained
  // from a call to `AddReceiverAndGenerateRemoteInfo()` and passed via IPDL
  // to this function in the remote process.
  static Maybe<GamepadStateReceiver> Create(
      const GamepadStateBroadcastReceiverInfo& aReceiverInfo);

  // Allow move
  GamepadStateReceiver(GamepadStateReceiver&& aOther);
  GamepadStateReceiver& operator=(GamepadStateReceiver&& aOther);

  // Disallow copy
  GamepadStateReceiver(const GamepadStateReceiver&) = delete;
  GamepadStateReceiver& operator=(const GamepadStateReceiver&) = delete;

  ~GamepadStateReceiver();

 private:
  class Impl;

  GamepadStateReceiver();
  explicit GamepadStateReceiver(UniquePtr<Impl> aImpl);

  UniquePtr<Impl> mImpl;
};

}  // namespace mozilla::dom

#endif  // DOM_GAMEPAD_GAMEPADSTATERECEIVER_H_
