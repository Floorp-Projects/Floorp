/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Mark Hammond <markh@ActiveState.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMNSUIEvent.h"
#include "nsGUIEvent.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDOMKeyEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

#include "nsIEventStateManager.h"

#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsISupportsArray.h"

#include "nsIPref.h"

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ADDREF(nsMenuBarListener)
NS_IMPL_RELEASE(nsMenuBarListener)
NS_IMPL_QUERY_INTERFACE3(nsMenuBarListener, nsIDOMKeyListener, nsIDOMFocusListener, nsIDOMMouseListener)


////////////////////////////////////////////////////////////////////////

PRInt32 nsMenuBarListener::mAccessKey = -1;
PRBool nsMenuBarListener::mAccessKeyFocuses = PR_FALSE;

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
  :mAccessKeyDown(PR_FALSE)
{
  NS_INIT_REFCNT();
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
#else
  mAccessKey = 0;
#endif

  // Get the menu access key value from prefs, overriding the default:
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && prefs)
  {
    rv = prefs->GetIntPref("ui.key.menuAccessKey", &mAccessKey);
    rv |= prefs->GetBoolPref("ui.key.menuAccessKeyFocuses",
                             &mAccessKeyFocuses);
  }
#ifdef DEBUG_akkana
  if (NS_FAILED(rv) || !prefs)
  {
    NS_ASSERTION(PR_FALSE,"Menubar listener couldn't get accel key from prefs!\n");
  }
#endif
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  InitAccessKey();

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
      aKeyEvent->PreventBubble();
      aKeyEvent->PreventCapture();
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
  // if event has already been handled, bail
  nsCOMPtr<nsIDOMNSUIEvent> uiEvent ( do_QueryInterface(aKeyEvent) );
  if ( uiEvent ) {
    PRBool eventHandled = PR_FALSE;
    uiEvent->GetPreventDefault ( &eventHandled );
    if ( eventHandled )
      return NS_OK;       // don't consume event
  }

  nsresult retVal = NS_OK;  // default is to not consume event
  
  InitAccessKey();

  if (mAccessKey)
  {
    nsCOMPtr<nsIDOMNSUIEvent> nsUIEvent = do_QueryInterface(aKeyEvent);
    PRBool preventDefault;

    nsUIEvent->GetPreventDefault(&preventDefault);
    if (!preventDefault) {
      nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
      PRUint32 theChar;
      keyEvent->GetKeyCode(&theChar);

      if (IsAccessKeyPressed(keyEvent) && (theChar != (PRUint32)mAccessKey))
      {
        mAccessKeyDown = PR_FALSE;

        // Do shortcut navigation.
        // A letter was pressed. We want to see if a shortcut gets matched. If
        // so, we'll know the menu got activated.
        keyEvent->GetCharCode(&theChar);

        PRBool active = PR_FALSE;
        mMenuBarFrame->ShortcutNavigation(theChar, active);

        if (active) {
          aKeyEvent->PreventBubble();
          aKeyEvent->PreventCapture();
          aKeyEvent->PreventDefault();
          
          retVal = NS_ERROR_BASE;       // I am consuming event
        }
      }    
#ifdef XP_WIN
      // Also need to handle F10 specially on Windows.
      else if (theChar == NS_VK_F10) {
        PRBool alt,ctrl,shift,meta;
        keyEvent->GetAltKey(&alt);
        keyEvent->GetCtrlKey(&ctrl);
        keyEvent->GetShiftKey(&shift);
        keyEvent->GetMetaKey(&meta);
        if (!(shift || alt || meta)) {
          // The F10 key just went down by itself or with ctrl pressed.
          // In Windows, both of these activate the menu bar.
          mMenuBarFrame->ToggleMenuActiveState();

          aKeyEvent->PreventBubble();
          aKeyEvent->PreventCapture();
          aKeyEvent->PreventDefault();
          return NS_ERROR_BASE; // consume the event
        }
      }
#endif   // XP_WIN
    } 
  }
  return retVal;
}

PRBool
nsMenuBarListener::IsAccessKeyPressed(nsIDOMKeyEvent* aKeyEvent)
{
  PRBool access;
  switch (mAccessKey)
  {
    case nsIDOMKeyEvent::DOM_VK_CONTROL:
      aKeyEvent->GetCtrlKey(&access);
      return access;
    case nsIDOMKeyEvent::DOM_VK_ALT:
      aKeyEvent->GetAltKey(&access);
      return access;
    case nsIDOMKeyEvent::DOM_VK_META:
      aKeyEvent->GetMetaKey(&access);
      return access;
    default:
      return PR_FALSE;
  }
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  InitAccessKey();

  if (mAccessKey && mAccessKeyFocuses)
  {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    PRUint32 theChar;
    keyEvent->GetKeyCode(&theChar);

    if (theChar == (PRUint32)mAccessKey) {
      // No other modifiers can be down.
      // Especially CTRL.  CTRL+ALT == AltGR, and
      // we'll fuck up on non-US enhanced 102-key
      // keyboards if we don't check this.
      PRBool ctrl = PR_FALSE;
      if (mAccessKey != nsIDOMKeyEvent::DOM_VK_CONTROL)
        keyEvent->GetCtrlKey(&ctrl);
      PRBool alt=PR_FALSE;
      if (mAccessKey != nsIDOMKeyEvent::DOM_VK_ALT)
        keyEvent->GetAltKey(&alt);
      PRBool shift=PR_FALSE;
      if (mAccessKey != nsIDOMKeyEvent::DOM_VK_SHIFT)
        keyEvent->GetShiftKey(&shift);
      PRBool meta=PR_FALSE;
      if (mAccessKey != nsIDOMKeyEvent::DOM_VK_META)
        keyEvent->GetMetaKey(&meta);
      if (!(ctrl || alt || shift || meta)) {
        // The access key just went down by itself. Track this.
        mAccessKeyDown = PR_TRUE;
      }
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


