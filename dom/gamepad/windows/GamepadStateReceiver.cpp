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
#include "prthread.h"
#include <atomic>
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

  bool StartMonitoringThread(
      const std::function<void(const GamepadChangeEvent&)>& aMonitorFn,
      const std::function<void(uint32_t)>& aTestCommandFn) {
    MOZ_ASSERT(!mMonitorThread);

    mMonitorFn = aMonitorFn;
    mTestCommandFn = aTestCommandFn;

    // Every time the thread wakes up from an event, it checks this before
    // it does anything else. The thread exits when this is `true`
    mStopMonitoring.store(false, std::memory_order_release);

    mMonitorThread = PR_CreateThread(
        PR_USER_THREAD,
        [](void* p) { static_cast<Impl*>(p)->MonitoringThread(); }, this,
        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);

    return !!mMonitorThread;
  }

  void StopMonitoringThread() {
    MOZ_ASSERT(mMonitorThread);

    /// Every time the thread wakes up from an event, it checks this before
    // it does anything else. The thread exits when this is `true`
    mStopMonitoring.store(true, std::memory_order_release);

    // Wake the thread up with the event, causing it to exit
    MOZ_ALWAYS_TRUE(::SetEvent(mEventHandle.Get()));

    MOZ_ALWAYS_TRUE(PR_JoinThread(mMonitorThread) == PR_SUCCESS);
    mMonitorThread = nullptr;
    mMonitorFn = nullptr;
    mTestCommandFn = nullptr;
  }

  // Disallow copy/move
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  ~Impl() {
    if (mMonitorThread) {
      MOZ_ASSERT(false,
                 "GamepadStateReceiver::~Impl() was called without "
                 "calling StopMonitoringThread().");
      StopMonitoringThread();
    }
  }

 private:
  explicit Impl(SharedState aSharedState,
                UniqueHandle<NTEventHandleTraits> aEventHandle)
      : mSharedState(std::move(aSharedState)),
        mEventHandle(std::move(aEventHandle)),
        mMonitorThread(nullptr) {}

  // This compares two GamepadValues structures for a single gamepad and
  // generates events for any detected differences.
  // Generally we are either comparing a known gamepad to its last known state,
  // or we are comparing a new gamepad against the default state
  void DiffGamepadValues(
      GamepadHandle handle, const GamepadProperties& props,
      const GamepadValues& curValues, const GamepadValues& newValues,
      const std::function<void(const GamepadChangeEvent&)>& aFn) {
    // Diff axes
    for (uint32_t i = 0; i < props.numAxes; ++i) {
      if (curValues.axes[i] != newValues.axes[i]) {
        GamepadAxisInformation axisInfo(i, newValues.axes[i]);
        GamepadChangeEvent e(handle, axisInfo);
        aFn(e);
      }
    }

    // Diff buttons
    for (uint32_t i = 0; i < props.numButtons; ++i) {
      if ((curValues.buttonValues[i] != newValues.buttonValues[i]) ||
          (curValues.buttonPressedBits[i] != newValues.buttonPressedBits[i]) ||
          (curValues.buttonTouchedBits[i] != newValues.buttonTouchedBits[i])) {
        GamepadButtonInformation buttonInfo(i, newValues.buttonValues[i],
                                            newValues.buttonPressedBits[i],
                                            newValues.buttonTouchedBits[i]);
        GamepadChangeEvent e(handle, buttonInfo);
        aFn(e);
      }
    }

    // Diff indictator lights
    for (uint32_t i = 0; i < props.numLights; ++i) {
      if (curValues.lights[i] != newValues.lights[i]) {
        GamepadLightIndicatorTypeInformation lightInfo(i, newValues.lights[i]);
        GamepadChangeEvent e(handle, lightInfo);
        aFn(e);
      }
    }

    // Diff multi-touch states
    for (uint32_t i = 0; i < props.numTouches; ++i) {
      if (curValues.touches[i] != newValues.touches[i]) {
        GamepadTouchInformation touchInfo(i, newValues.touches[i]);
        GamepadChangeEvent e(handle, touchInfo);
        aFn(e);
      }
    }

    // Diff poses
    if (curValues.pose != newValues.pose) {
      GamepadPoseInformation poseInfo(newValues.pose);
      GamepadChangeEvent e(handle, poseInfo);
      aFn(e);
    }
  }

  // Compare two gamepad slots and generate events based on the differences
  //
  // This is generally used to compare the old and new state of a single slot.
  // If the same gamepad is still in the slot from last check, we just need
  // to diff the gamepad's values.
  //
  // If a different gamepad is now in the slot, we need to unregister the old
  // one, register the new one, and generate events for every value that is
  // different than the default "new gamepad" state.
  //
  void DiffGamepadSlots(
      uint32_t changeId, const GamepadSlot& curState,
      const GamepadSlot& newState,
      const std::function<void(const GamepadChangeEvent&)>& aFn) {
    if (curState.handle == newState.handle) {
      if (newState.handle != GamepadHandle{}) {
        // Same gamepad is plugged in as last time - Just diff values
        DiffGamepadValues(newState.handle, newState.props, curState.values,
                          newState.values, aFn);
      }
      return;
    }

    // The gamepad in this slot has changed.

    // If there was previously a gamepad in this slot, remove it
    if (curState.handle != GamepadHandle{}) {
      GamepadChangeEvent e(curState.handle, GamepadRemoved{});
      aFn(e);
    }

    // If there is a gamepad in it now, register it
    if (newState.handle != GamepadHandle{}) {
      GamepadAdded gamepadInfo(
          NS_ConvertUTF8toUTF16(nsDependentCString(&newState.props.id[0])),
          newState.props.mapping, newState.props.hand, 0,
          newState.props.numButtons, newState.props.numAxes,
          newState.props.numHaptics, newState.props.numLights,
          newState.props.numTouches);

      GamepadChangeEvent e(newState.handle, gamepadInfo);
      aFn(e);

      // Since the gamepad is new, we diff its values against a
      // default-constructed GamepadValues structure
      DiffGamepadValues(newState.handle, newState.props, GamepadValues{},
                        newState.values, aFn);
    }
  }

  void MonitoringThread() {
    while (true) {
      // If the other side crashes or something goes very wrong, we're probably
      // about to be destroyed anyhow. Just quit the thread and await our
      // inevitable doom
      if (::WaitForSingleObject(mEventHandle.Get(), INFINITE) !=
          WAIT_OBJECT_0) {
        break;
      }

      // First check if the signal is from StopMonitoring telling us we're done
      if (mStopMonitoring.load(std::memory_order_acquire)) {
        break;
      }

      // Read the shared memory
      GamepadSystemState newSystemState;
      mSharedState.RunWithLock(
          [&](GamepadSystemState* p) { newSystemState = *p; });

      // SECURITY BOUNDARY -- After this validation, we can trust newSystemState
      ValidateGamepadSystemState(&newSystemState);

      if (mGamepadSystemState.changeId != newSystemState.changeId) {
        // Diff the state of each gamepad slot from the previous read
        for (size_t i = 0; i < kMaxGamepads; ++i) {
          DiffGamepadSlots(newSystemState.changeId,
                           mGamepadSystemState.gamepadSlots[i],
                           newSystemState.gamepadSlots[i], mMonitorFn);
        }

        if (mGamepadSystemState.testCommandTrigger !=
            newSystemState.testCommandTrigger) {
          if (mTestCommandFn) {
            mTestCommandFn(newSystemState.testCommandId);
          }
        }

        // Save the current read for the next diff
        mGamepadSystemState = newSystemState;
      }
    }
  }

  SharedState mSharedState;
  UniqueHandle<NTEventHandleTraits> mEventHandle;
  GamepadSystemState mGamepadSystemState;

  std::atomic_bool mStopMonitoring;
  std::function<void(const GamepadChangeEvent&)> mMonitorFn;
  std::function<void(uint32_t)> mTestCommandFn;
  PRThread* mMonitorThread;
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

bool GamepadStateReceiver::StartMonitoringThread(
    const std::function<void(const GamepadChangeEvent&)>& aFn) {
  return mImpl->StartMonitoringThread(aFn, nullptr);
}
bool GamepadStateReceiver::StartMonitoringThreadForTesting(
    const std::function<void(const GamepadChangeEvent&)>& aMonitorFn,
    const std::function<void(uint32_t)>& aTestCommandFn) {
  return mImpl->StartMonitoringThread(aMonitorFn, aTestCommandFn);
}

void GamepadStateReceiver::StopMonitoringThread() {
  mImpl->StopMonitoringThread();
}

}  // namespace mozilla::dom
