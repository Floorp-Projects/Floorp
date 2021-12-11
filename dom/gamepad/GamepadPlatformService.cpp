/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadPlatformService.h"

#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/dom/GamepadMonitoring.h"
#include "mozilla/dom/GamepadTestChannelParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Mutex.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsHashKeys.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

namespace {

// This is the singleton instance of GamepadPlatformService, can be called
// by both background and monitor thread.
StaticRefPtr<GamepadPlatformService> gGamepadPlatformServiceSingleton;

}  // namespace

// static
GamepadMonitoringState& GamepadMonitoringState::GetSingleton() {
  static GamepadMonitoringState sInstance{};
  return sInstance;
}

void GamepadMonitoringState::AddObserver(GamepadTestChannelParent* aParent) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ALWAYS_TRUE(mObservers.append(aParent));
}

void GamepadMonitoringState::RemoveObserver(GamepadTestChannelParent* aParent) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  WeakPtr<GamepadTestChannelParent>* observer = nullptr;

  for (auto& item : mObservers) {
    if (item == aParent) {
      observer = &item;
    }
  }

  MOZ_ASSERT(
      observer,
      "Attempted to remove a GamepadTestChannelParent that was never added");

  std::swap(*observer, mObservers.back());
  mObservers.popBack();
}

bool GamepadMonitoringState::IsMonitoring() const {
  AssertIsOnBackgroundThread();
  return mIsMonitoring;
}

void GamepadMonitoringState::Set(bool aIsMonitoring) {
  AssertIsOnBackgroundThread();

  if (mIsMonitoring != aIsMonitoring) {
    mIsMonitoring = aIsMonitoring;
    for (auto& observer : mObservers) {
      // Since each GamepadTestChannelParent removes itself in its dtor, this
      // should never be nullptr
      MOZ_RELEASE_ASSERT(observer);
      observer->OnMonitoringStateChanged(aIsMonitoring);
    }
  }
}

GamepadPlatformService::GamepadPlatformService()
    : mNextGamepadHandleValue(1),
      mMutex("mozilla::dom::GamepadPlatformService") {}

GamepadPlatformService::~GamepadPlatformService() { Cleanup(); }

// static
already_AddRefed<GamepadPlatformService>
GamepadPlatformService::GetParentService() {
  // GamepadPlatformService can only be accessed in parent process
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!gGamepadPlatformServiceSingleton) {
    // Only Background Thread can create new GamepadPlatformService instance.
    if (IsOnBackgroundThread()) {
      gGamepadPlatformServiceSingleton = new GamepadPlatformService();
    } else {
      return nullptr;
    }
  }
  RefPtr<GamepadPlatformService> service(gGamepadPlatformServiceSingleton);
  return service.forget();
}

template <class T>
void GamepadPlatformService::NotifyGamepadChange(GamepadHandle aHandle,
                                                 const T& aInfo) {
  // This method is called by monitor populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadChangeEventBody body(aInfo);
  GamepadChangeEvent e(aHandle, body);

  // mChannelParents may be accessed by background thread in the
  // same time, we use mutex to prevent possible race condtion
  MutexAutoLock autoLock(mMutex);

  for (uint32_t i = 0; i < mChannelParents.Length(); ++i) {
    mChannelParents[i]->DispatchUpdateEvent(e);
  }
}

GamepadHandle GamepadPlatformService::AddGamepad(
    const char* aID, GamepadMappingType aMapping, GamepadHand aHand,
    uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aHaptics,
    uint32_t aNumLightIndicator, uint32_t aNumTouchEvents) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadHandle gamepadHandle{mNextGamepadHandleValue++,
                              GamepadHandleKind::GamepadPlatformManager};

  // Only VR controllers has displayID, we give 0 to the general gamepads.
  GamepadAdded a(NS_ConvertUTF8toUTF16(nsDependentCString(aID)), aMapping,
                 aHand, 0, aNumButtons, aNumAxes, aHaptics, aNumLightIndicator,
                 aNumTouchEvents);

  mGamepadAdded.emplace(gamepadHandle, a);
  NotifyGamepadChange<GamepadAdded>(gamepadHandle, a);
  return gamepadHandle;
}

void GamepadPlatformService::RemoveGamepad(GamepadHandle aHandle) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadRemoved a;
  NotifyGamepadChange<GamepadRemoved>(aHandle, a);
  mGamepadAdded.erase(aHandle);
}

void GamepadPlatformService::NewButtonEvent(GamepadHandle aHandle,
                                            uint32_t aButton, bool aPressed,
                                            bool aTouched, double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadButtonInformation a(aButton, aValue, aPressed, aTouched);
  NotifyGamepadChange<GamepadButtonInformation>(aHandle, a);
}

