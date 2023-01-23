/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarListener.h"
#include "XULButtonElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/XULButtonElement.h"
#include "nsMenuBarFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsPIWindowRoot.h"
#include "nsISound.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

#include "nsContentUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/XULMenuParentElement.h"
#include "nsXULPopupManager.h"

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

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBarFrame,
                                     nsIContent* aMenuBarContent)
    : mMenuBarFrame(aMenuBarFrame),
      mContent(dom::XULMenuParentElement::FromNode(aMenuBarContent)),
      mEventTarget(aMenuBarContent->GetComposedDoc()),
      mTopWindowEventTarget(nullptr),
      mAccessKeyDown(false),
      mAccessKeyDownCanceled(false) {
  MOZ_ASSERT(mEventTarget);
  MOZ_ASSERT(mContent);

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
  RefPtr<dom::EventTarget> topWindowEventTarget =
      nsContentUtils::GetWindowRoot(aMenuBarContent->GetComposedDoc());
  mTopWindowEventTarget = topWindowEventTarget.get();

  mTopWindowEventTarget->AddSystemEventListener(u"deactivate"_ns, this, true);
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() {
  MOZ_ASSERT(!mEventTarget,
             "OnDestroyMenuBarFrame() should've alreay been called");
}

void nsMenuBarListener::OnDestroyMenuBarFrame() {
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

  mTopWindowEventTarget->RemoveSystemEventListener(u"deactivate"_ns, this,
                                                   true);

  mMenuBarFrame = nullptr;
  mEventTarget = nullptr;
  mTopWindowEventTarget = nullptr;
}

int32_t nsMenuBarListener::GetMenuAccessKey() {
  InitAccessKey();
  return mAccessKey;
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
  if (mMenuBarFrame->IsActive()) {
    mMenuBarFrame->SetActive(false);
  } else {
    RefPtr content = mContent;
    mMenuBarFrame->SetActive(true);
    content->SelectFirstItem();
  }
}

////////////////////////////////////////////////////////////////////////
nsresult nsMenuBarListener::KeyUp(Event* aKeyEvent) {
  WidgetKeyboardEvent* nativeKeyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (!nativeKeyEvent) {
    return NS_OK;
  }

  InitAccessKey();

  // handlers shouldn't be triggered by non-trusted events.
  if (!nativeKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  if (!mAccessKey || !StaticPrefs::ui_key_menuAccessKeyFocuses()) {
    return NS_OK;
  }

  // On a press of the ALT key by itself, we toggle the menu's
  // active/inactive state.
  if (!nativeKeyEvent->DefaultPrevented() && mAccessKeyDown &&
      !mAccessKeyDownCanceled &&
      static_cast<int32_t>(nativeKeyEvent->mKeyCode) == mAccessKey) {
    // The access key was down and is now up, and no other
    // keys were pressed in between.
    bool toggleMenuActiveState = true;
    if (!mMenuBarFrame->IsActive()) {
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

  if (!Destroyed() && mMenuBarFrame->IsActive()) {
    nativeKeyEvent->StopPropagation();
    nativeKeyEvent->PreventDefault();
  }

  return NS_OK;
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
      if ((GetModifiersForAccessKey(*keyEvent) & ~MODIFIER_CONTROL) == 0) {
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
          RefPtr child = mContent->GetActiveMenuChild();
          // In GTK, this also opens the first menu.
          child->OpenMenuPopup(false);
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
      if (mMenuBarFrame->IsActive()) {
        if (nsCOMPtr<nsISound> sound = do_GetService("@mozilla.org/sound;1")) {
          sound->Beep();
        }
        mMenuBarFrame->SetActive(false);
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

    mMenuBarFrame->SetActiveByKeyboard();
    mMenuBarFrame->SetActive(true);
    menuForKey->OpenMenuPopup(true);

    // The opened menu will listen next keyup event.
    // Therefore, we should clear the keydown flags here.
    mAccessKeyDown = mAccessKeyDownCanceled = false;

    aKeyEvent->StopPropagation();
    aKeyEvent->PreventDefault();
  }

  return NS_OK;
}

bool nsMenuBarListener::IsAccessKeyPressed(KeyboardEvent& aKeyEvent) {
  InitAccessKey();
  // No other modifiers are allowed to be down except for Shift.
  uint32_t modifiers = GetModifiersForAccessKey(aKeyEvent);

  return (mAccessKeyMask != MODIFIER_SHIFT && (modifiers & mAccessKeyMask) &&
          (modifiers & ~(mAccessKeyMask | MODIFIER_SHIFT)) == 0);
}

Modifiers nsMenuBarListener::GetModifiersForAccessKey(
    KeyboardEvent& aKeyEvent) {
  WidgetInputEvent* inputEvent = aKeyEvent.WidgetEventPtr()->AsInputEvent();
  MOZ_ASSERT(inputEvent);

  static const Modifiers kPossibleModifiersForAccessKey =
      (MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_META |
       MODIFIER_OS);
  return inputEvent->mModifiers & kPossibleModifiersForAccessKey;
}

dom::XULButtonElement* nsMenuBarListener::GetMenuForKeyEvent(
    KeyboardEvent& aKeyEvent) {
  if (!IsAccessKeyPressed(aKeyEvent)) {
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
  return mMenuBarFrame->MenubarElement().FindMenuWithShortcut(aKeyEvent);
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
      (GetModifiersForAccessKey(*keyEvent) & ~MODIFIER_CONTROL) == 0) {
    ReserveKeyIfNeeded(aKeyEvent);
  }
#endif

  if (mAccessKey && StaticPrefs::ui_key_menuAccessKeyFocuses()) {
    bool defaultPrevented = aKeyEvent->DefaultPrevented();

    // No other modifiers can be down.
    // Especially CTRL.  CTRL+ALT == AltGR, and we'll break on non-US
    // enhanced 102-key keyboards if we don't check this.
    bool isAccessKeyDownEvent =
        ((theChar == (uint32_t)mAccessKey) &&
         (GetModifiersForAccessKey(*keyEvent) & ~mAccessKeyMask) == 0);

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
    if (GetMenuForKeyEvent(*keyEvent)) {
      ReserveKeyIfNeeded(aKeyEvent);
    }
  }

  return NS_OK;  // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult nsMenuBarListener::Blur(Event* aEvent) {
  if (!IsMenuOpen() && mMenuBarFrame->IsActive()) {
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

bool nsMenuBarListener::IsMenuOpen() const {
  auto* activeChild = mContent->GetActiveMenuChild();
  return activeChild && activeChild->IsMenuPopupOpen();
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

  if (!IsMenuOpen() && mMenuBarFrame->IsActive()) {
    ToggleMenuActiveState();
  }

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
MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
nsMenuBarListener::HandleEvent(Event* aEvent) {
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
