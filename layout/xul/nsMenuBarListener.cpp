/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsPIWindowRoot.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

#include "nsContentUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"

using namespace mozilla;
using mozilla::dom::Event;
using mozilla::dom::KeyboardEvent;

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ISUPPORTS(nsMenuBarListener, nsIDOMEventListener)

////////////////////////////////////////////////////////////////////////

int32_t nsMenuBarListener::mAccessKey = -1;
Modifiers nsMenuBarListener::mAccessKeyMask = 0;
bool nsMenuBarListener::mAccessKeyFocuses = false;

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBarFrame,
                                     nsIContent* aMenuBarContent)
    : mMenuBarFrame(aMenuBarFrame),
      mEventTarget(aMenuBarContent ? aMenuBarContent->GetComposedDoc()
                                   : nullptr),
      mTopWindowEventTarget(nullptr),
      mAccessKeyDown(false),
      mAccessKeyDownCanceled(false) {
  MOZ_ASSERT(mEventTarget);

  // Hook up the menubar as a key listener on the whole document.  This will
  // see every keypress that occurs, but after everyone else does.

  // Also hook up the listener to the window listening for focus events. This
  // is so we can keep proper state as the user alt-tabs through processes.

  mEventTarget->AddSystemEventListener(NS_LITERAL_STRING("keypress"), this,
                                       false);
  mEventTarget->AddSystemEventListener(NS_LITERAL_STRING("keydown"), this,
                                       false);
  mEventTarget->AddSystemEventListener(NS_LITERAL_STRING("keyup"), this, false);
  mEventTarget->AddSystemEventListener(
      NS_LITERAL_STRING("mozaccesskeynotfound"), this, false);
  // Need a capturing event listener if the user has blocked pages from
  // overriding system keys so that we can prevent menu accesskeys from being
  // cancelled.
  mEventTarget->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);

  // mousedown event should be handled in all phase
  mEventTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), this, true);
  mEventTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), this, false);
  mEventTarget->AddEventListener(NS_LITERAL_STRING("blur"), this, true);

  mEventTarget->AddEventListener(NS_LITERAL_STRING("MozDOMFullscreen:Entered"),
                                 this, false);

  // Needs to listen to the deactivate event of the window.
  RefPtr<dom::EventTarget> topWindowEventTarget =
      nsContentUtils::GetWindowRoot(aMenuBarContent->GetComposedDoc());
  mTopWindowEventTarget = topWindowEventTarget.get();

  mTopWindowEventTarget->AddSystemEventListener(NS_LITERAL_STRING("deactivate"),
                                                this, true);
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() {
  MOZ_ASSERT(!mEventTarget,
             "OnDestroyMenuBarFrame() should've alreay been called");
}

void nsMenuBarListener::OnDestroyMenuBarFrame() {
  mEventTarget->RemoveSystemEventListener(NS_LITERAL_STRING("keypress"), this,
                                          false);
  mEventTarget->RemoveSystemEventListener(NS_LITERAL_STRING("keydown"), this,
                                          false);
  mEventTarget->RemoveSystemEventListener(NS_LITERAL_STRING("keyup"), this,
                                          false);
  mEventTarget->RemoveSystemEventListener(
      NS_LITERAL_STRING("mozaccesskeynotfound"), this, false);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);

  mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, true);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this,
                                    false);
  mEventTarget->RemoveEventListener(NS_LITERAL_STRING("blur"), this, true);

  mEventTarget->RemoveEventListener(
      NS_LITERAL_STRING("MozDOMFullscreen:Entered"), this, false);

  mTopWindowEventTarget->RemoveSystemEventListener(
      NS_LITERAL_STRING("deactivate"), this, true);

  mMenuBarFrame = nullptr;
  mEventTarget = nullptr;
  mTopWindowEventTarget = nullptr;
}

void nsMenuBarListener::InitializeStatics() {
  Preferences::AddBoolVarCache(&mAccessKeyFocuses,
                               "ui.key.menuAccessKeyFocuses");
}

nsresult nsMenuBarListener::GetMenuAccessKey(int32_t* aAccessKey) {
  if (!aAccessKey) return NS_ERROR_INVALID_POINTER;
  InitAccessKey();
  *aAccessKey = mAccessKey;
  return NS_OK;
}

