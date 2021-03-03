/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadStateBroadcaster.h"

namespace mozilla::dom {

class GamepadStateBroadcaster::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
};

// static
Maybe<GamepadStateBroadcaster> GamepadStateBroadcaster::Create() {
  return Nothing{};
}

GamepadStateBroadcaster::~GamepadStateBroadcaster() = default;

bool GamepadStateBroadcaster::AddReceiverAndGenerateRemoteInfo(
    const mozilla::ipc::IProtocol* aActor,
    GamepadStateBroadcastReceiverInfo* aOut) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::RemoveReceiver(
    const mozilla::ipc::IProtocol* aActor) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::AddGamepad(
    GamepadHandle aHandle, const char* aID, GamepadMappingType aMapping,
    GamepadHand aHand, uint32_t aNumButtons, uint32_t aNumAxes,
    uint32_t aNumHaptics, uint32_t aNumLights, uint32_t aNumTouches) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::RemoveGamepad(GamepadHandle aHandle) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::NewAxisMoveEvent(GamepadHandle aHandle,
                                               uint32_t aAxis, double aValue) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::NewButtonEvent(GamepadHandle aHandle,
                                             uint32_t aButton, bool aPressed,
                                             bool aTouched, double aValue) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::NewLightIndicatorTypeEvent(
    GamepadHandle aHandle, uint32_t aLight, GamepadLightIndicatorType aType) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::NewPoseEvent(GamepadHandle aHandle,
                                           const GamepadPoseState& aState) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::NewMultiTouchEvent(
    GamepadHandle aHandle, uint32_t aTouchArrayIndex,
    const GamepadTouchState& aState) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateBroadcaster::SendTestCommand(uint32_t aCommandId) {
  MOZ_CRASH("Should never be called");
}

GamepadStateBroadcaster::GamepadStateBroadcaster(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster& GamepadStateBroadcaster::operator=(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster::GamepadStateBroadcaster() = default;
GamepadStateBroadcaster::GamepadStateBroadcaster(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

}  // namespace mozilla::dom
