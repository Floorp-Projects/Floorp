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

GamepadStateBroadcaster::GamepadStateBroadcaster(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster& GamepadStateBroadcaster::operator=(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster::GamepadStateBroadcaster() = default;
GamepadStateBroadcaster::GamepadStateBroadcaster(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

}  // namespace mozilla::dom