void GamepadPlatformService::NewButtonEvent(GamepadHandle aHandle,
                                            uint32_t aButton, bool aPressed,
                                            double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aHandle, aButton, aPressed, aPressed, aValue);
}

void GamepadPlatformService::NewButtonEvent(GamepadHandle aHandle,
                                            uint32_t aButton, bool aPressed,
                                            bool aTouched) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aHandle, aButton, aPressed, aTouched, aPressed ? 1.0L : 0.0L);
}

void GamepadPlatformService::NewButtonEvent(GamepadHandle aHandle,
                                            uint32_t aButton, bool aPressed) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aHandle, aButton, aPressed, aPressed, aPressed ? 1.0L : 0.0L);
}

void GamepadPlatformService::NewAxisMoveEvent(GamepadHandle aHandle,
                                              uint32_t aAxis, double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadAxisInformation a(aAxis, aValue);
  NotifyGamepadChange<GamepadAxisInformation>(aHandle, a);
}

void GamepadPlatformService::NewLightIndicatorTypeEvent(
    GamepadHandle aHandle, uint32_t aLight, GamepadLightIndicatorType aType) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadLightIndicatorTypeInformation a(aLight, aType);
  NotifyGamepadChange<GamepadLightIndicatorTypeInformation>(aHandle, a);
}

void GamepadPlatformService::NewPoseEvent(GamepadHandle aHandle,
                                          const GamepadPoseState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadPoseInformation a(aState);
  NotifyGamepadChange<GamepadPoseInformation>(aHandle, a);
}

void GamepadPlatformService::NewMultiTouchEvent(
    GamepadHandle aHandle, uint32_t aTouchArrayIndex,
    const GamepadTouchState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadTouchInformation a(aTouchArrayIndex, aState);
  NotifyGamepadChange<GamepadTouchInformation>(aHandle, a);
}

void GamepadPlatformService::ResetGamepadIndexes() {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  mNextGamepadHandleValue = 1;
}

void GamepadPlatformService::AddChannelParent(
    GamepadEventChannelParent* aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mChannelParents.Contains(aParent));

  // We use mutex here to prevent race condition with monitor thread
  {
    MutexAutoLock autoLock(mMutex);
    mChannelParents.AppendElement(aParent);

    // For a new GamepadEventChannel, we have to send the exising GamepadAdded
    // to it to make it can have the same amount of gamepads with others.
    if (mChannelParents.Length() > 1) {
      for (const auto& evt : mGamepadAdded) {
        GamepadChangeEventBody body(evt.second);
        GamepadChangeEvent e(evt.first, body);
        aParent->DispatchUpdateEvent(e);
      }
    }
  }

  StartGamepadMonitoring();

  GamepadMonitoringState::GetSingleton().Set(true);
}

void GamepadPlatformService::RemoveChannelParent(
    GamepadEventChannelParent* aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mChannelParents.Contains(aParent));

  // We use mutex here to prevent race condition with monitor thread
  {
    MutexAutoLock autoLock(mMutex);
    mChannelParents.RemoveElement(aParent);
    if (!mChannelParents.IsEmpty()) {
      return;
    }
  }

  GamepadMonitoringState::GetSingleton().Set(false);

  StopGamepadMonitoring();
  ResetGamepadIndexes();
  MaybeShutdown();
}

void GamepadPlatformService::MaybeShutdown() {
  // This method is invoked in MaybeStopGamepadMonitoring when
  // an IPDL channel is going to be destroyed
  AssertIsOnBackgroundThread();

  // We have to release gGamepadPlatformServiceSingleton ouside
  // the mutex as well as making upcoming GetParentService() call
  // recreate new singleton, so we use this RefPtr to temporarily
  // hold the reference, postponing the release process until this
  // method ends.
  RefPtr<GamepadPlatformService> kungFuDeathGrip;

  bool isChannelParentEmpty;
  {
    MutexAutoLock autoLock(mMutex);
    isChannelParentEmpty = mChannelParents.IsEmpty();
    if (isChannelParentEmpty) {
      kungFuDeathGrip = gGamepadPlatformServiceSingleton;
      gGamepadPlatformServiceSingleton = nullptr;
      mGamepadAdded.clear();
    }
  }
}

void GamepadPlatformService::Cleanup() {
  // This method is called when GamepadPlatformService is
  // successfully distructed in background thread
  AssertIsOnBackgroundThread();

  MutexAutoLock autoLock(mMutex);
  mChannelParents.Clear();
}

}  // namespace mozilla::dom
