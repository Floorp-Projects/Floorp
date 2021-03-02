/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadStateReceiver.h"

namespace mozilla::dom {

class GamepadStateReceiver::Impl {
 public:
  Impl() = default;
  ~Impl() = default;
};

// static
Maybe<GamepadStateReceiver> GamepadStateReceiver::Create(
    const GamepadStateBroadcastReceiverInfo& aReceiverInfo) {
  return Nothing{};
}

GamepadStateReceiver::~GamepadStateReceiver() = default;

GamepadStateReceiver::GamepadStateReceiver(GamepadStateReceiver&& aOther) =
    default;
GamepadStateReceiver& GamepadStateReceiver::operator=(
    GamepadStateReceiver&& aOther) = default;

GamepadStateReceiver::GamepadStateReceiver() = default;

GamepadStateReceiver::GamepadStateReceiver(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

bool GamepadStateReceiver::StartMonitoringThread(
    const std::function<void(const GamepadChangeEvent&)>& aFn) {
  MOZ_CRASH("Should never be called");
}
bool GamepadStateReceiver::StartMonitoringThreadForTesting(
    const std::function<void(const GamepadChangeEvent&)>& aMonitorFn,
    const std::function<void(uint32_t)>& aTestCommandFn) {
  MOZ_CRASH("Should never be called");
}

void GamepadStateReceiver::StopMonitoringThread() {
  MOZ_CRASH("Should never be called");
}

}  // namespace mozilla::dom