void nsMenuBarListener::InitAccessKey() {
  if (mAccessKey >= 0) return;

    // Compiled-in defaults, in case we can't get LookAndFeel --
    // mac doesn't have menu shortcuts, other platforms use alt.
#ifdef XP_MACOSX
  mAccessKey = 0;
  mAccessKeyMask = 0;
#else
  mAccessKey = dom::KeyboardEvent_Binding::DOM_VK_ALT;
  mAccessKeyMask = MODIFIER_ALT;
#endif

  // Get the menu access key value from prefs, overriding the default:
  mAccessKey = Preferences::GetInt("ui.key.menuAccessKey", mAccessKey);
  switch (mAccessKey) {
    case dom::KeyboardEvent_Binding::DOM_VK_SHIFT:
      mAccessKeyMask = MODIFIER_SHIFT;
      break;
    case dom::KeyboardEvent_Binding::DOM_VK_CONTROL:
      mAccessKeyMask = MODIFIER_CONTROL;
      break;
    case dom::KeyboardEvent_Binding::DOM_VK_ALT:
      mAccessKeyMask = MODIFIER_ALT;
      break;
    case dom::KeyboardEvent_Binding::DOM_VK_META:
      mAccessKeyMask = MODIFIER_META;
      break;
    case dom::KeyboardEvent_Binding::DOM_VK_WIN:
      mAccessKeyMask = MODIFIER_OS;
      break;
    default:
      // Don't touch mAccessKeyMask.
      break;
  }
}

