/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadPlatformService_h_
#define mozilla_dom_GamepadPlatformService_h_

#include "mozilla/dom/GamepadBinding.h"

#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {

class GamepadEventChannelParent;

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
class GamepadPlatformService final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadPlatformService)
 public:
  //Get the singleton service
  static already_AddRefed<GamepadPlatformService> GetParentService();

  // Add a gamepad to the list of known gamepads, and return its index.
  uint32_t AddGamepad(const char* aID, GamepadMappingType aMapping,
                      GamepadHand aHand, uint32_t aNumButtons,
                      uint32_t aNumAxes, uint32_t aNumHaptics);
  // Remove the gamepad at |aIndex| from the list of known gamepads.
  void RemoveGamepad(uint32_t aIndex);

  // Update the state of |aButton| for the gamepad at |aIndex| for all
  // windows that are listening and visible, and fire one of
  // a gamepadbutton{up,down} event at them as well.
  // aPressed is used for digital buttons, aValue is for analog buttons.
  void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed,
                      double aValue);
  // When only a digital button is available the value will be synthesized.
  void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed);

  // Update the state of |aAxis| for the gamepad at |aIndex| for all
  // windows that are listening and visible, and fire a gamepadaxismove
  // event at them as well.
  void NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis, double aValue);
  // Update the state of |aState| for the gamepad at |aIndex| for all
  // windows that are listening and visible.
  void NewPoseEvent(uint32_t aIndex, const GamepadPoseState& aState);

  // When shutting down the platform communications for gamepad, also reset the
  // indexes.
  void ResetGamepadIndexes();

  //Add IPDL parent instance
  void AddChannelParent(GamepadEventChannelParent* aParent);

  //Remove IPDL parent instance
  void RemoveChannelParent(GamepadEventChannelParent* aParent);

  bool HasGamepadListeners();

  void MaybeShutdown();

 private:
  GamepadPlatformService();
  ~GamepadPlatformService();
  template<class T> void NotifyGamepadChange(const T& aInfo);

  // Flush all pending events buffered in mPendingEvents, must be called
  // with mMutex held
  void FlushPendingEvents();
  void Cleanup();

  // mGamepadIndex can only be accessed by monitor thread
  uint32_t mGamepadIndex;

  // mChannelParents stores all the GamepadEventChannelParent instances
  // which may be accessed by both background thread and monitor thread
  // simultaneously, so we have a mutex to prevent race condition
  nsTArray<RefPtr<GamepadEventChannelParent>> mChannelParents;

  // This mutex protects mChannelParents from race condition
  // between background and monitor thread
  Mutex mMutex;

  // In mochitest, it is possible that the test Events is synthesized
  // before GamepadEventChannel created, we need to buffer all events
  // until the channel is created if that happens.
  nsTArray<GamepadChangeEvent> mPendingEvents;
};

} // namespace dom
} // namespace mozilla

#endif
