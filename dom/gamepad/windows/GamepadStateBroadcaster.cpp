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

  void AddGamepad(GamepadHandle aHandle, const char* aID,
                  GamepadMappingType aMapping, GamepadHand aHand,
                  uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aNumHaptics,
                  uint32_t aNumLights, uint32_t aNumTouches) {
    size_t lenId = strlen(aID);
    MOZ_RELEASE_ASSERT(lenId <= kMaxGamepadIdLength);
    MOZ_RELEASE_ASSERT(aNumButtons <= kMaxButtonsPerGamepad);
    MOZ_RELEASE_ASSERT(aNumAxes <= kMaxAxesPerGamepad);
    MOZ_RELEASE_ASSERT(aNumLights <= kMaxLightsPerGamepad);
    MOZ_RELEASE_ASSERT(aNumTouches <= kMaxNumMultiTouches);

    // We pass an empty handle as the first argument, which tells
    // ModifyGamepadSlot to run the lambda on the first empty slot that's
    // found.
    //
    // If there are no empty slots, the following lambda won't be run and
    // the add will fail silently (which is preferable to crashing)
    ModifyGamepadSlot(GamepadHandle{}, [&](GamepadSlot& slot) {
      slot.handle = aHandle;
      memcpy(&slot.props.id[0], aID, lenId);
      slot.props.id[lenId] = 0;
      slot.props.mapping = aMapping;
      slot.props.hand = aHand;
      slot.props.numButtons = aNumButtons;
      slot.props.numAxes = aNumAxes;
      slot.props.numHaptics = aNumHaptics;
      slot.props.numLights = aNumLights;
      slot.props.numTouches = aNumTouches;
    });
  }

  void RemoveGamepad(GamepadHandle aHandle) {
    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle,
                      [&](GamepadSlot& slot) { slot = GamepadSlot{}; });
  }

  void NewAxisMoveEvent(GamepadHandle aHandle, uint32_t aAxis, double aValue) {
    MOZ_RELEASE_ASSERT(aAxis < kMaxAxesPerGamepad);

    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle, [&](GamepadSlot& slot) {
      MOZ_ASSERT(aAxis < slot.props.numAxes);
      slot.values.axes[aAxis] = aValue;
    });
  }

  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed,
                      bool aTouched, double aValue) {
    MOZ_RELEASE_ASSERT(aButton < kMaxButtonsPerGamepad);

    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle, [&](GamepadSlot& slot) {
      MOZ_ASSERT(aButton < slot.props.numButtons);
      slot.values.buttonValues[aButton] = aValue;
      slot.values.buttonPressedBits[aButton] = aPressed;
      slot.values.buttonTouchedBits[aButton] = aTouched;
    });
  }

  void NewLightIndicatorTypeEvent(GamepadHandle aHandle, uint32_t aLight,
                                  GamepadLightIndicatorType aType) {
    MOZ_RELEASE_ASSERT(aLight < kMaxLightsPerGamepad);

    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle, [&](GamepadSlot& slot) {
      MOZ_ASSERT(aLight < slot.props.numLights);
      slot.values.lights[aLight] = aType;
    });
  }

  void NewPoseEvent(GamepadHandle aHandle, const GamepadPoseState& aState) {
    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle,
                      [&](GamepadSlot& slot) { slot.values.pose = aState; });
  }

  void NewMultiTouchEvent(GamepadHandle aHandle, uint32_t aTouchArrayIndex,
                          const GamepadTouchState& aState) {
    MOZ_RELEASE_ASSERT(aTouchArrayIndex < kMaxNumMultiTouches);

    // If the handle was never added, this function will do nothing
    ModifyGamepadSlot(aHandle, [&](GamepadSlot& slot) {
      MOZ_ASSERT(aTouchArrayIndex < slot.props.numTouches);
      slot.values.touches[aTouchArrayIndex] = aState;
    });
  }

  void SendTestCommand(uint32_t aCommandId) {
    mSharedState.RunWithLock([&](GamepadSystemState* p) {
      // SECURITY BOUNDARY -- `GamepadSystemState* p` is a local copy of the
      // shared memory (it is not a live copy), and we only write to it here.
      // It is also only used for testing
      p->testCommandId = aCommandId;
      p->testCommandTrigger = !p->testCommandTrigger;

      p->changeId = mChangeId;
      ++mChangeId;
    });

    TriggerEvents();
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
      : mChangeId(1), mSharedState(std::move(aSharedState)) {}

  void ModifyGamepadSlot(GamepadHandle aHandle,
                         const std::function<void(GamepadSlot&)>& aFn) {
    mSharedState.RunWithLock([&](GamepadSystemState* p) {
      // SECURITY BOUNDARY -- `GamepadSystemState* p` is a local copy of the
      // shared memory (it is not a live copy), so after this validation we can
      // trust the values inside of it
      ValidateGamepadSystemState(p);

      GamepadSlot* foundSlot = nullptr;
      for (auto& slot : p->gamepadSlots) {
        if (slot.handle == aHandle) {
          foundSlot = &slot;
          break;
        }
      }

      if (!foundSlot) {
        return;
      }

      aFn(*foundSlot);

      p->changeId = mChangeId;
      ++mChangeId;
    });

    TriggerEvents();
  }

  void TriggerEvents() {
    for (auto& x : mBroadcastEventHandles) {
      MOZ_ALWAYS_TRUE(::SetEvent(x.eventHandle.Get()));
    }
  }

  uint64_t mChangeId;
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

void GamepadStateBroadcaster::AddGamepad(
    GamepadHandle aHandle, const char* aID, GamepadMappingType aMapping,
    GamepadHand aHand, uint32_t aNumButtons, uint32_t aNumAxes,
    uint32_t aNumHaptics, uint32_t aNumLights, uint32_t aNumTouches) {
  mImpl->AddGamepad(aHandle, aID, aMapping, aHand, aNumButtons, aNumAxes,
                    aNumHaptics, aNumLights, aNumTouches);
}

void GamepadStateBroadcaster::RemoveGamepad(GamepadHandle aHandle) {
  mImpl->RemoveGamepad(aHandle);
}

void GamepadStateBroadcaster::NewAxisMoveEvent(GamepadHandle aHandle,
                                               uint32_t aAxis, double aValue) {
  mImpl->NewAxisMoveEvent(aHandle, aAxis, aValue);
}

void GamepadStateBroadcaster::NewButtonEvent(GamepadHandle aHandle,
                                             uint32_t aButton, bool aPressed,
                                             bool aTouched, double aValue) {
  mImpl->NewButtonEvent(aHandle, aButton, aPressed, aTouched, aValue);
}

void GamepadStateBroadcaster::NewLightIndicatorTypeEvent(
    GamepadHandle aHandle, uint32_t aLight, GamepadLightIndicatorType aType) {
  mImpl->NewLightIndicatorTypeEvent(aHandle, aLight, aType);
}

void GamepadStateBroadcaster::NewPoseEvent(GamepadHandle aHandle,
                                           const GamepadPoseState& aState) {
  mImpl->NewPoseEvent(aHandle, aState);
}

void GamepadStateBroadcaster::NewMultiTouchEvent(
    GamepadHandle aHandle, uint32_t aTouchArrayIndex,
    const GamepadTouchState& aState) {
  mImpl->NewMultiTouchEvent(aHandle, aTouchArrayIndex, aState);
}

void GamepadStateBroadcaster::SendTestCommand(uint32_t aCommandId) {
  mImpl->SendTestCommand(aCommandId);
}

GamepadStateBroadcaster::GamepadStateBroadcaster(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster& GamepadStateBroadcaster::operator=(
    GamepadStateBroadcaster&& aOther) = default;

GamepadStateBroadcaster::GamepadStateBroadcaster() = default;
GamepadStateBroadcaster::GamepadStateBroadcaster(UniquePtr<Impl> aImpl)
    : mImpl(std::move(aImpl)) {}

}  // namespace mozilla::dom
