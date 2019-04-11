/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadManager.h"

#include "mozilla/dom/Gamepad.h"
#include "mozilla/dom/GamepadAxisMoveEvent.h"
#include "mozilla/dom/GamepadButtonEvent.h"
#include "mozilla/dom/GamepadEvent.h"
#include "mozilla/dom/GamepadEventChannelChild.h"
#include "mozilla/dom/GamepadMonitoring.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"

#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDOMWindow.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsThreadUtils.h"
#include "VRManagerChild.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"

#include <cstddef>

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

namespace {

const char* kGamepadEnabledPref = "dom.gamepad.enabled";
const char* kGamepadEventsEnabledPref =
    "dom.gamepad.non_standard_events.enabled";

const nsTArray<RefPtr<nsGlobalWindowInner>>::index_type NoIndex =
    nsTArray<RefPtr<nsGlobalWindowInner>>::NoIndex;

bool sShutdown = false;

StaticRefPtr<GamepadManager> gGamepadManagerSingleton;
const uint32_t VR_GAMEPAD_IDX_OFFSET = 0x01 << 16;

}  // namespace

NS_IMPL_ISUPPORTS(GamepadManager, nsIObserver)

GamepadManager::GamepadManager()
    : mEnabled(false),
      mNonstandardEventsEnabled(false),
      mShuttingDown(false),
      mPromiseID(0) {}

nsresult GamepadManager::Init() {
  mEnabled = IsAPIEnabled();
  mNonstandardEventsEnabled =
      Preferences::GetBool(kGamepadEventsEnabledPref, false);
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (NS_WARN_IF(!observerService)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  rv = observerService->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                    false);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
GamepadManager::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
  }
  BeginShutdown();
  return NS_OK;
}

void GamepadManager::StopMonitoring() {
  for (uint32_t i = 0; i < mChannelChildren.Length(); ++i) {
    mChannelChildren[i]->SendGamepadListenerRemoved();
  }
  if (gfx::VRManagerChild::IsCreated()) {
    gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
    vm->SendControllerListenerRemoved();
  }
  mChannelChildren.Clear();
  mGamepads.Clear();
}

void GamepadManager::BeginShutdown() {
  mShuttingDown = true;
  StopMonitoring();
  // Don't let windows call back to unregister during shutdown
  for (uint32_t i = 0; i < mListeners.Length(); i++) {
    mListeners[i]->SetHasGamepadEventListener(false);
  }
  mListeners.Clear();
  sShutdown = true;
}

void GamepadManager::AddListener(nsGlobalWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(NS_IsMainThread());

  // IPDL child has not been created
  if (mChannelChildren.IsEmpty()) {
    PBackgroundChild* actor = BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!actor)) {
      // We are probably shutting down.
      return;
    }

    GamepadEventChannelChild* child = new GamepadEventChannelChild();
    PGamepadEventChannelChild* initedChild =
        actor->SendPGamepadEventChannelConstructor(child);
    if (NS_WARN_IF(!initedChild)) {
      // We are probably shutting down.
      return;
    }

    MOZ_ASSERT(initedChild == child);
    child->SendGamepadListenerAdded();
    mChannelChildren.AppendElement(child);

    if (gfx::VRManagerChild::IsCreated()) {
      // Construct VRManagerChannel and ask adding the connected
      // VR controllers to GamepadManager
      gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
      vm->SendControllerListenerAdded();
    }
  }

  if (!mEnabled || mShuttingDown ||
      nsContentUtils::ShouldResistFingerprinting(aWindow->GetExtantDoc())) {
    return;
  }

  if (mListeners.IndexOf(aWindow) != NoIndex) {
    return;  // already exists
  }

  mListeners.AppendElement(aWindow);
}

