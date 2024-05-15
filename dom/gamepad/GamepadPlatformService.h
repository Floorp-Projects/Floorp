/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadPlatformService_h_
#define mozilla_dom_GamepadPlatformService_h_

#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadHandle.h"

#include <map>
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::dom {

class GamepadEventChannelParent;
enum class GamepadLightIndicatorType : uint8_t;
struct GamepadPoseState;
class GamepadTestChannelParent;
struct GamepadTouchState;
class GamepadPlatformService;

class GamepadMonitoringState {
 public:
  static GamepadMonitoringState& GetSingleton();

  void AddObserver(GamepadTestChannelParent* aParent);
  void RemoveObserver(GamepadTestChannelParent* aParent);

  bool IsMonitoring() const;

  GamepadMonitoringState(const GamepadMonitoringState&) = delete;
  GamepadMonitoringState(GamepadMonitoringState&&) = delete;
  GamepadMonitoringState& operator=(const GamepadMonitoringState) = delete;
  GamepadMonitoringState& operator=(GamepadMonitoringState&&) = delete;

 private:
  GamepadMonitoringState() = default;
  ~GamepadMonitoringState() = default;

  void Set(bool aIsMonitoring);

  bool mIsMonitoring{false};
  Vector<WeakPtr<GamepadTestChannelParent>> mObservers;

  friend class mozilla::dom::GamepadPlatformService;
};

// Platform Service for building and transmitting IPDL messages
// through the HAL sandbox. Used by platform specific
// Gamepad implementations
//
// This class can be accessed by the following 2 threads :
// 1. Background thread:
//    This thread takes charge of IPDL communications
//    between here and DOM side
//
// 2. Monitor Thread:
//    This thread is populated in platform-dependent backends, which
//    is in charge of processing gamepad hardware events from OS
class GamepadPlatformService final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadPlatformService)
 public:
  // Get the singleton service
  static already_AddRefed<GamepadPlatformService> GetParentService();

  // Add a gamepad to the list of known gamepads, and return its handle.
  GamepadHandle AddGamepad(const char* aID, GamepadMappingType aMapping,
                           GamepadHand aHand, uint32_t aNumButtons,
                           uint32_t aNumAxes, uint32_t aNumHaptics,
                           uint32_t aNumLightIndicator,
                           uint32_t aNumTouchEvents);
  // Remove the gamepad at |aHandle| from the list of known gamepads.
  void RemoveGamepad(GamepadHandle aHandle);

  // Update the state of |aButton| for the gamepad at |aHandle| for all
  // windows that are listening and visible, and fire one of
  // a gamepadbutton{up,down} event at them as well.
  // aPressed is used for digital buttons, aTouched is for detecting touched
  // events, aValue is for analog buttons.
  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed,
                      bool aTouched, double aValue);
  // When only a digital button is available the value will be synthesized.
  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed);
  // When only a digital button are available the value will be synthesized.
  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed,
                      bool aTouched);
  // When only a digital button are available the value will be synthesized.
  void NewButtonEvent(GamepadHandle aHandle, uint32_t aButton, bool aPressed,
                      double aValue);
  // Update the state of |aAxis| for the gamepad at |aHandle| for all
  // windows that are listening and visible, and fire a gamepadaxismove
  // event at them as well.
  void NewAxisMoveEvent(GamepadHandle aHandle, uint32_t aAxis, double aValue);
  // Update the state of |aState| for the gamepad at |aHandle| for all
  // windows that are listening and visible.
  void NewPoseEvent(GamepadHandle aHandle, const GamepadPoseState& aState);
  // Update the type of |aType| for the gamepad at |aHandle| for all
  // windows that are listening and visible.
  void NewLightIndicatorTypeEvent(GamepadHandle aHandle, uint32_t aLight,
                                  GamepadLightIndicatorType aType);
  // Update the state of |aState| for the gamepad at |aHandle| with
  // |aTouchArrayIndex| for all windows that are listening and visible.
  void NewMultiTouchEvent(GamepadHandle aHandle, uint32_t aTouchArrayIndex,
                          const GamepadTouchState& aState);

  // When shutting down the platform communications for gamepad, also reset the
  // indexes.
  void ResetGamepadIndexes();

  // Add IPDL parent instance
  void AddChannelParent(GamepadEventChannelParent* aParent);

  // Remove IPDL parent instance
  void RemoveChannelParent(GamepadEventChannelParent* aParent);

  void MaybeShutdown();

  nsTArray<GamepadAdded> GetAllGamePads() {
    nsTArray<GamepadAdded> gamepads;

    for (const auto& elem : mGamepadAdded) {
      gamepads.AppendElement(elem.second);
    }
    return gamepads;
  }

 private:
  GamepadPlatformService();
  ~GamepadPlatformService();
  template <class T>
  void NotifyGamepadChange(GamepadHandle aHandle, const T& aInfo);

  void Cleanup();

  // mNextGamepadHandleValue can only be accessed by monitor thread
  uint32_t mNextGamepadHandleValue;

  // mChannelParents stores all the GamepadEventChannelParent instances
  // which may be accessed by both background thread and monitor thread
  // simultaneously, so we have a mutex to prevent race condition
  nsTArray<RefPtr<GamepadEventChannelParent>> mChannelParents;

  // This mutex protects mChannelParents from race condition
  // between background and monitor thread
  Mutex mMutex MOZ_UNANNOTATED;

  std::map<GamepadHandle, GamepadAdded> mGamepadAdded;
};

}  // namespace mozilla::dom

#endif
