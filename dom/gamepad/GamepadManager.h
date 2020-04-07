/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadManager_h_
#define mozilla_dom_GamepadManager_h_

#include "nsIObserver.h"
// Needed for GamepadMappingType
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadServiceType.h"

class nsGlobalWindowInner;

namespace mozilla {
namespace gfx {
class VRManagerChild;
}  // namespace gfx
namespace dom {

class EventTarget;
class Gamepad;
class GamepadChangeEvent;
class GamepadEventChannelChild;

class GamepadManager final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Returns true if we actually have a service up and running
  static bool IsServiceRunning();
  // Get the singleton service
  static already_AddRefed<GamepadManager> GetService();

  // Our gamepad index has VR_GAMEPAD_IDX_OFFSET while GamepadChannelType
  // is from VRManager.
  static uint32_t GetGamepadIndexWithServiceType(
      uint32_t aIndex, GamepadServiceType aServiceType);

  void BeginShutdown();
  void StopMonitoring();

  // Indicate that |aWindow| wants to receive gamepad events.
  void AddListener(nsGlobalWindowInner* aWindow);
  // Indicate that |aWindow| should no longer receive gamepad events.
  void RemoveListener(nsGlobalWindowInner* aWindow);

  // Add a gamepad to the list of known gamepads.
  void AddGamepad(uint32_t aIndex, const nsAString& aID,
                  GamepadMappingType aMapping, GamepadHand aHand,
                  GamepadServiceType aServiceType, uint32_t aDisplayID,
                  uint32_t aNumButtons, uint32_t aNumAxes, uint32_t aNumHaptics,
                  uint32_t aNumLightIndicator, uint32_t aNumTouchEvents);

  // Remove the gamepad at |aIndex| from the list of known gamepads.
  void RemoveGamepad(uint32_t aIndex, GamepadServiceType aServiceType);

  // Synchronize the state of |aGamepad| to match the gamepad stored at |aIndex|
  void SyncGamepadState(uint32_t aIndex, nsGlobalWindowInner* aWindow,
                        Gamepad* aGamepad);

  // Returns gamepad object if index exists, null otherwise
  already_AddRefed<Gamepad> GetGamepad(uint32_t aIndex) const;

  // Returns gamepad object if GamepadId exists, null otherwise
  already_AddRefed<Gamepad> GetGamepad(uint32_t aGamepadId,
                                       GamepadServiceType aServiceType) const;

  // Receive GamepadChangeEvent messages from parent process to fire DOM events
  void Update(const GamepadChangeEvent& aGamepadEvent);

  // Trigger vibrate haptic event to gamepad channels.
  already_AddRefed<Promise> VibrateHaptic(uint32_t aControllerIdx,
                                          uint32_t aHapticIndex,
                                          double aIntensity, double aDuration,
                                          nsIGlobalObject* aGlobal,
                                          ErrorResult& aRv);
  // Send stop haptic events to gamepad channels.
  void StopHaptics();

  // Set light indicator color event to gamepad channels.
  already_AddRefed<Promise> SetLightIndicatorColor(uint32_t aControllerIdx,
                                                   uint32_t aLightColorIndex,
                                                   uint8_t aRed, uint8_t aGreen,
                                                   uint8_t aBlue,
                                                   nsIGlobalObject* aGlobal,
                                                   ErrorResult& aRv);

 protected:
  GamepadManager();
  ~GamepadManager() = default;

  // Fire a gamepadconnected or gamepaddisconnected event for the gamepad
  // at |aIndex| to all windows that are listening and have received
  // gamepad input.
  void NewConnectionEvent(uint32_t aIndex, bool aConnected);

  // Fire a gamepadaxismove event to the window at |aTarget| for |aGamepad|.
  void FireAxisMoveEvent(EventTarget* aTarget, Gamepad* aGamepad, uint32_t axis,
                         double value);

  // Fire one of gamepadbutton{up,down} event at the window at |aTarget| for
  // |aGamepad|.
  void FireButtonEvent(EventTarget* aTarget, Gamepad* aGamepad,
                       uint32_t aButton, double aValue);

  // Fire one of gamepad{connected,disconnected} event at the window at
  // |aTarget| for |aGamepad|.
  void FireConnectionEvent(EventTarget* aTarget, Gamepad* aGamepad,
                           bool aConnected);

  // true if this feature is enabled in preferences
  bool mEnabled;
  // true if non-standard events are enabled in preferences
  bool mNonstandardEventsEnabled;
  // true when shutdown has begun
  bool mShuttingDown;

  // Gamepad IPDL child
  // This pointer is only used by this singleton instance and
  // will be destroyed during the IPDL shutdown chain, so we
  // don't need to refcount it here.
  nsTArray<GamepadEventChannelChild*> mChannelChildren;

 private:
  nsresult Init();

  void MaybeConvertToNonstandardGamepadEvent(const GamepadChangeEvent& aEvent,
                                             nsGlobalWindowInner* aWindow);

  bool SetGamepadByEvent(const GamepadChangeEvent& aEvent,
                         nsGlobalWindowInner* aWindow = nullptr);

  bool MaybeWindowHasSeenGamepad(nsGlobalWindowInner* aWindow, uint32_t aIndex);
  // Returns true if we have already sent data from this gamepad
  // to this window. This should only return true if the user
  // explicitly interacted with a gamepad while this window
  // was focused, by pressing buttons or similar actions.
  bool WindowHasSeenGamepad(nsGlobalWindowInner* aWindow,
                            uint32_t aIndex) const;
  // Indicate that a window has received data from a gamepad.
  void SetWindowHasSeenGamepad(nsGlobalWindowInner* aWindow, uint32_t aIndex,
                               bool aHasSeen = true);

  // Gamepads connected to the system. Copies of these are handed out
  // to each window.
  nsRefPtrHashtable<nsUint32HashKey, Gamepad> mGamepads;
  // Inner windows that are listening for gamepad events.
  // has been sent to that window.
  nsTArray<RefPtr<nsGlobalWindowInner>> mListeners;
  uint32_t mPromiseID;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_GamepadManager_h_
