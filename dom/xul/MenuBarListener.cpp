/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MenuBarListener.h"
#include "XULButtonElement.h"
#include "mozilla/Attributes.h"
#include "nsISound.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"

#include "nsContentUtils.h"
#include "nsPIWindowRoot.h"
#include "nsIFrame.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/XULButtonElement.h"
#include "mozilla/dom/XULMenuBarElement.h"
#include "mozilla/dom/XULMenuParentElement.h"
#include "nsXULPopupManager.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(MenuBarListener, nsIDOMEventListener)

MenuBarListener::MenuBarListener(XULMenuBarElement& aElement)
    : mMenuBar(&aElement),
      mEventTarget(aElement.GetComposedDoc()),
      mAccessKeyDown(false),
      mAccessKeyDownCanceled(false) {
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(mMenuBar);

  // Hook up the menubar as a key listener on the whole document.  This will
  // see every keypress that occurs, but after everyone else does.

  // Also hook up the listener to the window listening for focus events. This
  // is so we can keep proper state as the user alt-tabs through processes.

  mEventTarget->AddSystemEventListener(u"keypress"_ns, this, false);
  mEventTarget->AddSystemEventListener(u"keydown"_ns, this, false);
  mEventTarget->AddSystemEventListener(u"keyup"_ns, this, false);
  mEventTarget->AddSystemEventListener(u"mozaccesskeynotfound"_ns, this, false);
  // Need a capturing event listener if the user has blocked pages from
  // overriding system keys so that we can prevent menu accesskeys from being
  // cancelled.
  mEventTarget->AddEventListener(u"keydown"_ns, this, true);

  // mousedown event should be handled in all phase
  mEventTarget->AddEventListener(u"mousedown"_ns, this, true);
  mEventTarget->AddEventListener(u"mousedown"_ns, this, false);
  mEventTarget->AddEventListener(u"blur"_ns, this, true);

  mEventTarget->AddEventListener(u"MozDOMFullscreen:Entered"_ns, this, false);

  // Needs to listen to the deactivate event of the window.
  RefPtr<EventTarget> top = nsContentUtils::GetWindowRoot(mEventTarget);
  if (!NS_WARN_IF(!top)) {
    top->AddSystemEventListener(u"deactivate"_ns, this, true);
  }
}

////////////////////////////////////////////////////////////////////////
MenuBarListener::~MenuBarListener() {
  MOZ_ASSERT(!mEventTarget, "Should've detached always");
}

void MenuBarListener::Detach() {
  if (!mMenuBar) {
    MOZ_ASSERT(!mEventTarget);
    return;
  }
  mEventTarget->RemoveSystemEventListener(u"keypress"_ns, this, false);
  mEventTarget->RemoveSystemEventListener(u"keydown"_ns, this, false);
  mEventTarget->RemoveSystemEventListener(u"keyup"_ns, this, false);
  mEventTarget->RemoveSystemEventListener(u"mozaccesskeynotfound"_ns, this,
                                          false);
  mEventTarget->RemoveEventListener(u"keydown"_ns, this, true);

  mEventTarget->RemoveEventListener(u"mousedown"_ns, this, true);
  mEventTarget->RemoveEventListener(u"mousedown"_ns, this, false);
  mEventTarget->RemoveEventListener(u"blur"_ns, this, true);

  mEventTarget->RemoveEventListener(u"MozDOMFullscreen:Entered"_ns, this,
                                    false);
  RefPtr<EventTarget> top = nsContentUtils::GetWindowRoot(mEventTarget);
  if (!NS_WARN_IF(!top)) {
    top->RemoveSystemEventListener(u"deactivate"_ns, this, true);
  }
  mMenuBar = nullptr;
  mEventTarget = nullptr;
}

void MenuBarListener::ToggleMenuActiveState(ByKeyboard aByKeyboard) {
  RefPtr menuBar = mMenuBar;
  if (menuBar->IsActive()) {
    menuBar->SetActive(false);
  } else {
    if (aByKeyboard == ByKeyboard::Yes) {
      menuBar->SetActiveByKeyboard();
    }
    // This will activate the menubar if needed.
    menuBar->SelectFirstItem();
  }
}

