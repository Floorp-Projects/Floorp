/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadPlatformService.h"

#include "mozilla/dom/GamepadEventChannelParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Mutex.h"
#include "mozilla/Unused.h"

#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIThread.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

namespace {

// This is the singleton instance of GamepadPlatformService, can be called
// by both background and monitor thread.
StaticRefPtr<GamepadPlatformService> gGamepadPlatformServiceSingleton;

}  // namespace

GamepadPlatformService::GamepadPlatformService()
    : mGamepadIndex(0), mMutex("mozilla::dom::GamepadPlatformService") {}

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
void GamepadPlatformService::NotifyGamepadChange(uint32_t aIndex,
                                                 const T& aInfo) {
  // This method is called by monitor populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadChangeEventBody body(aInfo);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  // mChannelParents may be accessed by background thread in the
  // same time, we use mutex to prevent possible race condtion
  MutexAutoLock autoLock(mMutex);

  // Buffer all events if we have no Channel to dispatch, which
  // may happen when performing Mochitest.
  if (mChannelParents.IsEmpty()) {
    mPendingEvents.AppendElement(e);
    return;
  }

  for (uint32_t i = 0; i < mChannelParents.Length(); ++i) {
    mChannelParents[i]->DispatchUpdateEvent(e);
  }
}

uint32_t GamepadPlatformService::AddGamepad(
    const char* aID, GamepadMappingType aMapping, GamepadHand aHand,
    uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aHaptics,
    uint32_t aNumLightIndicator, uint32_t aNumTouchEvents) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  uint32_t index = ++mGamepadIndex;

  // Only VR controllers has displayID, we give 0 to the general gamepads.
  GamepadAdded a(NS_ConvertUTF8toUTF16(nsDependentCString(aID)), aMapping,
                 aHand, 0, aNumButtons, aNumAxes, aHaptics, aNumLightIndicator,
                 aNumTouchEvents);

  NotifyGamepadChange<GamepadAdded>(index, a);
  return index;
}

void GamepadPlatformService::RemoveGamepad(uint32_t aIndex) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadRemoved a;
  NotifyGamepadChange<GamepadRemoved>(aIndex, a);
}

void GamepadPlatformService::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                            bool aPressed, bool aTouched,
                                            double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadButtonInformation a(aButton, aValue, aPressed, aTouched);
  NotifyGamepadChange<GamepadButtonInformation>(aIndex, a);
}

void GamepadPlatformService::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                            bool aPressed, double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aIndex, aButton, aPressed, aPressed, aValue);
}

void GamepadPlatformService::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                            bool aPressed, bool aTouched) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aIndex, aButton, aPressed, aTouched, aPressed ? 1.0L : 0.0L);
}

void GamepadPlatformService::NewButtonEvent(uint32_t aIndex, uint32_t aButton,
                                            bool aPressed) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aIndex, aButton, aPressed, aPressed, aPressed ? 1.0L : 0.0L);
}

void GamepadPlatformService::NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                              double aValue) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadAxisInformation a(aAxis, aValue);
  NotifyGamepadChange<GamepadAxisInformation>(aIndex, a);
}

void GamepadPlatformService::NewLightIndicatorTypeEvent(
    uint32_t aIndex, uint32_t aLight, GamepadLightIndicatorType aType) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadLightIndicatorTypeInformation a(aLight, aType);
  NotifyGamepadChange<GamepadLightIndicatorTypeInformation>(aIndex, a);
}

void GamepadPlatformService::NewPoseEvent(uint32_t aIndex,
                                          const GamepadPoseState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  GamepadPoseInformation a(aState);
  NotifyGamepadChange<GamepadPoseInformation>(aIndex, a);
}

void GamepadPlatformService::NewMultiTouchEvent(
    uint32_t aIndex, uint32_t aTouchArrayIndex,
    const GamepadTouchState& aState) {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  GamepadTouchInformation a(aTouchArrayIndex, aState);
  NotifyGamepadChange<GamepadTouchInformation>(aIndex, a);
}

void GamepadPlatformService::ResetGamepadIndexes() {
  // This method is called by monitor thread populated in
  // platform-dependent backends
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());
  mGamepadIndex = 0;
}

void GamepadPlatformService::AddChannelParent(
    GamepadEventChannelParent* aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mChannelParents.Contains(aParent));

  // We use mutex here to prevent race condition with monitor thread
  MutexAutoLock autoLock(mMutex);
  mChannelParents.AppendElement(aParent);
  FlushPendingEvents();
}

void GamepadPlatformService::FlushPendingEvents() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChannelParents.IsEmpty());

  if (mPendingEvents.IsEmpty()) {
    return;
  }

  // NOTE: This method must be called with mMutex held because it accesses
  // mChannelParents.
  for (uint32_t i = 0; i < mChannelParents.Length(); ++i) {
    for (uint32_t j = 0; j < mPendingEvents.Length(); ++j) {
      mChannelParents[i]->DispatchUpdateEvent(mPendingEvents[j]);
    }
  }
  mPendingEvents.Clear();
}

void GamepadPlatformService::RemoveChannelParent(
    GamepadEventChannelParent* aParent) {
  // mChannelParents can only be modified once GamepadEventChannelParent
  // is created or removed in Background thread
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mChannelParents.Contains(aParent));

  // We use mutex here to prevent race condition with monitor thread
  MutexAutoLock autoLock(mMutex);
  mChannelParents.RemoveElement(aParent);
}

bool GamepadPlatformService::HasGamepadListeners() {
  // mChannelParents may be accessed by background thread in the
  // same time, we use mutex to prevent possible race condtion
  AssertIsOnBackgroundThread();

  // We use mutex here to prevent race condition with monitor thread
  MutexAutoLock autoLock(mMutex);
  for (uint32_t i = 0; i < mChannelParents.Length(); i++) {
    if (mChannelParents[i]->HasGamepadListener()) {
      return true;
    }
  }
  return false;
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

}  // namespace dom
}  // namespace mozilla
