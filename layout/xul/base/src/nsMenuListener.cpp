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

#include "nsMenuListener.h"
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
 * nsMenuListener implementation
 */

NS_IMPL_ADDREF(nsMenuListener)
NS_IMPL_RELEASE(nsMenuListener)
NS_IMPL_QUERY_INTERFACE3(nsMenuListener, nsIDOMKeyListener, nsIDOMFocusListener, nsIDOMMouseListener)


////////////////////////////////////////////////////////////////////////

nsMenuListener::nsMenuListener(nsIMenuParent* aMenuParent) 
{
  NS_INIT_REFCNT();
  mMenuParent = aMenuParent;
}

////////////////////////////////////////////////////////////////////////
nsMenuListener::~nsMenuListener() 
{
}



////////////////////////////////////////////////////////////////////////
nsresult
nsMenuListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  aKeyEvent->PreventBubble();
	aKeyEvent->PreventCapture();
  aKeyEvent->PreventDefault();

  return NS_ERROR_BASE; // I am consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRInt32 menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
    PRUint32 theChar;
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
    keyEvent->GetKeyCode(&theChar);

    PRBool access = PR_FALSE;
    PRBool accessKeyDown = PR_FALSE;
    switch (menuAccessKey)
    {
      case nsIDOMKeyEvent::DOM_VK_CONTROL:
        keyEvent->GetCtrlKey(&access);
        break;
      case nsIDOMKeyEvent::DOM_VK_ALT:
        keyEvent->GetAltKey(&access);
        break;
      case nsIDOMKeyEvent::DOM_VK_META:
        keyEvent->GetMetaKey(&access);
        break;
      default:
        access = 0;
    }
    if (theChar == nsIDOMKeyEvent::DOM_VK_TAB && accessKeyDown) {
      accessKeyDown = PR_FALSE;
    }

    if (theChar == (PRUint32)menuAccessKey || access) {
      // No other modifiers can be down.
      // Especially CTRL.  CTRL+ALT == AltGR, and
      // we'll fuck up on non-US enhanced 102-key
      // keyboards if we don't check this.
      PRBool ctrl = PR_FALSE;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_CONTROL)
        keyEvent->GetCtrlKey(&ctrl);
      PRBool alt=PR_FALSE;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_ALT)
        keyEvent->GetAltKey(&alt);
      PRBool shift=PR_FALSE;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_SHIFT)
        keyEvent->GetShiftKey(&shift);
      PRBool meta=PR_FALSE;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_META)
        keyEvent->GetMetaKey(&meta);
      if (!(ctrl || alt || shift || meta)) {
        // The access key just went down by itself. Track this.
        accessKeyDown = PR_TRUE;
      }
    }
    if (accessKeyDown)
      mMenuParent->DismissChain();
  }

  aKeyEvent->PreventBubble();
	aKeyEvent->PreventCapture();
  aKeyEvent->PreventDefault();

  return NS_ERROR_BASE; // I am consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	keyEvent->GetKeyCode(&theChar);
  PRBool handled = PR_FALSE;

  if (theChar == NS_VK_LEFT ||
      theChar == NS_VK_RIGHT ||
      theChar == NS_VK_UP ||
      theChar == NS_VK_DOWN) {
    // The arrow keys were pressed. User is moving around within
    // the menus.
	  mMenuParent->KeyboardNavigation(theChar, handled);
  }
  else if (theChar == NS_VK_ESCAPE) {
    // Close one level.
	  mMenuParent->Escape(handled);
    if (!handled)
      mMenuParent->DismissChain();
  }
  else if (theChar == NS_VK_ENTER ||
           theChar == NS_VK_RETURN) {
    // Open one level.
    mMenuParent->Enter();
  }
  else {
    PRInt32 menuAccessKey = -1;
    nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
    if (menuAccessKey) {
      // Do shortcut navigation.
      // A letter was pressed. We want to see if a shortcut gets matched. If
      // so, we'll know the menu got activated.
      keyEvent->GetCharCode(&theChar);
      mMenuParent->ShortcutNavigation(theChar, handled);
    }
  }

  aKeyEvent->PreventBubble();
	aKeyEvent->PreventCapture();
  aKeyEvent->PreventDefault();

  return NS_ERROR_BASE; // I am consuming event
}

////////////////////////////////////////////////////////////////////////

nsresult
nsMenuListener::Focus(nsIDOMEvent* aEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuListener::Blur(nsIDOMEvent* aEvent)
{
  
  return NS_OK; // means I am NOT consuming event
}
  
////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

nsresult 
nsMenuListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult 
nsMenuListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


