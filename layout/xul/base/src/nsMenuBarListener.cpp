/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Mark Hammond <markh@ActiveState.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"

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

using namespace mozilla;

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ISUPPORTS1(nsMenuBarListener, nsIDOMEventListener)

#define MODIFIER_SHIFT    1
#define MODIFIER_CONTROL  2
#define MODIFIER_ALT      4
#define MODIFIER_META     8

////////////////////////////////////////////////////////////////////////

PRInt32 nsMenuBarListener::mAccessKey = -1;
PRUint32 nsMenuBarListener::mAccessKeyMask = 0;
PRBool nsMenuBarListener::mAccessKeyFocuses = PR_FALSE;

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
  :mAccessKeyDown(PR_FALSE), mAccessKeyDownCanceled(PR_FALSE)
{
  mMenuBarFrame = aMenuBar;
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() 
{
}

nsresult
nsMenuBarListener::GetMenuAccessKey(PRInt32* aAccessKey)
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
      pm->HidePopup(popupFrame->GetContent(), PR_FALSE, PR_FALSE, PR_TRUE);
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
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  PRBool trustedEvent = PR_FALSE;

  if (domNSEvent) {
    domNSEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  if (mAccessKey && mAccessKeyFocuses)
  {
    // On a press of the ALT key by itself, we toggle the menu's 
    // active/inactive state.
    // Get the ascii key code.
    PRUint32 theChar;
    keyEvent->GetKeyCode(&theChar);

    if (mAccessKeyDown && !mAccessKeyDownCanceled &&
        (PRInt32)theChar == mAccessKey)
    {
      // The access key was down and is now up, and no other
      // keys were pressed in between.
      if (!mMenuBarFrame->IsActive()) {
        mMenuBarFrame->SetActiveByKeyboard();
      }
      ToggleMenuActiveState();
    }
    mAccessKeyDown = PR_FALSE;
    mAccessKeyDownCanceled = PR_FALSE;

    PRBool active = mMenuBarFrame->IsActive();
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
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  if (domNSEvent) {
    PRBool eventHandled = PR_FALSE;
    domNSEvent->GetPreventDefault(&eventHandled);
    if (eventHandled) {
      return NS_OK;       // don't consume event
    }
  }

  //handlers shouldn't be triggered by non-trusted events.
  PRBool trustedEvent = PR_FALSE;
  if (domNSEvent) {
    domNSEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  nsresult retVal = NS_OK;  // default is to not consume event
  
  InitAccessKey();

  if (mAccessKey)
  {
    PRBool preventDefault;
    domNSEvent->GetPreventDefault(&preventDefault);
    if (!preventDefault) {
      nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
      PRUint32 keyCode, charCode;
      keyEvent->GetKeyCode(&keyCode);
      keyEvent->GetCharCode(&charCode);

      PRBool hasAccessKeyCandidates = charCode != 0;
      if (!hasAccessKeyCandidates) {
        nsEvent* nativeEvent = nsContentUtils::GetNativeEvent(aKeyEvent);
        nsKeyEvent* nativeKeyEvent = static_cast<nsKeyEvent*>(nativeEvent);
        if (nativeKeyEvent) {
          nsAutoTArray<PRUint32, 10> keys;
          nsContentUtils::GetAccessKeyCandidates(nativeKeyEvent, keys);
          hasAccessKeyCandidates = !keys.IsEmpty();
        }
      }

      // Cancel the access key flag unless we are pressing the access key.
      if (keyCode != (PRUint32)mAccessKey) {
        mAccessKeyDownCanceled = PR_TRUE;
      }

      if (IsAccessKeyPressed(keyEvent) && hasAccessKeyCandidates) {
        // Do shortcut navigation.
        // A letter was pressed. We want to see if a shortcut gets matched. If
        // so, we'll know the menu got activated.
        nsMenuFrame* result = mMenuBarFrame->FindMenuWithShortcut(keyEvent);
        if (result) {
          mMenuBarFrame->SetActiveByKeyboard();
          mMenuBarFrame->SetActive(PR_TRUE);
          result->OpenMenu(PR_TRUE);

          // The opened menu will listen next keyup event.
          // Therefore, we should clear the keydown flags here.
          mAccessKeyDown = mAccessKeyDownCanceled = PR_FALSE;

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

          aKeyEvent->StopPropagation();
          aKeyEvent->PreventDefault();
          return NS_OK; // consume the event
        }
      }
#endif // !XP_MACOSX
    } 
  }

  return retVal;
}

PRBool
nsMenuBarListener::IsAccessKeyPressed(nsIDOMKeyEvent* aKeyEvent)
{
  InitAccessKey();
  // No other modifiers are allowed to be down except for Shift.
  PRUint32 modifiers = GetModifiers(aKeyEvent);

  return (mAccessKeyMask != MODIFIER_SHIFT &&
          (modifiers & mAccessKeyMask) &&
          (modifiers & ~(mAccessKeyMask | MODIFIER_SHIFT)) == 0);
}

PRUint32
nsMenuBarListener::GetModifiers(nsIDOMKeyEvent* aKeyEvent)
{
  PRUint32 modifiers = 0;
  PRBool modifier;

  aKeyEvent->GetShiftKey(&modifier);
  if (modifier)
    modifiers |= MODIFIER_SHIFT;

  aKeyEvent->GetCtrlKey(&modifier);
  if (modifier)
    modifiers |= MODIFIER_CONTROL;

  aKeyEvent->GetAltKey(&modifier);
  if (modifier)
    modifiers |= MODIFIER_ALT;

  aKeyEvent->GetMetaKey(&modifier);
  if (modifier)
    modifiers |= MODIFIER_META;

  return modifiers;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  InitAccessKey();

  //handlers shouldn't be triggered by non-trusted events.
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  PRBool trustedEvent = PR_FALSE;

  if (domNSEvent) {
    domNSEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  if (mAccessKey && mAccessKeyFocuses)
  {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    PRUint32 theChar;
    keyEvent->GetKeyCode(&theChar);

    if (!mAccessKeyDownCanceled && theChar == (PRUint32)mAccessKey &&
        (GetModifiers(keyEvent) & ~mAccessKeyMask) == 0) {
      // No other modifiers can be down.
      // Especially CTRL.  CTRL+ALT == AltGR, and
      // we'll fuck up on non-US enhanced 102-key
      // keyboards if we don't check this.
      mAccessKeyDown = PR_TRUE;
    }
    else {
      // Some key other than the access key just went down,
      // so we won't activate the menu bar when the access
      // key is released.

      mAccessKeyDownCanceled = PR_TRUE;
    }
  }

  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult
nsMenuBarListener::Blur(nsIDOMEvent* aEvent)
{
  if (!mMenuBarFrame->IsMenuOpen() && mMenuBarFrame->IsActive()) {
    ToggleMenuActiveState();
    mAccessKeyDown = PR_FALSE;
    mAccessKeyDownCanceled = PR_FALSE;
  }
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
    mAccessKeyDownCanceled = PR_TRUE;
  }

  PRUint16 phase = 0;
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
