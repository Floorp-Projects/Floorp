/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMNSUIEvent.h"

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

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

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
#ifndef XP_MAC
  mAccessKey = nsIDOMKeyEvent::DOM_VK_ALT;
#else
  mAccessKey = 0;
#endif

  // Get the menu access key value from prefs, overriding the default:
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    rv = prefs->GetIntPref("ui.key.menuAccessKey", &mAccessKey);
    rv |= prefs->GetBoolPref("ui.key.menuAccessKeyFocuses",
                             &mAccessKeyFocuses);
  }
#ifdef DEBUG_akkana
  if (NS_FAILED(rv))
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
      // The access key was down and is now up.
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

      PRBool doShortcut = PR_FALSE;
      if (!mAccessKeyFocuses) {
        if (IsAccessKeyPressed(keyEvent) && (theChar != (PRUint32)mAccessKey))
          doShortcut = PR_TRUE;
      }
      else if (mAccessKeyDown && (theChar != (PRUint32)mAccessKey)) {
        doShortcut = PR_TRUE;
      }

      if (doShortcut)
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
        }
        return NS_ERROR_BASE; // I am consuming event
      }    
    } 
  }
  return NS_OK;
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

    PRBool access = IsAccessKeyPressed(keyEvent);
    if (theChar == nsIDOMKeyEvent::DOM_VK_TAB && mAccessKeyDown) {
      mAccessKeyDown = PR_FALSE;
    }

    if (theChar == (PRUint32)mAccessKey || access) {
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
	  mAccessKeyDown = PR_FALSE;
  }
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