void nsMenuBarListener::ToggleMenuActiveState() {
  nsMenuFrame* closemenu = mMenuBarFrame->ToggleMenuActiveState();
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && closemenu) {
    nsMenuPopupFrame* popupFrame = closemenu->GetPopup();
    if (popupFrame)
      pm->HidePopup(popupFrame->GetContent(), false, false, true, false);
  }
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::KeyUp(Event* aKeyEvent) {
  RefPtr<KeyboardEvent> keyEvent = aKeyEvent->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_OK;
  }

  InitAccessKey();

  // handlers shouldn't be triggered by non-trusted events.
  if (!keyEvent->IsTrusted()) {
    return NS_OK;
  }

  if (mAccessKey && mAccessKeyFocuses) {
    bool defaultPrevented = keyEvent->DefaultPrevented();

    // On a press of the ALT key by itself, we toggle the menu's
    // active/inactive state.
    // Get the ascii key code.
    uint32_t theChar = keyEvent->KeyCode();

    if (!defaultPrevented && mAccessKeyDown && !mAccessKeyDownCanceled &&
        (int32_t)theChar == mAccessKey) {
      // The access key was down and is now up, and no other
      // keys were pressed in between.
      bool toggleMenuActiveState = true;
      if (!mMenuBarFrame->IsActive()) {
        // First, close all existing popups because other popups shouldn't
        // handle key events when menubar is active and IME should be
        // disabled.
        nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
        if (pm) {
          pm->Rollup(0, false, nullptr, nullptr);
        }
        // If menubar active state is changed or the menubar is destroyed
        // during closing the popups, we should do nothing anymore.
        toggleMenuActiveState = !Destroyed() && !mMenuBarFrame->IsActive();
      }
      if (toggleMenuActiveState) {
        if (!mMenuBarFrame->IsActive()) {
          mMenuBarFrame->SetActiveByKeyboard();
        }
        ToggleMenuActiveState();
      }
    }
    mAccessKeyDown = false;
    mAccessKeyDownCanceled = false;

    bool active = !Destroyed() && mMenuBarFrame->IsActive();
    if (active) {
      keyEvent->StopPropagation();
      keyEvent->PreventDefault();
      return NS_OK;  // I am consuming event
    }
  }

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::KeyPress(Event* aKeyEvent) {
  // if event has already been handled, bail
  if (!aKeyEvent || aKeyEvent->DefaultPrevented()) {
    return NS_OK;  // don't consume event
  }

  // handlers shouldn't be triggered by non-trusted events.
  if (!aKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  InitAccessKey();

  if (mAccessKey) {
    // If accesskey handling was forwarded to a child process, wait for
    // the mozaccesskeynotfound event before handling accesskeys.
    WidgetKeyboardEvent* nativeKeyEvent =
        aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
    if (!nativeKeyEvent) {
      return NS_OK;
    }

    RefPtr<KeyboardEvent> keyEvent = aKeyEvent->AsKeyboardEvent();
    uint32_t keyCode = keyEvent->KeyCode();

    // Cancel the access key flag unless we are pressing the access key.
    if (keyCode != (uint32_t)mAccessKey) {
      mAccessKeyDownCanceled = true;
    }

#ifndef XP_MACOSX
    // Need to handle F10 specially on Non-Mac platform.
    if (nativeKeyEvent->mMessage == eKeyPress && keyCode == NS_VK_F10) {
      if ((GetModifiersForAccessKey(keyEvent) & ~MODIFIER_CONTROL) == 0) {
        // If the keyboard event should activate the menubar and will be
        // sent to a remote process, it should be executed with reply
        // event from the focused remote process.  Note that if the menubar
        // is active, the event is already marked as "stop cross
        // process dispatching".  So, in that case, this won't wait
        // reply from the remote content.
        if (nativeKeyEvent->WillBeSentToRemoteProcess()) {
          nativeKeyEvent->StopImmediatePropagation();
          nativeKeyEvent->MarkAsWaitingReplyFromRemoteProcess();
          return NS_OK;
        }
        // The F10 key just went down by itself or with ctrl pressed.
        // In Windows, both of these activate the menu bar.
        mMenuBarFrame->SetActiveByKeyboard();
        ToggleMenuActiveState();

        if (mMenuBarFrame->IsActive()) {
#  ifdef MOZ_WIDGET_GTK
          // In GTK, this also opens the first menu.
          mMenuBarFrame->GetCurrentMenuItem()->OpenMenu(false);
#  endif
          aKeyEvent->StopPropagation();
          aKeyEvent->PreventDefault();
        }
      }

      return NS_OK;
    }
#endif  // !XP_MACOSX

    nsMenuFrame* menuFrameForKey = GetMenuForKeyEvent(keyEvent, false);
    if (!menuFrameForKey) {
      return NS_OK;
    }

    // If the keyboard event matches with a menu item's accesskey and
    // will be sent to a remote process, it should be executed with
    // reply event from the focused remote process.  Note that if the
    // menubar is active, the event is already marked as "stop cross
    // process dispatching".  So, in that case, this won't wait
    // reply from the remote content.
    if (nativeKeyEvent->WillBeSentToRemoteProcess()) {
      nativeKeyEvent->StopImmediatePropagation();
      nativeKeyEvent->MarkAsWaitingReplyFromRemoteProcess();
      return NS_OK;
    }

    mMenuBarFrame->SetActiveByKeyboard();
    mMenuBarFrame->SetActive(true);
    menuFrameForKey->OpenMenu(true);

    // The opened menu will listen next keyup event.
    // Therefore, we should clear the keydown flags here.
    mAccessKeyDown = mAccessKeyDownCanceled = false;

    aKeyEvent->StopPropagation();
    aKeyEvent->PreventDefault();
  }

  return NS_OK;
}

bool nsMenuBarListener::IsAccessKeyPressed(KeyboardEvent* aKeyEvent) {
  InitAccessKey();
  // No other modifiers are allowed to be down except for Shift.
  uint32_t modifiers = GetModifiersForAccessKey(aKeyEvent);

  return (mAccessKeyMask != MODIFIER_SHIFT && (modifiers & mAccessKeyMask) &&
          (modifiers & ~(mAccessKeyMask | MODIFIER_SHIFT)) == 0);
}

Modifiers nsMenuBarListener::GetModifiersForAccessKey(
    KeyboardEvent* aKeyEvent) {
  WidgetInputEvent* inputEvent = aKeyEvent->WidgetEventPtr()->AsInputEvent();
  MOZ_ASSERT(inputEvent);

  static const Modifiers kPossibleModifiersForAccessKey =
      (MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_META |
       MODIFIER_OS);
  return (inputEvent->mModifiers & kPossibleModifiersForAccessKey);
}

nsMenuFrame* nsMenuBarListener::GetMenuForKeyEvent(KeyboardEvent* aKeyEvent,
                                                   bool aPeek) {
  if (!IsAccessKeyPressed(aKeyEvent)) {
    return nullptr;
  }

  uint32_t charCode = aKeyEvent->CharCode();
  bool hasAccessKeyCandidates = charCode != 0;
  if (!hasAccessKeyCandidates) {
    WidgetKeyboardEvent* nativeKeyEvent =
        aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();

    AutoTArray<uint32_t, 10> keys;
    nativeKeyEvent->GetAccessKeyCandidates(keys);
    hasAccessKeyCandidates = !keys.IsEmpty();
  }

  if (hasAccessKeyCandidates) {
    // Do shortcut navigation.
    // A letter was pressed. We want to see if a shortcut gets matched. If
    // so, we'll know the menu got activated.
    return mMenuBarFrame->FindMenuWithShortcut(aKeyEvent, aPeek);
  }

  return nullptr;
}

void nsMenuBarListener::ReserveKeyIfNeeded(Event* aKeyEvent) {
  WidgetKeyboardEvent* nativeKeyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (nsContentUtils::ShouldBlockReservedKeys(nativeKeyEvent)) {
    nativeKeyEvent->MarkAsReservedByChrome();
  }
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::KeyDown(Event* aKeyEvent) {
  InitAccessKey();

  // handlers shouldn't be triggered by non-trusted events.
  if (!aKeyEvent || !aKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  RefPtr<KeyboardEvent> keyEvent = aKeyEvent->AsKeyboardEvent();
  if (!keyEvent) {
    return NS_OK;
  }

  uint32_t theChar = keyEvent->KeyCode();

  uint16_t eventPhase = keyEvent->EventPhase();
  bool capturing = (eventPhase == dom::Event_Binding::CAPTURING_PHASE);

#ifndef XP_MACOSX
  if (capturing && !mAccessKeyDown && theChar == NS_VK_F10 &&
      (GetModifiersForAccessKey(keyEvent) & ~MODIFIER_CONTROL) == 0) {
    ReserveKeyIfNeeded(aKeyEvent);
  }
#endif

  if (mAccessKey && mAccessKeyFocuses) {
    bool defaultPrevented = aKeyEvent->DefaultPrevented();

    // No other modifiers can be down.
    // Especially CTRL.  CTRL+ALT == AltGR, and we'll break on non-US
    // enhanced 102-key keyboards if we don't check this.
    bool isAccessKeyDownEvent =
        ((theChar == (uint32_t)mAccessKey) &&
         (GetModifiersForAccessKey(keyEvent) & ~mAccessKeyMask) == 0);

    if (!capturing && !mAccessKeyDown) {
      // If accesskey isn't being pressed and the key isn't the accesskey,
      // ignore the event.
      if (!isAccessKeyDownEvent) {
        return NS_OK;
      }

      // Otherwise, accept the accesskey state.
      mAccessKeyDown = true;
      // If default is prevented already, cancel the access key down.
      mAccessKeyDownCanceled = defaultPrevented;
      return NS_OK;
    }

    // If the pressed accesskey was canceled already or the event was
    // consumed already, ignore the event.
    if (mAccessKeyDownCanceled || defaultPrevented) {
      return NS_OK;
    }

    // Some key other than the access key just went down,
    // so we won't activate the menu bar when the access key is released.
    mAccessKeyDownCanceled = !isAccessKeyDownEvent;
  }

  if (capturing && mAccessKey) {
    nsMenuFrame* menuFrameForKey = GetMenuForKeyEvent(keyEvent, true);
    if (menuFrameForKey) {
      ReserveKeyIfNeeded(aKeyEvent);
    }
  }

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult nsMenuBarListener::Blur(Event* aEvent) {
  if (!mMenuBarFrame->IsMenuOpen() && mMenuBarFrame->IsActive()) {
    ToggleMenuActiveState();
    mAccessKeyDown = false;
    mAccessKeyDownCanceled = false;
  }
  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult nsMenuBarListener::OnWindowDeactivated(Event* aEvent) {
  // Reset the accesskey state because we cannot receive the keyup event for
  // the pressing accesskey.
  mAccessKeyDown = false;
  mAccessKeyDownCanceled = false;
  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::MouseDown(Event* aMouseEvent) {
  // NOTE: MouseDown method listens all phases

  // Even if the mousedown event is canceled, it means the user don't want
  // to activate the menu.  Therefore, we need to record it at capturing (or
  // target) phase.
  if (mAccessKeyDown) {
    mAccessKeyDownCanceled = true;
  }

  // Don't do anything at capturing phase, any behavior should be cancelable.
  if (aMouseEvent->EventPhase() == dom::Event_Binding::CAPTURING_PHASE) {
    return NS_OK;
  }

  if (!mMenuBarFrame->IsMenuOpen() && mMenuBarFrame->IsActive())
    ToggleMenuActiveState();

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult nsMenuBarListener::Fullscreen(Event* aEvent) {
  if (mMenuBarFrame->IsActive()) {
    ToggleMenuActiveState();
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::HandleEvent(Event* aEvent) {
  // If the menu bar is collapsed, don't do anything.
  if (!mMenuBarFrame->StyleVisibility()->IsVisible()) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("keyup")) {
    return KeyUp(aEvent);
  }
  if (eventType.EqualsLiteral("keydown")) {
    return KeyDown(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return KeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("mozaccesskeynotfound")) {
    return KeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("blur")) {
    return Blur(aEvent);
  }
  if (eventType.EqualsLiteral("deactivate")) {
    return OnWindowDeactivated(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    return MouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("MozDOMFullscreen:Entered")) {
    return Fullscreen(aEvent);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}
