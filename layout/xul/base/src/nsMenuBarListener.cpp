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

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ADDREF(nsMenuBarListener)
NS_IMPL_RELEASE(nsMenuBarListener)
NS_IMPL_QUERY_INTERFACE3(nsMenuBarListener, nsIDOMKeyListener, nsIDOMFocusListener, nsIDOMMouseListener)


////////////////////////////////////////////////////////////////////////

nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
:mAltKeyDown(PR_FALSE)
{
  NS_INIT_REFCNT();
  mMenuBarFrame = aMenuBar;
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() 
{
}



////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  // On a press of the ALT key by itself, we toggle the menu's 
  // active/inactive state.
  // Get the ascii key code.
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	keyEvent->GetKeyCode(&theChar);

#if !defined(XP_UNIX) || defined(NTO)
  if (theChar == NS_VK_ALT && mAltKeyDown) {
    // The ALT key was down and is now up.
    mMenuBarFrame->ToggleMenuActiveState();
  }
  mAltKeyDown = PR_FALSE; 
#endif

  PRBool active = mMenuBarFrame->IsActive();
  if (active) {
	  aKeyEvent->PreventBubble();
    aKeyEvent->PreventCapture();
    aKeyEvent->PreventDefault();
    return NS_ERROR_BASE; // I am consuming event
  } 
  
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
#if !defined(XP_UNIX) || defined(NTO)
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	keyEvent->GetKeyCode(&theChar);

  if (mAltKeyDown && (theChar != NS_VK_ALT)) {
    mAltKeyDown = PR_FALSE;

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
#endif
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
#if !defined(XP_UNIX) || defined(NTO)
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	keyEvent->GetKeyCode(&theChar);
  PRBool alt;
  keyEvent->GetAltKey(&alt);
  if (theChar == NS_VK_TAB && mAltKeyDown) {
    mAltKeyDown = PR_FALSE;
  }

  if (theChar == NS_VK_ALT && alt) {
    // No other modifiers can be down.
    // Especially CTRL.  CTRL+ALT == AltGR, and
    // we'll fuck up on non-US enhanced 102-key
    // keyboards if we don't check this.
    PRBool ctrl,shift,meta;
    keyEvent->GetCtrlKey(&ctrl);
    keyEvent->GetShiftKey(&shift);
    keyEvent->GetMetaKey(&meta);
    if (!(ctrl || shift || meta)) {
      // The ALT key just went down by itself. Track this.
      mAltKeyDown = PR_TRUE;
    }
  }
#endif

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
	  mAltKeyDown = PR_FALSE;
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
	  mAltKeyDown = PR_FALSE;
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


