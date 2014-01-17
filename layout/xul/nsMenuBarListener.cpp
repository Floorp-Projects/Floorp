/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsIDOMEvent.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDOMKeyEvent.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ISUPPORTS1(nsMenuBarListener, nsIDOMEventListener)

#define MODIFIER_SHIFT    1
#define MODIFIER_CONTROL  2
#define MODIFIER_ALT      4
#define MODIFIER_META     8
#define MODIFIER_OS       16

////////////////////////////////////////////////////////////////////////

int32_t nsMenuBarListener::mAccessKey = -1;
uint32_t nsMenuBarListener::mAccessKeyMask = 0;
bool nsMenuBarListener::mAccessKeyFocuses = false;

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
  :mAccessKeyDown(false), mAccessKeyDownCanceled(false)
{
  mMenuBarFrame = aMenuBar;
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() 
{
}

nsresult
nsMenuBarListener::GetMenuAccessKey(int32_t* aAccessKey)
{
  if (!aAccessKey)
    return NS_ERROR_INVALID_POINTER;
  InitAccessKey();
  *aAccessKey = mAccessKey;
  return NS_OK;
}

void nsMenuBarListener::InitAccessKey()
{
  if (mAccessKey >= 0)
    return;

  // Compiled-in defaults, in case we can't get LookAndFeel --
  // mac doesn't have menu shortcuts, other platforms use alt.
#ifdef XP_MACOSX
  mAccessKey = 0;
  mAccessKeyMask = 0;
#else
  mAccessKey = nsIDOMKeyEvent::DOM_VK_ALT;
  mAccessKeyMask = MODIFIER_ALT;
#endif

  // Get the menu access key value from prefs, overriding the default:
  mAccessKey = Preferences::GetInt("ui.key.menuAccessKey", mAccessKey);
  if (mAccessKey == nsIDOMKeyEvent::DOM_VK_SHIFT)
    mAccessKeyMask = MODIFIER_SHIFT;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_CONTROL)
    mAccessKeyMask = MODIFIER_CONTROL;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_ALT)
    mAccessKeyMask = MODIFIER_ALT;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_META)
    mAccessKeyMask = MODIFIER_META;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_WIN)
    mAccessKeyMask = MODIFIER_OS;

  mAccessKeyFocuses = Preferences::GetBool("ui.key.menuAccessKeyFocuses");
}

void
nsMenuBarListener::ToggleMenuActiveState()
{
  nsMenuFrame* closemenu = mMenuBarFrame->ToggleMenuActiveState();
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && closemenu) {
    nsMenuPopupFrame* popupFrame = closemenu->GetPopup();
    if (popupFrame)
      pm->HidePopup(popupFrame->GetContent(), false, false, true);
  }
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  if (!keyEvent) {
    return NS_OK;
  }

  InitAccessKey();

  //handlers shouldn't be triggered by non-trusted events.
  bool trustedEvent = false;
  aKeyEvent->GetIsTrusted(&trustedEvent);

  if (!trustedEvent) {
    return NS_OK;
  }

  if (mAccessKey && mAccessKeyFocuses)
  {
    bool defaultPrevented = false;
    aKeyEvent->GetDefaultPrevented(&defaultPrevented);

    // On a press of the ALT key by itself, we toggle the menu's 
    // active/inactive state.
    // Get the ascii key code.
    uint32_t theChar;
    keyEvent->GetKeyCode(&theChar);

    if (!defaultPrevented && mAccessKeyDown && !mAccessKeyDownCanceled &&
        (int32_t)theChar == mAccessKey)
    {
      // The access key was down and is now up, and no other
      // keys were pressed in between.
      if (!mMenuBarFrame->IsActive()) {
        mMenuBarFrame->SetActiveByKeyboard();
      }
      ToggleMenuActiveState();
    }
    mAccessKeyDown = false;
    mAccessKeyDownCanceled = false;

    bool active = mMenuBarFrame->IsActive();
    if (active) {
      aKeyEvent->StopPropagation();
      aKeyEvent->PreventDefault();
      return NS_OK; // I am consuming event
    }
  }
  
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  // if event has already been handled, bail
  if (aKeyEvent) {
    bool eventHandled = false;
    aKeyEvent->GetDefaultPrevented(&eventHandled);
    if (eventHandled) {
      return NS_OK;       // don't consume event
    }
  }

  //handlers shouldn't be triggered by non-trusted events.
  bool trustedEvent = false;
  if (aKeyEvent) {
    aKeyEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  nsresult retVal = NS_OK;  // default is to not consume event
  
  InitAccessKey();

  if (mAccessKey)
  {
    bool preventDefault;
    aKeyEvent->GetDefaultPrevented(&preventDefault);
    if (!preventDefault) {
      nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
      uint32_t keyCode, charCode;
      keyEvent->GetKeyCode(&keyCode);
      keyEvent->GetCharCode(&charCode);

      bool hasAccessKeyCandidates = charCode != 0;
      if (!hasAccessKeyCandidates) {
        WidgetKeyboardEvent* nativeKeyEvent =
          aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
        if (nativeKeyEvent) {
          nsAutoTArray<uint32_t, 10> keys;
          nsContentUtils::GetAccessKeyCandidates(nativeKeyEvent, keys);
          hasAccessKeyCandidates = !keys.IsEmpty();
        }
      }

      // Cancel the access key flag unless we are pressing the access key.
      if (keyCode != (uint32_t)mAccessKey) {
        mAccessKeyDownCanceled = true;
      }

      if (IsAccessKeyPressed(keyEvent) && hasAccessKeyCandidates) {
        // Do shortcut navigation.
        // A letter was pressed. We want to see if a shortcut gets matched. If
        // so, we'll know the menu got activated.
        nsMenuFrame* result = mMenuBarFrame->FindMenuWithShortcut(keyEvent);
        if (result) {
          mMenuBarFrame->SetActiveByKeyboard();
          mMenuBarFrame->SetActive(true);
          result->OpenMenu(true);

          // The opened menu will listen next keyup event.
          // Therefore, we should clear the keydown flags here.
          mAccessKeyDown = mAccessKeyDownCanceled = false;

          aKeyEvent->StopPropagation();
          aKeyEvent->PreventDefault();
          retVal = NS_OK;       // I am consuming event
        }
      }    
#ifndef XP_MACOSX
      // Also need to handle F10 specially on Non-Mac platform.
      else if (keyCode == NS_VK_F10) {
        if ((GetModifiers(keyEvent) & ~MODIFIER_CONTROL) == 0) {
          // The F10 key just went down by itself or with ctrl pressed.
          // In Windows, both of these activate the menu bar.
          mMenuBarFrame->SetActiveByKeyboard();
          ToggleMenuActiveState();

          if (mMenuBarFrame->IsActive()) {
#ifdef MOZ_WIDGET_GTK
            // In GTK, this also opens the first menu.
            mMenuBarFrame->GetCurrentMenuItem()->OpenMenu(true);
#endif
            aKeyEvent->StopPropagation();
            aKeyEvent->PreventDefault();
            return NS_OK; // consume the event
          }
        }
      }
#endif // !XP_MACOSX
    } 
  }

  return retVal;
}

