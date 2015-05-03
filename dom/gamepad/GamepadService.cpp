/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadService.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadAxisMoveEvent.h"
#include "mozilla/dom/GamepadButtonEvent.h"
#include "mozilla/dom/GamepadEvent.h"
#include "mozilla/dom/GamepadMonitoring.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"

#include "nsAutoPtr.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

#include <cstddef>

namespace mozilla {
namespace dom {

namespace {
const char* kGamepadEnabledPref = "dom.gamepad.enabled";
const char* kGamepadEventsEnabledPref =
  "dom.gamepad.non_standard_events.enabled";
// Amount of time to wait before cleaning up gamepad resources
// when no pages are listening for events.
const int kCleanupDelayMS = 2000;
const nsTArray<nsRefPtr<nsGlobalWindow> >::index_type NoIndex =
    nsTArray<nsRefPtr<nsGlobalWindow> >::NoIndex;

StaticRefPtr<GamepadService> gGamepadServiceSingleton;

} // namespace

bool GamepadService::sShutdown = false;

NS_IMPL_ISUPPORTS(GamepadService, nsIObserver)

GamepadService::GamepadService()
  : mStarted(false),
    mShuttingDown(false)
{
  mEnabled = IsAPIEnabled();
  mNonstandardEventsEnabled =
    Preferences::GetBool(kGamepadEventsEnabledPref, false);
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->AddObserver(this,
                               NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                               false);
}

NS_IMETHODIMP
GamepadService::Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  observerService->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);

  BeginShutdown();
  return NS_OK;
}

void
GamepadService::BeginShutdown()
{
  mShuttingDown = true;
  if (mTimer) {
    mTimer->Cancel();
  }
  if (mStarted) {
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      MaybeStopGamepadMonitoring();
    } else {
      ContentChild::GetSingleton()->SendGamepadListenerRemoved();
    }
    mStarted = false;
  }
  // Don't let windows call back to unregister during shutdown
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->SetHasGamepadEventListener(false);
  }
  mListeners.Clear();
  mGamepads.Clear();
  sShutdown = true;
}

void
GamepadService::AddListener(nsGlobalWindow* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());
  if (mShuttingDown) {
    return;
  }

  if (mListeners.IndexOf(aWindow) != NoIndex) {
    return; // already exists
  }

  if (!mStarted && mEnabled) {
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      StartGamepadMonitoring();
    } else {
      ContentChild::GetSingleton()->SendGamepadListenerAdded();
    }
    mStarted = true;
  }
  mListeners.AppendElement(aWindow);
}

void
GamepadService::RemoveListener(nsGlobalWindow* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  if (mShuttingDown) {
    // Doesn't matter at this point. It's possible we're being called
    // as a result of our own destructor here, so just bail out.
    return;
  }

  if (mListeners.IndexOf(aWindow) == NoIndex) {
    return; // doesn't exist
  }

  mListeners.RemoveElement(aWindow);

  if (mListeners.Length() == 0 && !mShuttingDown && mStarted) {
    StartCleanupTimer();
  }
}

