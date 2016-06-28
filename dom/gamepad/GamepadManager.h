/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadManager_h_
#define mozilla_dom_GamepadManager_h_

#include "nsIIPCBackgroundChildCreateCallback.h"
#include "nsIObserver.h"
// Needed for GamepadMappingType
#include "mozilla/dom/GamepadBinding.h"

class nsGlobalWindow;

namespace mozilla {
namespace dom {

class EventTarget;
class Gamepad;
class GamepadChangeEvent;
class GamepadEventChannelChild;

class GamepadManager final : public nsIObserver,
                             public nsIIPCBackgroundChildCreateCallback
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  // Returns true if we actually have a service up and running
  static bool IsServiceRunning();
  // Get the singleton service
  static already_AddRefed<GamepadManager> GetService();
  // Return true if the API is preffed on.
  static bool IsAPIEnabled();

  void BeginShutdown();
  void StopMonitoring();

  // Indicate that |aWindow| wants to receive gamepad events.
  void AddListener(nsGlobalWindow* aWindow);
  // Indicate that |aWindow| should no longer receive gamepad events.
  void RemoveListener(nsGlobalWindow* aWindow);

  // Add a gamepad to the list of known gamepads.
  void AddGamepad(uint32_t aIndex, const nsAString& aID, GamepadMappingType aMapping,
                  uint32_t aNumButtons, uint32_t aNumAxes);

  // Remove the gamepad at |aIndex| from the list of known gamepads.
  void RemoveGamepad(uint32_t aIndex);

  // Update the state of |aButton| for the gamepad at |aIndex| for all
  // windows that are listening and visible, and fire one of
  // a gamepadbutton{up,down} event at them as well.
  // aPressed is used for digital buttons, aValue is for analog buttons.
  void NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed,
                      double aValue);

  // Update the state of |aAxis| for the gamepad at |aIndex| for all
  // windows that are listening and visible, and fire a gamepadaxismove
  // event at them as well.
  void NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis, double aValue);

  // Synchronize the state of |aGamepad| to match the gamepad stored at |aIndex|
  void SyncGamepadState(uint32_t aIndex, Gamepad* aGamepad);

  // Returns gamepad object if index exists, null otherwise
  already_AddRefed<Gamepad> GetGamepad(uint32_t aIndex) const;

  // Receive GamepadChangeEvent messages from parent process to fire DOM events
  void Update(const GamepadChangeEvent& aGamepadEvent);

 protected:
  GamepadManager();
  ~GamepadManager() {};

  // Fire a gamepadconnected or gamepaddisconnected event for the gamepad
  // at |aIndex| to all windows that are listening and have received
  // gamepad input.
  void NewConnectionEvent(uint32_t aIndex, bool aConnected);

  // Fire a gamepadaxismove event to the window at |aTarget| for |aGamepad|.
  void FireAxisMoveEvent(EventTarget* aTarget,
                         Gamepad* aGamepad,
                         uint32_t axis,
                         double value);

  // Fire one of gamepadbutton{up,down} event at the window at |aTarget| for
  // |aGamepad|.
  void FireButtonEvent(EventTarget* aTarget,
                       Gamepad* aGamepad,
                       uint32_t aButton,
                       double aValue);

  // Fire one of gamepad{connected,disconnected} event at the window at
  // |aTarget| for |aGamepad|.
  void FireConnectionEvent(EventTarget* aTarget,
                           Gamepad* aGamepad,
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
  // don't need to refcount it here
  GamepadEventChannelChild MOZ_NON_OWNING_REF *mChild;

 private:

  nsresult Init();

  bool MaybeWindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex);
  // Returns true if we have already sent data from this gamepad
  // to this window. This should only return true if the user
  // explicitly interacted with a gamepad while this window
  // was focused, by pressing buttons or similar actions.
  bool WindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex) const;
  // Indicate that a window has recieved data from a gamepad.
  void SetWindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex,
                               bool aHasSeen = true);

  // Gamepads connected to the system. Copies of these are handed out
  // to each window.
  nsRefPtrHashtable<nsUint32HashKey, Gamepad> mGamepads;
  // Inner windows that are listening for gamepad events.
  // has been sent to that window.
  nsTArray<RefPtr<nsGlobalWindow>> mListeners;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_GamepadManager_h_