void GamepadManager::RemoveListener(nsGlobalWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);

  if (mShuttingDown) {
    // Doesn't matter at this point. It's possible we're being called
    // as a result of our own destructor here, so just bail out.
    return;
  }

  if (mListeners.IndexOf(aWindow) == NoIndex) {
    return;  // doesn't exist
  }

  for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
    aWindow->RemoveGamepad(iter.Key());
  }

  mListeners.RemoveElement(aWindow);

  if (mListeners.IsEmpty()) {
    StopMonitoring();
  }
}

already_AddRefed<Gamepad> GamepadManager::GetGamepad(uint32_t aIndex) const {
  RefPtr<Gamepad> gamepad;
  if (mGamepads.Get(aIndex, getter_AddRefs(gamepad))) {
    return gamepad.forget();
  }

  return nullptr;
}

already_AddRefed<Gamepad> GamepadManager::GetGamepad(
    uint32_t aGamepadId, GamepadServiceType aServiceType) const {
  return GetGamepad(GetGamepadIndexWithServiceType(aGamepadId, aServiceType));
}

uint32_t GamepadManager::GetGamepadIndexWithServiceType(
    uint32_t aIndex, GamepadServiceType aServiceType) const {
  uint32_t newIndex = 0;

  switch (aServiceType) {
    case GamepadServiceType::Standard:
      MOZ_ASSERT(aIndex <= VR_GAMEPAD_IDX_OFFSET);
      newIndex = aIndex;
      break;
    case GamepadServiceType::VR:
      newIndex = aIndex + VR_GAMEPAD_IDX_OFFSET;
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }

  return newIndex;
}

void GamepadManager::AddGamepad(uint32_t aIndex, const nsAString& aId,
                                GamepadMappingType aMapping, GamepadHand aHand,
                                GamepadServiceType aServiceType,
                                uint32_t aDisplayID, uint32_t aNumButtons,
                                uint32_t aNumAxes, uint32_t aNumHaptics) {
  uint32_t newIndex = GetGamepadIndexWithServiceType(aIndex, aServiceType);

  // TODO: bug 852258: get initial button/axis state
  RefPtr<Gamepad> gamepad = new Gamepad(nullptr, aId,
                                        0,  // index is set by global window
                                        newIndex, aMapping, aHand, aDisplayID,
                                        aNumButtons, aNumAxes, aNumHaptics);

  // We store the gamepad related to its index given by the parent process,
  // and no duplicate index is allowed.
  MOZ_ASSERT(!mGamepads.Get(newIndex, nullptr));
  mGamepads.Put(newIndex, gamepad);
  NewConnectionEvent(newIndex, true);
}

void GamepadManager::RemoveGamepad(uint32_t aIndex,
                                   GamepadServiceType aServiceType) {
  uint32_t newIndex = GetGamepadIndexWithServiceType(aIndex, aServiceType);

  RefPtr<Gamepad> gamepad = GetGamepad(newIndex);
  if (!gamepad) {
    NS_WARNING("Trying to delete gamepad with invalid index");
    return;
  }
  gamepad->SetConnected(false);
  NewConnectionEvent(newIndex, false);
  mGamepads.Remove(newIndex);
}

void GamepadManager::FireButtonEvent(EventTarget* aTarget, Gamepad* aGamepad,
                                     uint32_t aButton, double aValue) {
  nsString name = aValue == 1.0L ? NS_LITERAL_STRING("gamepadbuttondown")
                                 : NS_LITERAL_STRING("gamepadbuttonup");
  GamepadButtonEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  init.mButton = aButton;
  RefPtr<GamepadButtonEvent> event =
      GamepadButtonEvent::Constructor(aTarget, name, init);

  event->SetTrusted(true);

  aTarget->DispatchEvent(*event);
}

void GamepadManager::FireAxisMoveEvent(EventTarget* aTarget, Gamepad* aGamepad,
                                       uint32_t aAxis, double aValue) {
  GamepadAxisMoveEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  init.mAxis = aAxis;
  init.mValue = aValue;
  RefPtr<GamepadAxisMoveEvent> event = GamepadAxisMoveEvent::Constructor(
      aTarget, NS_LITERAL_STRING("gamepadaxismove"), init);

  event->SetTrusted(true);

  aTarget->DispatchEvent(*event);
}

