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

class GamepadChangeEvent;
class GamepadTestHelper;

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
//
//   // This thread will probably be running async, so be sure to capture by
//   // value and not reference on most things
//   maybeReceiver->StartMonitoringThread([=](const GamepadChangeEvent& e) {
//     this->handleEvent(e);
//   });
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

  // Start a thread that monitors the shared memory for changes
  //
  // The thread will call `aFn` with an event generated for each gamepad
  // value that has changed since the last time it checked. Note that this
  // means that multiple changes to the same value will appear as a single
  // change if they happen quickly enough. This generally doesn't matter for
  // gamepads.
  bool StartMonitoringThread(
      const std::function<void(const GamepadChangeEvent&)>& aFn);

  // Stop the monitoring thread
  //
  // If the broadcaster continues sending messages after this is called, they
  // will be missed. It is okay to restart the monitoring thread and even
  // to pass in a different function.
  void StopMonitoringThread();

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

  friend class GamepadTestHelper;
  bool StartMonitoringThreadForTesting(
      const std::function<void(const GamepadChangeEvent&)>& aMonitorFn,
      const std::function<void(uint32_t)>& aTestCommandFn);
};

}  // namespace mozilla::dom

#endif  // DOM_GAMEPAD_GAMEPADSTATERECEIVER_H_