already_AddRefed<Gamepad>
GamepadService::GetGamepad(uint32_t aIndex)
{
  nsRefPtr<Gamepad> gamepad;
  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

void
GamepadService::AddGamepad(uint32_t aIndex,
                           const nsAString& aId,
                           GamepadMappingType aMapping,
                           uint32_t aNumButtons,
                           uint32_t aNumAxes)
{
  //TODO: bug 852258: get initial button/axis state
  nsRefPtr<Gamepad> gamepad =
    new Gamepad(nullptr,
                aId,
                0, // index is set by global window
                aMapping,
                aNumButtons,
                aNumAxes);

  // We store the gamepad related to its index given by the parent process.
  mGamepads.Put(aIndex, gamepad);
  NewConnectionEvent(aIndex, true);
}

void
GamepadService::RemoveGamepad(uint32_t aIndex)
{
  nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (!gamepad) {
    NS_WARNING("Trying to delete gamepad with invalid index");
    return;
  }
  gamepad->SetConnected(false);
    NewConnectionEvent(aIndex, false);
  mGamepads.Remove(aIndex);
}

void
GamepadService::NewButtonEvent(uint32_t aIndex, uint32_t aButton, bool aPressed,
                               double aValue)
{
  nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (mShuttingDown || !gamepad) {
    return;
  }

  gamepad->SetButton(aButton, aPressed, aValue);

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  for (uint32_t i = listeners.Length(); i > 0 ; ) {
    --i;

    // Only send events to non-background windows
    if (!listeners[i]->IsCurrentInnerWindow() ||
        listeners[i]->GetOuterWindow()->IsBackground()) {
      continue;
    }

    bool first_time = false;
    if (!WindowHasSeenGamepad(listeners[i], aIndex)) {
      // This window hasn't seen this gamepad before, so
      // send a connection event first.
      SetWindowHasSeenGamepad(listeners[i], aIndex);
      first_time = true;
    }

    nsRefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
    if (listenerGamepad) {
      listenerGamepad->SetButton(aButton, aPressed, aValue);
      if (first_time) {
        FireConnectionEvent(listeners[i], listenerGamepad, true);
      }
      if (mNonstandardEventsEnabled) {
        // Fire event
        FireButtonEvent(listeners[i], listenerGamepad, aButton, aValue);
      }
    }
  }
}

void
GamepadService::FireButtonEvent(EventTarget* aTarget,
                                Gamepad* aGamepad,
                                uint32_t aButton,
                                double aValue)
{
  nsString name = aValue == 1.0L ? NS_LITERAL_STRING("gamepadbuttondown") :
                                   NS_LITERAL_STRING("gamepadbuttonup");
  GamepadButtonEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  init.mButton = aButton;
  nsRefPtr<GamepadButtonEvent> event =
    GamepadButtonEvent::Constructor(aTarget, name, init);

  event->SetTrusted(true);

  bool defaultActionEnabled = true;
  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis, double aValue)
{
  nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (mShuttingDown || !gamepad) {
    return;
  }
  gamepad->SetAxis(aAxis, aValue);

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  for (uint32_t i = listeners.Length(); i > 0 ; ) {
    --i;

    // Only send events to non-background windows
    if (!listeners[i]->IsCurrentInnerWindow() ||
        listeners[i]->GetOuterWindow()->IsBackground()) {
      continue;
    }

    bool first_time = false;
    if (!WindowHasSeenGamepad(listeners[i], aIndex)) {
      // This window hasn't seen this gamepad before, so
      // send a connection event first.
      SetWindowHasSeenGamepad(listeners[i], aIndex);
      first_time = true;
    }

    nsRefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
    if (listenerGamepad) {
      listenerGamepad->SetAxis(aAxis, aValue);
      if (first_time) {
        FireConnectionEvent(listeners[i], listenerGamepad, true);
      }
      if (mNonstandardEventsEnabled) {
        // Fire event
        FireAxisMoveEvent(listeners[i], listenerGamepad, aAxis, aValue);
      }
    }
  }
}

void
GamepadService::FireAxisMoveEvent(EventTarget* aTarget,
                                  Gamepad* aGamepad,
                                  uint32_t aAxis,
                                  double aValue)
{
  GamepadAxisMoveEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  init.mAxis = aAxis;
  init.mValue = aValue;
  nsRefPtr<GamepadAxisMoveEvent> event =
    GamepadAxisMoveEvent::Constructor(aTarget,
                                      NS_LITERAL_STRING("gamepadaxismove"),
                                      init);

  event->SetTrusted(true);

  bool defaultActionEnabled = true;
  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::NewConnectionEvent(uint32_t aIndex, bool aConnected)
{
  nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);

  if (mShuttingDown || !gamepad) {
    return;
  }

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<nsRefPtr<nsGlobalWindow> > listeners(mListeners);

  if (aConnected) {
    for (uint32_t i = listeners.Length(); i > 0 ; ) {
      --i;

      // Only send events to non-background windows
      if (!listeners[i]->IsCurrentInnerWindow() ||
          listeners[i]->GetOuterWindow()->IsBackground()) {
        continue;
      }

      // We don't fire a connected event here unless the window
      // has seen input from at least one device.
      if (!listeners[i]->HasSeenGamepadInput()) {
        continue;
      }

      SetWindowHasSeenGamepad(listeners[i], aIndex);

      nsRefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
      if (listenerGamepad) {
        // Fire event
        FireConnectionEvent(listeners[i], listenerGamepad, aConnected);
      }
    }
  } else {
    // For disconnection events, fire one at every window that has received
    // data from this gamepad.
    for (uint32_t i = listeners.Length(); i > 0 ; ) {
      --i;

      // Even background windows get these events, so we don't have to
      // deal with the hassle of syncing the state of removed gamepads.

      if (WindowHasSeenGamepad(listeners[i], aIndex)) {
        nsRefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
        if (listenerGamepad) {
          listenerGamepad->SetConnected(false);
          // Fire event
          FireConnectionEvent(listeners[i], listenerGamepad, false);
          listeners[i]->RemoveGamepad(aIndex);
        }
      }
    }
  }
}

void
GamepadService::FireConnectionEvent(EventTarget* aTarget,
                                    Gamepad* aGamepad,
                                    bool aConnected)
{
  nsString name = aConnected ? NS_LITERAL_STRING("gamepadconnected") :
                               NS_LITERAL_STRING("gamepaddisconnected");
  GamepadEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  nsRefPtr<GamepadEvent> event =
    GamepadEvent::Constructor(aTarget, name, init);

  event->SetTrusted(true);

  bool defaultActionEnabled = true;
  aTarget->DispatchEvent(event, &defaultActionEnabled);
}

void
GamepadService::SyncGamepadState(uint32_t aIndex, Gamepad* aGamepad)
{
  nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (mShuttingDown || !mEnabled || !gamepad) {
    return;
  }

  aGamepad->SyncState(gamepad);
}

// static
bool
GamepadService::IsServiceRunning()
{
  return !!gGamepadServiceSingleton;
}

// static
already_AddRefed<GamepadService>
GamepadService::GetService()
{
  if (sShutdown) {
    return nullptr;
  }

  if (!gGamepadServiceSingleton) {
    gGamepadServiceSingleton = new GamepadService();
    ClearOnShutdown(&gGamepadServiceSingleton);
  }
  nsRefPtr<GamepadService> service(gGamepadServiceSingleton);
  return service.forget();
}

// static
bool
GamepadService::IsAPIEnabled() {
  return Preferences::GetBool(kGamepadEnabledPref, false);
}

bool
GamepadService::WindowHasSeenGamepad(nsGlobalWindow* aWindow, uint32_t aIndex)
{
  nsRefPtr<Gamepad> gamepad = aWindow->GetGamepad(aIndex);
  return gamepad != nullptr;
}

void
GamepadService::SetWindowHasSeenGamepad(nsGlobalWindow* aWindow,
                                        uint32_t aIndex,
                                        bool aHasSeen)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  if (mListeners.IndexOf(aWindow) == NoIndex) {
    // This window isn't even listening for gamepad events.
    return;
  }

  if (aHasSeen) {
    aWindow->SetHasSeenGamepadInput(true);
    nsCOMPtr<nsISupports> window = ToSupports(aWindow);
    nsRefPtr<Gamepad> gamepad = GetGamepad(aIndex);
    MOZ_ASSERT(gamepad);
    if (!gamepad) {
      return;
    }
    nsRefPtr<Gamepad> clonedGamepad = gamepad->Clone(window);
    aWindow->AddGamepad(aIndex, clonedGamepad);
  } else {
    aWindow->RemoveGamepad(aIndex);
  }
}

