/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadService_h_
#define mozilla_dom_GamepadService_h_

#include <stdint.h>
#include "Gamepad.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIGamepadServiceTest.h"
#include "nsGlobalWindow.h"
#include "nsIFocusManager.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsTArray.h"

class nsIDOMDocument;

namespace mozilla {
namespace dom {

class EventTarget;

class GamepadService : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Get the singleton service
  static already_AddRefed<GamepadService> GetService();
  // Return true if the API is preffed on.
  static bool IsAPIEnabled();

  void BeginShutdown();

  // Indicate that |aWindow| wants to receive gamepad events.
  void AddListener(nsGlobalWindow* aWindow);
  // Indicate that |aWindow| should no longer receive gamepad events.
  void RemoveListener(nsGlobalWindow* aWindow);

  // Add a gamepad to the list of known gamepads, and return its index.
  uint32_t AddGamepad(const char* aID, GamepadMappingType aMapping,
                      uint32_t aNumButtons, uint32_t aNumAxes);
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

  // Synchronize the state of |aGamepad| to match the gamepad stored at |aIndex|
  void SyncGamepadState(uint32_t aIndex, Gamepad* aGamepad);

 protected:
  GamepadService();
  virtual ~GamepadService() {};
  void StartCleanupTimer();

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
  // true if the platform-specific backend has started work
  bool mStarted;
  // true when shutdown has begun
  bool mShuttingDown;

 private:
  // Returns true if we have already sent data from this gamepad
  // to this window. This should only return true if the user
  // explicitly interacted with a gamepad while this window
  // was focused, by pressing buttons or similar actions.
  bool WindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex);
  // Indicate that a window has recieved data from a gamepad.
  void SetWindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex,
                               bool aHasSeen = true);

  static void TimeoutHandler(nsITimer* aTimer, void* aClosure);
  static bool sShutdown;

  // Gamepads connected to the system. Copies of these are handed out
  // to each window.
  nsTArray<nsRefPtr<Gamepad> > mGamepads;
  // nsGlobalWindows that are listening for gamepad events.
  // has been sent to that window.
  nsTArray<nsRefPtr<nsGlobalWindow> > mListeners;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIFocusManager> mFocusManager;
  nsCOMPtr<nsIObserver> mObserver;
};

// Service for testing purposes
class GamepadServiceTest : public nsIGamepadServiceTest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGAMEPADSERVICETEST

  GamepadServiceTest();

  static already_AddRefed<GamepadServiceTest> CreateService();

private:
  static GamepadServiceTest* sSingleton;
  virtual ~GamepadServiceTest() {};
};

}  // namespace dom
}  // namespace mozilla

#define NS_GAMEPAD_TEST_CID \
{ 0xfb1fcb57, 0xebab, 0x4cf4, \
{ 0x96, 0x3b, 0x1e, 0x4d, 0xb8, 0x52, 0x16, 0x96 } }
#define NS_GAMEPAD_TEST_CONTRACTID "@mozilla.org/gamepad-test;1"

#endif // mozilla_dom_GamepadService_h_
