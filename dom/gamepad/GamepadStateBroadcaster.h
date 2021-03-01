/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the receiving half of a pair. See GamepadStateReceiver.h for the
// receiving half of this.

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

#ifndef DOM_GAMEPAD_GAMEPADSTATEBROADCASTER_H_
#define DOM_GAMEPAD_GAMEPADSTATEBROADCASTER_H_

#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadTouchState.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::ipc {
class IProtocol;
}  // namespace mozilla::ipc

namespace mozilla::dom {

class GamepadChangeEvent;
class GamepadTestHelper;

// IPDL structure that knows how to initialize a receiver
class GamepadStateBroadcastReceiverInfo;

// The broadcasting half of the pair
//
// Can be used to deliver an event quickly and simultaneously to every
// receiver. Does not guarantee that every event will be seen by every
// receiver, but *does* guarantee that every receiver will see a coherent
// snapshot of the controller state at a point in time.
//
// (IE they will never see an unplugged controller with a button pressed)
//
// # Example:
//
// Maybe<GamepadStateBroadcaster> maybeBroadcaster =
//     GamepadStateBroadcaster::Create();
// MOZ_ASSERT(maybeBroadcaster);
//
// GamepadStateBroadcastReceiverInfo remoteInfo{};
// bool result = maybeBroadcaster->AddReceiverAndGenerateRemoteInfo(
//     aIPCActor, &remoteInfo);
// MOZ_ASSERT(result);
//
// result = aIPCActor->SendBroadcasterInfo(remoteInfo);
// MOZ_ASSERT(result);
//
// maybeBroadcaster->AddGamepad(aHandle);
// maybeBroadcaster->NewAxisMoveEvent(aHandle, 0 /*axisId*/, 0.0 /*value*/);
//
class GamepadStateBroadcaster {
 public:
  // Create a new broadcaster
  //
  // Allocates the shared memory region for a new broadcaster that can
  // be shared with others using `AddReceiverAndGenerateRemoteInfo()`
  static Maybe<GamepadStateBroadcaster> Create();

  // Add an IPDL actor as a new receiver and create remote info
  //
  // `aActor` will be added as a listener for events, and `aOut` will contain
  // the remote info that must be passed via IPDL to the remote process.
  //
  // The info will only be valid within the context of `aActor`, and failure
  // to deliver it will result in a handle leak until the remote actor's
  // process dies.
  bool AddReceiverAndGenerateRemoteInfo(
      const mozilla::ipc::IProtocol* aActor,
      GamepadStateBroadcastReceiverInfo* aOut);

  // Removes an IPDL actor that was previously registered as a receiver
  //
  // This will free up any system resources that were required on the
  // broadcaster side.
  void RemoveReceiver(const mozilla::ipc::IProtocol* aActor);

  // Adds a new gamepad to shared memory region
  void AddGamepad(GamepadHandle aHandle, const char* aID,
                  GamepadMappingType aMapping, GamepadHand aHand,
                  uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aNumHaptics,
                  uint32_t aNumLights, uint32_t aNumTouches);

  // Removes a gamepad from the shared memory region
  void RemoveGamepad(GamepadHandle aHandle);

  // Update a gamepad axis in the shared memory region
  void NewAxisMoveEvent(GamepadHandle aHandle, uint32_t aAxis, double aValue);

  // Update a gamepad button in the shared memory region
  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed,
                      bool aTouched, double aValue);

  // Update a gamepad light indicator in the shared memory region
  void NewLightIndicatorTypeEvent(GamepadHandle aHandle, uint32_t aLight,
                                  GamepadLightIndicatorType aType);

  // Update a gamepad pose state in the shared memory region
  void NewPoseEvent(GamepadHandle aHandle, const GamepadPoseState& aState);

  // Update a gamepad multi-touch state in the shared memory region
  void NewMultiTouchEvent(GamepadHandle aHandle, uint32_t aTouchArrayIndex,
                          const GamepadTouchState& aState);

  // Allow move
  GamepadStateBroadcaster(GamepadStateBroadcaster&& aOther) noexcept;
  GamepadStateBroadcaster& operator=(GamepadStateBroadcaster&& aOther) noexcept;

  // Disallow copy
  GamepadStateBroadcaster(const GamepadStateBroadcaster&) = delete;
  GamepadStateBroadcaster& operator=(const GamepadStateBroadcaster&) = delete;

  ~GamepadStateBroadcaster();

 private:
  class Impl;

  GamepadStateBroadcaster();
  explicit GamepadStateBroadcaster(UniquePtr<Impl> aImpl);

  UniquePtr<Impl> mImpl;

  friend class GamepadTestHelper;
  void SendTestCommand(uint32_t aCommandId);
};

}  // namespace mozilla::dom

#endif  // DOM_GAMEPAD_GAMEPADSTATEBROADCASTER_H_