////////////////////////////////////////////////////////////////////////
nsresult MenuBarListener::KeyUp(Event* aKeyEvent) {
  WidgetKeyboardEvent* nativeKeyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!nativeKeyEvent) {
    return NS_OK;
  }

  // handlers shouldn't be triggered by non-trusted events.
  if (!nativeKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  const auto accessKey = LookAndFeel::GetMenuAccessKey();
  if (!accessKey || !StaticPrefs::ui_key_menuAccessKeyFocuses()) {
    return NS_OK;
  }

  // On a press of the ALT key by itself, we toggle the menu's
  // active/inactive state.
  if (!nativeKeyEvent->DefaultPrevented() && mAccessKeyDown &&
      !mAccessKeyDownCanceled && nativeKeyEvent->mKeyCode == accessKey) {
    // The access key was down and is now up, and no other
    // keys were pressed in between.
    bool toggleMenuActiveState = true;
    if (!mMenuBar->IsActive()) {
      // If the focused content is in a remote process, we should allow the
      // focused web app to prevent to activate the menubar.
      if (nativeKeyEvent->WillBeSentToRemoteProcess()) {
        nativeKeyEvent->StopImmediatePropagation();
        nativeKeyEvent->MarkAsWaitingReplyFromRemoteProcess();
        return NS_OK;
      }
      // First, close all existing popups because other popups shouldn't
      // handle key events when menubar is active and IME should be
      // disabled.
      if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
        pm->Rollup({});
      }
      // If menubar active state is changed or the menubar is destroyed
      // during closing the popups, we should do nothing anymore.
      toggleMenuActiveState = !Destroyed() && !mMenuBar->IsActive();
    }
    if (toggleMenuActiveState) {
      ToggleMenuActiveState(ByKeyboard::Yes);
    }
  }

  mAccessKeyDown = false;
  mAccessKeyDownCanceled = false;

  if (!Destroyed() && mMenuBar->IsActive()) {
    nativeKeyEvent->StopPropagation();
    nativeKeyEvent->PreventDefault();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult MenuBarListener::KeyPress(Event* aKeyEvent) {
  // if event has already been handled, bail
  if (!aKeyEvent || aKeyEvent->DefaultPrevented()) {
    return NS_OK;  // don't consume event
  }

  // handlers shouldn't be triggered by non-trusted events.
  if (!aKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  const auto accessKey = LookAndFeel::GetMenuAccessKey();
  if (!accessKey) {
    return NS_OK;
  }
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
  if (keyCode != accessKey) {
    mAccessKeyDownCanceled = true;
  }

#ifndef XP_MACOSX
  // Need to handle F10 specially on Non-Mac platform.
  if (nativeKeyEvent->mMessage == eKeyPress && keyCode == NS_VK_F10) {
    if ((keyEvent->GetModifiersForMenuAccessKey() & ~MODIFIER_CONTROL) == 0) {
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
      ToggleMenuActiveState(ByKeyboard::Yes);

      if (mMenuBar && mMenuBar->IsActive()) {
#  ifdef MOZ_WIDGET_GTK
        if (RefPtr child = mMenuBar->GetActiveMenuChild()) {
          // In GTK, this also opens the first menu.
          child->OpenMenuPopup(false);
        }
#  endif
        aKeyEvent->StopPropagation();
        aKeyEvent->PreventDefault();
      }
    }

    return NS_OK;
  }
#endif  // !XP_MACOSX

  RefPtr menuForKey = GetMenuForKeyEvent(*keyEvent);
  if (!menuForKey) {
#ifdef XP_WIN
    // Behavior on Windows - this item is on the menu bar, beep and deactivate
    // the menu bar.
    // TODO(emilio): This is rather odd, and I cannot get the beep to work,
    // but this matches what old code was doing...
    if (mMenuBar && mMenuBar->IsActive() && mMenuBar->IsActiveByKeyboard()) {
      if (nsCOMPtr<nsISound> sound = do_GetService("@mozilla.org/sound;1")) {
        sound->Beep();
      }
      ToggleMenuActiveState(ByKeyboard::Yes);
    }
#endif
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

  RefPtr menuBar = mMenuBar;
  menuBar->SetActiveByKeyboard();
  // This will activate the menubar as needed.
  menuForKey->OpenMenuPopup(true);

  // The opened menu will listen next keyup event.
  // Therefore, we should clear the keydown flags here.
  mAccessKeyDown = mAccessKeyDownCanceled = false;

  aKeyEvent->StopPropagation();
  aKeyEvent->PreventDefault();
  return NS_OK;
}

dom::XULButtonElement* MenuBarListener::GetMenuForKeyEvent(
    KeyboardEvent& aKeyEvent) {
  if (!aKeyEvent.IsMenuAccessKeyPressed()) {
    return nullptr;
  }

  uint32_t charCode = aKeyEvent.CharCode();
  bool hasAccessKeyCandidates = charCode != 0;
  if (!hasAccessKeyCandidates) {
    WidgetKeyboardEvent* nativeKeyEvent =
        aKeyEvent.WidgetEventPtr()->AsKeyboardEvent();
    AutoTArray<uint32_t, 10> keys;
    nativeKeyEvent->GetAccessKeyCandidates(keys);
    hasAccessKeyCandidates = !keys.IsEmpty();
  }

  if (!hasAccessKeyCandidates) {
    return nullptr;
  }
  // Do shortcut navigation.
  // A letter was pressed. We want to see if a shortcut gets matched. If
  // so, we'll know the menu got activated.
  return mMenuBar->FindMenuWithShortcut(aKeyEvent);
}

void MenuBarListener::ReserveKeyIfNeeded(Event* aKeyEvent) {
  WidgetKeyboardEvent* nativeKeyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (nsContentUtils::ShouldBlockReservedKeys(nativeKeyEvent)) {
    nativeKeyEvent->MarkAsReservedByChrome();
  }
}

////////////////////////////////////////////////////////////////////////
nsresult MenuBarListener::KeyDown(Event* aKeyEvent) {
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
      (keyEvent->GetModifiersForMenuAccessKey() & ~MODIFIER_CONTROL) == 0) {
    ReserveKeyIfNeeded(aKeyEvent);
  }
#endif

  const auto accessKey = LookAndFeel::GetMenuAccessKey();
  if (accessKey && StaticPrefs::ui_key_menuAccessKeyFocuses()) {
    bool defaultPrevented = aKeyEvent->DefaultPrevented();

    // No other modifiers can be down.
    // Especially CTRL.  CTRL+ALT == AltGR, and we'll break on non-US
    // enhanced 102-key keyboards if we don't check this.
    bool isAccessKeyDownEvent =
        (theChar == accessKey &&
         (keyEvent->GetModifiersForMenuAccessKey() &
          ~LookAndFeel::GetMenuAccessKeyModifiers()) == 0);

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

  if (capturing && accessKey) {
    if (GetMenuForKeyEvent(*keyEvent)) {
      ReserveKeyIfNeeded(aKeyEvent);
    }
  }

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult MenuBarListener::Blur(Event* aEvent) {
  if (!IsMenuOpen() && mMenuBar->IsActive()) {
    ToggleMenuActiveState(ByKeyboard::No);
    mAccessKeyDown = false;
    mAccessKeyDownCanceled = false;
  }
  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult MenuBarListener::OnWindowDeactivated(Event* aEvent) {
  // Reset the accesskey state because we cannot receive the keyup event for
  // the pressing accesskey.
  mAccessKeyDown = false;
  mAccessKeyDownCanceled = false;
  return NS_OK;  // means I am NOT consuming event
}

bool MenuBarListener::IsMenuOpen() const {
  auto* activeChild = mMenuBar->GetActiveMenuChild();
  return activeChild && activeChild->IsMenuPopupOpen();
}

////////////////////////////////////////////////////////////////////////
nsresult MenuBarListener::MouseDown(Event* aMouseEvent) {
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

  if (!IsMenuOpen() && mMenuBar->IsActive()) {
    ToggleMenuActiveState(ByKeyboard::No);
  }

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult MenuBarListener::Fullscreen(Event* aEvent) {
  if (mMenuBar->IsActive()) {
    ToggleMenuActiveState(ByKeyboard::No);
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
MenuBarListener::HandleEvent(Event* aEvent) {
  // If the menu bar is collapsed, don't do anything.
  if (!mMenuBar || !mMenuBar->GetPrimaryFrame() ||
      !mMenuBar->GetPrimaryFrame()->StyleVisibility()->IsVisible()) {
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

}  // namespace mozilla::dom
