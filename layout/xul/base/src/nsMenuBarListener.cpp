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
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"                                                 

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDOMKeyEvent.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

#include "nsIEventStateManager.h"

#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsISupportsArray.h"
#include "nsContentUtils.h"

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ADDREF(nsMenuBarListener)
NS_IMPL_RELEASE(nsMenuBarListener)
NS_IMPL_QUERY_INTERFACE3(nsMenuBarListener, nsIDOMKeyListener, nsIDOMFocusListener, nsIDOMMouseListener)

#define MODIFIER_SHIFT    1
#define MODIFIER_CONTROL  2
#define MODIFIER_ALT      4
#define MODIFIER_META     8

////////////////////////////////////////////////////////////////////////

PRInt32 nsMenuBarListener::mAccessKey = -1;
PRUint32 nsMenuBarListener::mAccessKeyMask = 0;
PRBool nsMenuBarListener::mAccessKeyFocuses = PR_FALSE;

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
  :mAccessKeyDown(PR_FALSE)
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
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  mAccessKey = nsIDOMKeyEvent::DOM_VK_ALT;
  mAccessKeyMask = MODIFIER_ALT;
#else
  mAccessKey = 0;
  mAccessKeyMask = 0;
#endif

  // Get the menu access key value from prefs, overriding the default:
  mAccessKey = nsContentUtils::GetIntPref("ui.key.menuAccessKey", mAccessKey);
  if (mAccessKey == nsIDOMKeyEvent::DOM_VK_SHIFT)
    mAccessKeyMask = MODIFIER_SHIFT;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_CONTROL)
    mAccessKeyMask = MODIFIER_CONTROL;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_ALT)
    mAccessKeyMask = MODIFIER_ALT;
  else if (mAccessKey == nsIDOMKeyEvent::DOM_VK_META)
    mAccessKeyMask = MODIFIER_META;

  mAccessKeyFocuses =
    nsContentUtils::GetBoolPref("ui.key.menuAccessKeyFocuses");
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  InitAccessKey();

  //handlers shouldn't be triggered by non-trusted events.
  if (aKeyEvent) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(aKeyEvent);
    if (privateEvent) {
      PRBool trustedEvent;
      privateEvent->IsTrustedEvent(&trustedEvent);
      if (!trustedEvent)
        return NS_OK;
    }
  }

  if (mAccessKey && mAccessKeyFocuses)
  {
    // On a press of the ALT key by itself, we toggle the menu's 
    // active/inactive state.
    // Get the ascii key code.
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    PRUint32 theChar;
    keyEvent->GetKeyCode(&theChar);

    if (mAccessKeyDown && (PRInt32)theChar == mAccessKey)
    {
      // The access key was down and is now up, and no other
      // keys were pressed in between.
      mMenuBarFrame->ToggleMenuActiveState();
    }
    mAccessKeyDown = PR_FALSE; 

    PRBool active = mMenuBarFrame->IsActive();
    if (active) {
      nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aKeyEvent));

      if (nsevent) {
        nsevent->PreventBubble();
        nsevent->PreventCapture();
      }

      aKeyEvent->PreventDefault();
      return NS_ERROR_BASE; // I am consuming event
    }
  }
  
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  mMenuBarFrame->ClearRecentlyRolledUp();

  // if event has already been handled, bail
  nsCOMPtr<nsIDOMNSUIEvent> uiEvent ( do_QueryInterface(aKeyEvent) );
  if ( uiEvent ) {
    PRBool eventHandled = PR_FALSE;
    uiEvent->GetPreventDefault ( &eventHandled );
    if ( eventHandled )
      return NS_OK;       // don't consume event
  }

  //handlers shouldn't be triggered by non-trusted events.
  if (aKeyEvent) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(aKeyEvent);
    if (privateEvent) {
      PRBool trustedEvent;
      privateEvent->IsTrustedEvent(&trustedEvent);
      if (!trustedEvent)
        return NS_OK;
    }
  }

  nsresult retVal = NS_OK;  // default is to not consume event
  
  InitAccessKey();

  if (mAccessKey)
  {
    nsCOMPtr<nsIDOMNSUIEvent> nsUIEvent = do_QueryInterface(aKeyEvent);
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aKeyEvent));

    PRBool preventDefault;

    nsUIEvent->GetPreventDefault(&preventDefault);
    if (!preventDefault) {
      nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
      PRUint32 keyCode, charCode;
      keyEvent->GetKeyCode(&keyCode);
      keyEvent->GetCharCode(&charCode);

      // Clear the access key flag unless we are pressing the access key.
      if (keyCode != (PRUint32)mAccessKey)
        mAccessKeyDown = PR_FALSE;

      // If charCode == 0, then it is not a printable character.
      // Don't attempt to handle accesskey for non-printable characters.
      if (IsAccessKeyPressed(keyEvent) && charCode)
      {
        // Do shortcut navigation.
        // A letter was pressed. We want to see if a shortcut gets matched. If
        // so, we'll know the menu got activated.
        PRBool active = PR_FALSE;
        mMenuBarFrame->ShortcutNavigation(keyEvent, active);

        if (active) {
          if (nsevent) {
            nsevent->PreventBubble();
            nsevent->PreventCapture();
          }

          aKeyEvent->PreventDefault();
          
          retVal = NS_ERROR_BASE;       // I am consuming event
        }
      }    
#if !defined(XP_MAC) && !defined(XP_MACOSX)
      // Also need to handle F10 specially on Non-Mac platform.
      else if (keyCode == NS_VK_F10) {
        if ((GetModifiers(keyEvent) & ~MODIFIER_CONTROL) == 0) {
          // The F10 key just went down by itself or with ctrl pressed.
          // In Windows, both of these activate the menu bar.
          mMenuBarFrame->ToggleMenuActiveState();

          if (nsevent) {
            nsevent->PreventBubble();
            nsevent->PreventCapture();
          }

          aKeyEvent->PreventDefault();
          return NS_ERROR_BASE; // consume the event
        }
      }
#endif   // !XP_MAC && !XP_MACOSX
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
  if (aKeyEvent) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(aKeyEvent);
    if (privateEvent) {
      PRBool trustedEvent;
      privateEvent->IsTrustedEvent(&trustedEvent);
      if (!trustedEvent)
        return NS_OK;
    }
  }

  if (mAccessKey && mAccessKeyFocuses)
  {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    PRUint32 theChar;
    keyEvent->GetKeyCode(&theChar);

    if (theChar == (PRUint32)mAccessKey && (GetModifiers(keyEvent) & ~mAccessKeyMask) == 0) {
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

      mAccessKeyDown = PR_FALSE;
    }
  }

  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult
nsMenuBarListener::Focus(nsIDOMEvent* aEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::Blur(nsIDOMEvent* aEvent)
{
  if (!mMenuBarFrame->IsOpen() && mMenuBarFrame->IsActive()) {
    mMenuBarFrame->ToggleMenuActiveState();
    PRBool handled;
    mMenuBarFrame->Escape(handled);
    mAccessKeyDown = PR_FALSE;
  }
  return NS_OK; // means I am NOT consuming event
}
  
////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (!mMenuBarFrame->IsOpen() && mMenuBarFrame->IsActive()) {
    mMenuBarFrame->ToggleMenuActiveState();
    PRBool handled;
    mMenuBarFrame->Escape(handled);
  }

  mAccessKeyDown = PR_FALSE;

  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  mMenuBarFrame->ClearRecentlyRolledUp();

  return NS_OK; // means I am NOT consuming event
}

nsresult 
nsMenuBarListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuBarListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}