bool
nsMenuBarListener::IsAccessKeyPressed(nsIDOMKeyEvent* aKeyEvent)
{
  InitAccessKey();
  // No other modifiers are allowed to be down except for Shift.
  uint32_t modifiers = GetModifiers(aKeyEvent);

  return (mAccessKeyMask != MODIFIER_SHIFT &&
          (modifiers & mAccessKeyMask) &&
          (modifiers & ~(mAccessKeyMask | MODIFIER_SHIFT)) == 0);
}

uint32_t
nsMenuBarListener::GetModifiers(nsIDOMKeyEvent* aKeyEvent)
{
  uint32_t modifiers = 0;
  WidgetInputEvent* inputEvent =
    aKeyEvent->GetInternalNSEvent()->AsInputEvent();
  MOZ_ASSERT(inputEvent);

  if (inputEvent->IsShift()) {
    modifiers |= MODIFIER_SHIFT;
  }

  if (inputEvent->IsControl()) {
    modifiers |= MODIFIER_CONTROL;
  }

  if (inputEvent->IsAlt()) {
    modifiers |= MODIFIER_ALT;
  }

  if (inputEvent->IsMeta()) {
    modifiers |= MODIFIER_META;
  }

  if (inputEvent->IsOS()) {
    modifiers |= MODIFIER_OS;
  }

  return modifiers;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  InitAccessKey();

  //handlers shouldn't be triggered by non-trusted events.
  bool trustedEvent = false;
  if (aKeyEvent) {
    aKeyEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  if (mAccessKey && mAccessKeyFocuses)
  {
    bool defaultPrevented = false;
    aKeyEvent->GetDefaultPrevented(&defaultPrevented);

    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    uint32_t theChar;
    keyEvent->GetKeyCode(&theChar);

    // No other modifiers can be down.
    // Especially CTRL.  CTRL+ALT == AltGR, and we'll fuck up on non-US
    // enhanced 102-key keyboards if we don't check this.
    bool isAccessKeyDownEvent =
      ((theChar == (uint32_t)mAccessKey) &&
       (GetModifiers(keyEvent) & ~mAccessKeyMask) == 0);

    if (!mAccessKeyDown) {
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

  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult
nsMenuBarListener::Blur(nsIDOMEvent* aEvent)
{
  if (!mMenuBarFrame->IsMenuOpen() && mMenuBarFrame->IsActive()) {
    ToggleMenuActiveState();
  }
  // Reset the accesskey state because we cannot receive the keyup event for
  // the pressing accesskey.
  mAccessKeyDown = false;
  mAccessKeyDownCanceled = false;
  return NS_OK; // means I am NOT consuming event
}
  
////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  // NOTE: MouseDown method listens all phases

  // Even if the mousedown event is canceled, it means the user don't want
  // to activate the menu.  Therefore, we need to record it at capturing (or
  // target) phase.
  if (mAccessKeyDown) {
    mAccessKeyDownCanceled = true;
  }

  uint16_t phase = 0;
  nsresult rv = aMouseEvent->GetEventPhase(&phase);
  NS_ENSURE_SUCCESS(rv, rv);
  // Don't do anything at capturing phase, any behavior should be cancelable.
  if (phase == nsIDOMEvent::CAPTURING_PHASE) {
    return NS_OK;
  }

  if (!mMenuBarFrame->IsMenuOpen() && mMenuBarFrame->IsActive())
    ToggleMenuActiveState();

  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::HandleEvent(nsIDOMEvent* aEvent)
{
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
  if (eventType.EqualsLiteral("blur")) {
    return Blur(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    return MouseDown(aEvent);
  }

  NS_ABORT();

  return NS_OK;
}
