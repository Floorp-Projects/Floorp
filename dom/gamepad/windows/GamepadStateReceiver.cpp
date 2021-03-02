/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadStateReceiver.h"

#include "GamepadStateLayout.h"
#include "GamepadWindowsUtil.h"
#include "mozilla/dom/GamepadStateBroadcastReceiverInfo.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include <inttypes.h>
#include <windows.h>

namespace mozilla::dom {

using SharedState = SynchronizedSharedMemory<GamepadSystemState>;

class GamepadStateReceiver::Impl {
 public:
  static UniquePtr<Impl> Create(
      const GamepadStateBroadcastReceiverInfo& aReceiverInfo) {
    Maybe<SharedState> sharedState =
        SharedState::CreateFromRemoteInfo(aReceiverInfo.sharedMemoryInfo());

    UniqueHandle<NTEventHandleTraits> eventHandle(
        reinterpret_cast<HANDLE>(aReceiverInfo.eventHandle()));

    if (!sharedState || !eventHandle) {
      return nullptr;
    }

    return UniquePtr<Impl>(
        new Impl(std::move(*sharedState), std::move(eventHandle)));
  }

  // Disallow copy/move
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  explicit Impl(SharedState aSharedState,
                UniqueHandle<NTEventHandleTraits> aEventHandle)
      : mSharedState(std::move(aSharedState)),
        mEventHandle(std::move(aEventHandle)) {}

  SharedState mSharedState;
  UniqueHandle<NTEventHandleTraits> mEventHandle;
};

//////////// Everything below this line is Pimpl boilerplate ///////////////////

// static
Maybe<GamepadStateReceiver> GamepadStateReceiver::Create(
    const GamepadStateBroadcastReceiverInfo& aReceiverInfo) {
  UniquePtr<Impl> impl = Impl::Create(aReceiverInfo);
  if (!impl) {
    return Nothing{};
  }

  return Some(GamepadStateReceiver(std::move(impl)));
}

GamepadStateReceiver::~GamepadStateReceiver() = default;

GamepadStateReceiver::GamepadStateReceiver(GamepadStateReceiver&& aOther) =
    default;
GamepadStateReceiver& GamepadStateReceiver::operator=(
    GamepadStateReceiver&& aOther) = default;

GamepadStateReceiver::GamepadStateReceiver() = default;

GamepadStateReceiver::GamepadStateReceiver(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

}  // namespace mozilla::dom
