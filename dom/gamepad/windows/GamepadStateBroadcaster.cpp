/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadStateBroadcaster.h"

#include "GamepadStateLayout.h"
#include "GamepadWindowsUtil.h"
#include "mozilla/dom/GamepadStateBroadcastReceiverInfo.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/SynchronizedSharedMemory.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/HashTable.h"
#include <inttypes.h>
#include <windows.h>

namespace mozilla::dom {

using SharedState = SynchronizedSharedMemory<GamepadSystemState>;

class GamepadStateBroadcaster::Impl {
 public:
  static UniquePtr<Impl> Create() {
    Maybe<SharedState> sharedState = SharedState::CreateNew();
    if (!sharedState) {
      return nullptr;
    }

    return UniquePtr<Impl>(new Impl(std::move(*sharedState)));
  }

  bool AddReceiverAndGenerateRemoteInfo(
      const mozilla::ipc::IProtocol* aActor,
      GamepadStateBroadcastReceiverInfo* aOut) {
    MOZ_ASSERT(aOut);

    DWORD targetPID = aActor ? aActor->OtherPid() : ::GetCurrentProcessId();

    // Create the NT event and a remote handle for it

    UniqueHandle<NTEventHandleTraits> eventHandle(
        ::CreateEvent(nullptr /*no ACL*/, FALSE /*not manual*/, FALSE /*unset*/,
                      nullptr /*no name*/));
    if (!eventHandle) {
      return false;
    }

    HANDLE remoteEventHandle = nullptr;
    if (!mozilla::ipc::DuplicateHandle(eventHandle.Get(), targetPID,
                                       &remoteEventHandle, 0,
                                       DUPLICATE_SAME_ACCESS)) {
      return false;
    }

    // Generate a remote handle for the shared memory

    SynchronizedSharedMemoryRemoteInfo sharedMemoryRemoteInfo;
    if (!mSharedState.GenerateRemoteInfo(aActor, &sharedMemoryRemoteInfo)) {
      // There is no reasonable way to clean up remoteEventHandle here, since
      // our process doesn't own it
      return false;
    }

    MOZ_ALWAYS_TRUE(mBroadcastEventHandles.append(
        BroadcastEventHandle{aActor, std::move(eventHandle)}));

    // The event and the shared memory are everything the remote side needs

    aOut->sharedMemoryInfo() = std::move(sharedMemoryRemoteInfo);
    aOut->eventHandle() = reinterpret_cast<WindowsHandle>(remoteEventHandle);

    return true;
  }

  void RemoveReceiver(const mozilla::ipc::IProtocol* aActor) {
    auto* ptr = [&]() -> BroadcastEventHandle* {
      for (auto& x : mBroadcastEventHandles) {
        if (x.actor == aActor) {
          return &x;
        }
      }
      return nullptr;
    }();

    if (!ptr) {
      MOZ_ASSERT(false, "Tried to remove a receiver that was never added");
      return;
    }

    // We don't care about order, so we can remove an entry by overwriting
    // it with the last element and then popping the last element
    if (ptr != &mBroadcastEventHandles.back()) {
      (*ptr) = std::move(mBroadcastEventHandles.back());
    }

    mBroadcastEventHandles.popBack();
  }

  // Disallow copy/move
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

 private:
  struct BroadcastEventHandle {
    const mozilla::ipc::IProtocol* actor;
    UniqueHandle<NTEventHandleTraits> eventHandle;
  };

  explicit Impl(SharedState aSharedState)
      : mSharedState(std::move(aSharedState)) {}

  SharedState mSharedState;
  Vector<BroadcastEventHandle> mBroadcastEventHandles;
};

//////////// Everything below this line is Pimpl boilerplate ///////////////////

// static
Maybe<GamepadStateBroadcaster> GamepadStateBroadcaster::Create() {
  UniquePtr<Impl> impl = Impl::Create();
  if (!impl) {
    return Nothing{};
  }

  return Some(GamepadStateBroadcaster(std::move(impl)));
}

GamepadStateBroadcaster::~GamepadStateBroadcaster() = default;

bool GamepadStateBroadcaster::AddReceiverAndGenerateRemoteInfo(
    const mozilla::ipc::IProtocol* aActor,
    GamepadStateBroadcastReceiverInfo* aOut) {
  return mImpl->AddReceiverAndGenerateRemoteInfo(aActor, aOut);
}

void GamepadStateBroadcaster::RemoveReceiver(
    const mozilla::ipc::IProtocol* aActor) {
  mImpl->RemoveReceiver(aActor);
}

GamepadStateBroadcaster::GamepadStateBroadcaster(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster& GamepadStateBroadcaster::operator=(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster::GamepadStateBroadcaster() = default;
GamepadStateBroadcaster::GamepadStateBroadcaster(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

}  // namespace mozilla::dom
