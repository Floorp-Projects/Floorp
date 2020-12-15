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
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsHashKeys.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

namespace {

// This is the singleton instance of GamepadPlatformService, can be called
// by both background and monitor thread.
static StaticMutex gGamepadPlatformServiceSingletonMutex;
static StaticRefPtr<GamepadPlatformService> gGamepadPlatformServiceSingleton;

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

// This class is created to service one-or-more event channels
// This implies an invariant - There is always at-least one event channel while
// this class is alive. It should be created with a channel, and it should be
// destroyed when the last channel is to be removed.
GamepadPlatformService::GamepadPlatformService(
    RefPtr<GamepadEventChannelParent> aParent)
    : mNextGamepadHandleValue(1),
      mMutex("mozilla::dom::GamepadPlatformService") {
  AssertIsOnBackgroundThread();
  mChannelParents.AppendElement(std::move(aParent));
  MOZ_ASSERT(mChannelParents.Length() == 1);
}

GamepadPlatformService::~GamepadPlatformService() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mChannelParents.Length() == 1);
}

// static
already_AddRefed<GamepadPlatformService>
GamepadPlatformService::GetParentService() {
  // GamepadPlatformService can only be accessed in parent process
  MOZ_ASSERT(XRE_IsParentProcess());

  // TODO - Remove this mutex once Bug 1682554 is fixed
  StaticMutexAutoLock lock(gGamepadPlatformServiceSingletonMutex);

  // TODO - Turn this back into an assertion after Bug 1682554 is fixed
  if (!gGamepadPlatformServiceSingleton) {
    return nullptr;
  }

  return RefPtr<GamepadPlatformService>(gGamepadPlatformServiceSingleton)
      .forget();
}

template <class T>
void GamepadPlatformService::NotifyGamepadChange(
    GamepadHandle aHandle, const T& aInfo, const MutexAutoLock& aProofOfLock) {
  // This method is called by monitor populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  aProofOfLock.AssertOwns(mMutex);

  GamepadChangeEventBody body(aInfo);
  GamepadChangeEvent e(aHandle, body);

  for (uint32_t i = 0; i < mChannelParents.Length(); ++i) {
    mChannelParents[i]->DispatchUpdateEvent(e);
  }
}

GamepadHandle GamepadPlatformService::AddGamepad(
    const char* aID, GamepadMappingType aMapping, GamepadHand aHand,
    uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aNumHaptics,
    uint32_t aNumLightIndicator, uint32_t aNumTouchEvents) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadHandle gamepadHandle{mNextGamepadHandleValue++,
                              GamepadHandleKind::GamepadPlatformManager};

  // Only VR controllers has displayID, we give 0 to the general gamepads.
  GamepadAdded a(NS_ConvertUTF8toUTF16(nsDependentCString(aID)), aMapping,
                 aHand, 0, aNumButtons, aNumAxes, aNumHaptics,
                 aNumLightIndicator, aNumTouchEvents);

  MutexAutoLock autoLock(mMutex);
  mGamepadAdded.emplace(gamepadHandle, a);
  NotifyGamepadChange<GamepadAdded>(gamepadHandle, a, autoLock);

  return gamepadHandle;
}

void GamepadPlatformService::RemoveGamepad(GamepadHandle aHandle) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadRemoved a;

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadRemoved>(aHandle, a, autoLock);
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

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadButtonInformation>(aHandle, a, autoLock);
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

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadAxisInformation>(aHandle, a, autoLock);
}

void GamepadPlatformService::NewLightIndicatorTypeEvent(
    GamepadHandle aHandle, uint32_t aLight, GamepadLightIndicatorType aType) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadLightIndicatorTypeInformation a(aLight, aType);

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadLightIndicatorTypeInformation>(aHandle, a,
                                                            autoLock);
}

void GamepadPlatformService::NewPoseEvent(GamepadHandle aHandle,
                                          const GamepadPoseState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadPoseInformation a(aState);

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadPoseInformation>(aHandle, a, autoLock);
}

void GamepadPlatformService::NewMultiTouchEvent(
    GamepadHandle aHandle, uint32_t aTouchArrayIndex,
    const GamepadTouchState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadTouchInformation a(aTouchArrayIndex, aState);

  MutexAutoLock autoLock(mMutex);
  NotifyGamepadChange<GamepadTouchInformation>(aHandle, a, autoLock);
}

void GamepadPlatformService::AddChannelParentInternal(
    const RefPtr<GamepadEventChannelParent>& aParent) {
  MutexAutoLock autoLock(mMutex);

  MOZ_RELEASE_ASSERT(!mChannelParents.Contains(aParent));
  mChannelParents.AppendElement(aParent);

  // Inform the new channel of all the gamepads that have already been added
  for (const auto& evt : mGamepadAdded) {
    GamepadChangeEventBody body(evt.second);
    GamepadChangeEvent e(evt.first, body);
    aParent->DispatchUpdateEvent(e);
  }
}

bool GamepadPlatformService::RemoveChannelParentInternal(
    GamepadEventChannelParent* aParent) {
  MutexAutoLock autoLock(mMutex);

  MOZ_RELEASE_ASSERT(mChannelParents.Contains(aParent));

  // If there is only one channel left, we destroy the singleton instead of
  // unregistering the channel
  if (mChannelParents.Length() == 1) {
    return false;
  }

  mChannelParents.RemoveElement(aParent);
  return true;
}

// static
void GamepadPlatformService::AddChannelParent(
    const RefPtr<GamepadEventChannelParent>& aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  {
    StaticMutexAutoLock lock(gGamepadPlatformServiceSingletonMutex);

    if (gGamepadPlatformServiceSingleton) {
      gGamepadPlatformServiceSingleton->AddChannelParentInternal(aParent);
      return;
    }

    gGamepadPlatformServiceSingleton =
        RefPtr<GamepadPlatformService>(new GamepadPlatformService{aParent});
  }

  StartGamepadMonitoring();
  GamepadMonitoringState::GetSingleton().Set(true);
}

// static
void GamepadPlatformService::RemoveChannelParent(
    GamepadEventChannelParent* aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  {
    StaticMutexAutoLock lock(gGamepadPlatformServiceSingletonMutex);

    MOZ_RELEASE_ASSERT(gGamepadPlatformServiceSingleton);

    // RemoveChannelParentInternal will refuse to remove the last channel
    // In that case, we should destroy the singleton
    if (gGamepadPlatformServiceSingleton->RemoveChannelParentInternal(
            aParent)) {
      return;
    }
  }

  GamepadMonitoringState::GetSingleton().Set(false);
  StopGamepadMonitoring();

  StaticMutexAutoLock lock(gGamepadPlatformServiceSingletonMutex);

  // We should never be destroying the singleton with event channels left in it
  MOZ_RELEASE_ASSERT(
      gGamepadPlatformServiceSingleton->mChannelParents.Length() == 1);

  // The only reference to the singleton should be the global one
  MOZ_RELEASE_ASSERT(gGamepadPlatformServiceSingleton->mRefCnt.get() == 1);

  gGamepadPlatformServiceSingleton = nullptr;
}

}  // namespace mozilla::dom