// static
void
GamepadService::TimeoutHandler(nsITimer* aTimer, void* aClosure)
{
  // the reason that we use self, instead of just using nsITimerCallback or nsIObserver
  // is so that subclasses are free to use timers without worry about the base classes's
  // usage.
  GamepadService* self = reinterpret_cast<GamepadService*>(aClosure);
  if (!self) {
    NS_ERROR("no self");
    return;
  }

  if (self->mShuttingDown) {
    return;
  }

  if (self->mListeners.Length() == 0) {
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      MaybeStopGamepadMonitoring();
    } else {
      ContentChild::GetSingleton()->SendGamepadListenerRemoved();
    }

    self->mStarted = false;
      self->mGamepads.Clear();
    }
  }

void
GamepadService::StartCleanupTimer()
{
  if (mTimer) {
    mTimer->Cancel();
  }

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mTimer) {
    mTimer->InitWithFuncCallback(TimeoutHandler,
                                 this,
                                 kCleanupDelayMS,
                                 nsITimer::TYPE_ONE_SHOT);
  }
}

void
GamepadService::Update(const GamepadChangeEvent& aEvent)
{
  if (aEvent.type() == GamepadChangeEvent::TGamepadAdded) {
    const GamepadAdded& a = aEvent.get_GamepadAdded();
    AddGamepad(a.index(), a.id(),
               static_cast<GamepadMappingType>(a.mapping()),
               a.num_buttons(), a.num_axes());
  } else if (aEvent.type() == GamepadChangeEvent::TGamepadRemoved) {
    const GamepadRemoved& a = aEvent.get_GamepadRemoved();
    RemoveGamepad(a.index());
  } else if (aEvent.type() == GamepadChangeEvent::TGamepadButtonInformation) {
    const GamepadButtonInformation& a = aEvent.get_GamepadButtonInformation();
    NewButtonEvent(a.index(), a.button(), a.pressed(), a.value());
  } else if (aEvent.type() == GamepadChangeEvent::TGamepadAxisInformation) {
    const GamepadAxisInformation& a = aEvent.get_GamepadAxisInformation();
    NewAxisMoveEvent(a.index(), a.axis(), a.value());
  } else {
    MOZ_CRASH("We shouldn't be here!");
  }
}

} // namespace dom
} // namespace mozilla