void GamepadManager::NewConnectionEvent(uint32_t aIndex, bool aConnected) {
  if (mShuttingDown) {
    return;
  }

  RefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (!gamepad) {
    return;
  }

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<RefPtr<nsGlobalWindowInner>> listeners(mListeners);

  if (aConnected) {
    for (uint32_t i = 0; i < listeners.Length(); i++) {
      // Do not fire gamepadconnected and gamepaddisconnected events when
      // privacy.resistFingerprinting is true.
      if (nsContentUtils::ShouldResistFingerprinting(
              listeners[i]->GetExtantDoc())) {
        continue;
      }

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

      RefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
      if (listenerGamepad) {
        // Fire event
        FireConnectionEvent(listeners[i], listenerGamepad, aConnected);
      }
    }
  } else {
    // For disconnection events, fire one at every window that has received
    // data from this gamepad.
    for (uint32_t i = 0; i < listeners.Length(); i++) {
      // Even background windows get these events, so we don't have to
      // deal with the hassle of syncing the state of removed gamepads.

      // Do not fire gamepadconnected and gamepaddisconnected events when
      // privacy.resistFingerprinting is true.
      if (nsContentUtils::ShouldResistFingerprinting(
              listeners[i]->GetExtantDoc())) {
        continue;
      }

      if (WindowHasSeenGamepad(listeners[i], aIndex)) {
        RefPtr<Gamepad> listenerGamepad = listeners[i]->GetGamepad(aIndex);
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

void GamepadManager::FireConnectionEvent(EventTarget* aTarget,
                                         Gamepad* aGamepad, bool aConnected) {
  nsString name = aConnected ? NS_LITERAL_STRING("gamepadconnected")
                             : NS_LITERAL_STRING("gamepaddisconnected");
  GamepadEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mGamepad = aGamepad;
  RefPtr<GamepadEvent> event = GamepadEvent::Constructor(aTarget, name, init);

  event->SetTrusted(true);

  aTarget->DispatchEvent(*event);
}

void GamepadManager::SyncGamepadState(uint32_t aIndex,
                                      nsGlobalWindowInner* aWindow,
                                      Gamepad* aGamepad) {
  if (mShuttingDown || !mEnabled ||
      nsContentUtils::ShouldResistFingerprinting(aWindow->GetExtantDoc())) {
    return;
  }

  RefPtr<Gamepad> gamepad = GetGamepad(aIndex);
  if (!gamepad) {
    return;
  }

  aGamepad->SyncState(gamepad);
}

// static
bool GamepadManager::IsServiceRunning() { return !!gGamepadManagerSingleton; }

// static
already_AddRefed<GamepadManager> GamepadManager::GetService() {
  if (sShutdown) {
    return nullptr;
  }

  if (!gGamepadManagerSingleton) {
    RefPtr<GamepadManager> manager = new GamepadManager();
    nsresult rv = manager->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    gGamepadManagerSingleton = manager;
    ClearOnShutdown(&gGamepadManagerSingleton);
  }

  RefPtr<GamepadManager> service(gGamepadManagerSingleton);
  return service.forget();
}

// static
bool GamepadManager::IsAPIEnabled() {
  return Preferences::GetBool(kGamepadEnabledPref, false);
}

bool GamepadManager::MaybeWindowHasSeenGamepad(nsGlobalWindowInner* aWindow,
                                               uint32_t aIndex) {
  if (!WindowHasSeenGamepad(aWindow, aIndex)) {
    // This window hasn't seen this gamepad before, so
    // send a connection event first.
    SetWindowHasSeenGamepad(aWindow, aIndex);
    return true;
  }
  return false;
}

bool GamepadManager::WindowHasSeenGamepad(nsGlobalWindowInner* aWindow,
                                          uint32_t aIndex) const {
  RefPtr<Gamepad> gamepad = aWindow->GetGamepad(aIndex);
  return gamepad != nullptr;
}

void GamepadManager::SetWindowHasSeenGamepad(nsGlobalWindowInner* aWindow,
                                             uint32_t aIndex, bool aHasSeen) {
  MOZ_ASSERT(aWindow);

  if (mListeners.IndexOf(aWindow) == NoIndex) {
    // This window isn't even listening for gamepad events.
    return;
  }

  if (aHasSeen) {
    aWindow->SetHasSeenGamepadInput(true);
    nsCOMPtr<nsISupports> window = ToSupports(aWindow);
    RefPtr<Gamepad> gamepad = GetGamepad(aIndex);
    if (!gamepad) {
      return;
    }
    RefPtr<Gamepad> clonedGamepad = gamepad->Clone(window);
    aWindow->AddGamepad(aIndex, clonedGamepad);
  } else {
    aWindow->RemoveGamepad(aIndex);
  }
}

void GamepadManager::Update(const GamepadChangeEvent& aEvent) {
  if (!mEnabled || mShuttingDown ||
      nsContentUtils::ShouldResistFingerprinting()) {
    return;
  }

  const uint32_t index = aEvent.index();
  GamepadServiceType serviceType = aEvent.service_type();
  GamepadChangeEventBody body = aEvent.body();

  if (body.type() == GamepadChangeEventBody::TGamepadAdded) {
    const GamepadAdded& a = body.get_GamepadAdded();
    AddGamepad(index, a.id(), static_cast<GamepadMappingType>(a.mapping()),
               static_cast<GamepadHand>(a.hand()), serviceType, a.display_id(),
               a.num_buttons(), a.num_axes(), a.num_haptics());
    return;
  }
  if (body.type() == GamepadChangeEventBody::TGamepadRemoved) {
    RemoveGamepad(index, serviceType);
    return;
  }

  if (!SetGamepadByEvent(aEvent)) {
    return;
  }

  // Hold on to listeners in a separate array because firing events
  // can mutate the mListeners array.
  nsTArray<RefPtr<nsGlobalWindowInner>> listeners(mListeners);

  for (uint32_t i = 0; i < listeners.Length(); i++) {
    // Only send events to non-background windows
    if (!listeners[i]->IsCurrentInnerWindow() ||
        listeners[i]->GetOuterWindow()->IsBackground()) {
      continue;
    }

    SetGamepadByEvent(aEvent, listeners[i]);
    MaybeConvertToNonstandardGamepadEvent(aEvent, listeners[i]);
  }
}

void GamepadManager::MaybeConvertToNonstandardGamepadEvent(
    const GamepadChangeEvent& aEvent, nsGlobalWindowInner* aWindow) {
  MOZ_ASSERT(aWindow);

  if (!mNonstandardEventsEnabled) {
    return;
  }

  const uint32_t index =
      GetGamepadIndexWithServiceType(aEvent.index(), aEvent.service_type());
  RefPtr<Gamepad> gamepad = aWindow->GetGamepad(index);
  const GamepadChangeEventBody& body = aEvent.body();

  if (gamepad) {
    switch (body.type()) {
      case GamepadChangeEventBody::TGamepadButtonInformation: {
        const GamepadButtonInformation& a = body.get_GamepadButtonInformation();
        FireButtonEvent(aWindow, gamepad, a.button(), a.value());
        break;
      }
      case GamepadChangeEventBody::TGamepadAxisInformation: {
        const GamepadAxisInformation& a = body.get_GamepadAxisInformation();
        FireAxisMoveEvent(aWindow, gamepad, a.axis(), a.value());
        break;
      }
      default:
        break;
    }
  }
}

bool GamepadManager::SetGamepadByEvent(const GamepadChangeEvent& aEvent,
                                       nsGlobalWindowInner* aWindow) {
  bool ret = false;
  bool firstTime = false;
  const uint32_t index =
      GetGamepadIndexWithServiceType(aEvent.index(), aEvent.service_type());
  if (aWindow) {
    firstTime = MaybeWindowHasSeenGamepad(aWindow, index);
  }

  RefPtr<Gamepad> gamepad =
      aWindow ? aWindow->GetGamepad(index) : GetGamepad(index);
  const GamepadChangeEventBody& body = aEvent.body();

  if (gamepad) {
    switch (body.type()) {
      case GamepadChangeEventBody::TGamepadButtonInformation: {
        const GamepadButtonInformation& a = body.get_GamepadButtonInformation();
        gamepad->SetButton(a.button(), a.pressed(), a.touched(), a.value());
        break;
      }
      case GamepadChangeEventBody::TGamepadAxisInformation: {
        const GamepadAxisInformation& a = body.get_GamepadAxisInformation();
        gamepad->SetAxis(a.axis(), a.value());
        break;
      }
      case GamepadChangeEventBody::TGamepadPoseInformation: {
        const GamepadPoseInformation& a = body.get_GamepadPoseInformation();
        gamepad->SetPose(a.pose_state());
        break;
      }
      case GamepadChangeEventBody::TGamepadHandInformation: {
        const GamepadHandInformation& a = body.get_GamepadHandInformation();
        gamepad->SetHand(a.hand());
        break;
      }
      default:
        MOZ_ASSERT(false);
        break;
    }
    ret = true;
  }

  if (aWindow && firstTime) {
    FireConnectionEvent(aWindow, gamepad, true);
  }

  return ret;
}

already_AddRefed<Promise> GamepadManager::VibrateHaptic(
    uint32_t aControllerIdx, uint32_t aHapticIndex, double aIntensity,
    double aDuration, nsIGlobalObject* aGlobal, ErrorResult& aRv) {
  const char* kGamepadHapticEnabledPref = "dom.gamepad.haptic_feedback.enabled";
  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  if (Preferences::GetBool(kGamepadHapticEnabledPref)) {
    if (aControllerIdx >= VR_GAMEPAD_IDX_OFFSET) {
      if (gfx::VRManagerChild::IsCreated()) {
        const uint32_t index = aControllerIdx - VR_GAMEPAD_IDX_OFFSET;
        gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
        vm->AddPromise(mPromiseID, promise);
        vm->SendVibrateHaptic(index, aHapticIndex, aIntensity, aDuration,
                              mPromiseID);
      }
    } else {
      for (const auto& channelChild : mChannelChildren) {
        channelChild->AddPromise(mPromiseID, promise);
        channelChild->SendVibrateHaptic(aControllerIdx, aHapticIndex,
                                        aIntensity, aDuration, mPromiseID);
      }
    }
  }

  ++mPromiseID;
  return promise.forget();
}

void GamepadManager::StopHaptics() {
  const char* kGamepadHapticEnabledPref = "dom.gamepad.haptic_feedback.enabled";
  if (!Preferences::GetBool(kGamepadHapticEnabledPref)) {
    return;
  }

  for (auto iter = mGamepads.Iter(); !iter.Done(); iter.Next()) {
    const uint32_t gamepadIndex = iter.UserData()->HashKey();
    if (gamepadIndex >= VR_GAMEPAD_IDX_OFFSET) {
      if (gfx::VRManagerChild::IsCreated()) {
        const uint32_t index = gamepadIndex - VR_GAMEPAD_IDX_OFFSET;
        gfx::VRManagerChild* vm = gfx::VRManagerChild::Get();
        vm->SendStopVibrateHaptic(index);
      }
    } else {
      for (auto& channelChild : mChannelChildren) {
        channelChild->SendStopVibrateHaptic(gamepadIndex);
      }
    }
  }
}

}  // namespace dom
}  // namespace mozilla
