/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#include "nsIDOMUIEvent.h"
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
nsMenuBarListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventReceiver>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMKeyListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMKeyListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aKeyEvent)
{  
  // On a press of the ALT key by itself, we toggle the menu's 
  // active/inactive state.
  // Get the ascii key code.
  nsCOMPtr<nsIDOMUIEvent> theEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	theEvent->GetKeyCode(&theChar);
  if (theChar == NS_VK_ALT && mAltKeyDown) {
    // The ALT key was down and is now up.
    mAltKeyDown = PR_FALSE;
    mMenuBarFrame->ToggleMenuActiveState();
  }
  
  PRBool active = mMenuBarFrame->IsActive();
  if (active)
    return NS_ERROR_BASE; // I am consuming event
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRBool active = mMenuBarFrame->IsActive();

  nsCOMPtr<nsIDOMUIEvent> theEvent = do_QueryInterface(aKeyEvent);
  PRUint32 theChar;
	theEvent->GetKeyCode(&theChar);
  if (theChar == NS_VK_ALT) {
    // The ALT key just went down. Track this.
    mAltKeyDown = PR_TRUE;
    return NS_OK;
  }
  
  PRBool altKeyWasDown = mAltKeyDown;
  mAltKeyDown = PR_FALSE;

  if (theChar == NS_VK_LEFT ||
      theChar == NS_VK_RIGHT ||
      theChar == NS_VK_UP ||
      theChar == NS_VK_DOWN) {
    // The arrow keys were pressed. User is moving around within
    // the menus.
    if (active) 
      mMenuBarFrame->KeyboardNavigation(theChar);
  }
  else if (theChar == NS_VK_ESCAPE) {
    // Close one level.
    if (active)
      mMenuBarFrame->Escape();
  }
  else if (theChar == NS_VK_ENTER ||
           theChar == NS_VK_RETURN) {
    // Open one level.
    if (active)
      mMenuBarFrame->Enter();
  }
  else if (active || altKeyWasDown) {
    // Get the character code.
    nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aKeyEvent);
    if (uiEvent) {
      // See if a letter was pressed.
      PRUint32 charCode;
      uiEvent->GetKeyCode(&charCode);

      // Do shortcut navigation.
      // A letter was pressed. We want to see if a shortcut gets matched. If
      // so, we'll know the menu got activated.
      mMenuBarFrame->ShortcutNavigation(charCode, active);
    }
  }
  if (active)
    return NS_ERROR_BASE; // I am consuming event
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMUIEvent> theEvent = do_QueryInterface(aKeyEvent);

  PRBool active = mMenuBarFrame->IsActive();
  
  if (active)
    return NS_ERROR_BASE; // I am consuming event
  return NS_OK; // means I am NOT consuming event
}

  ////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


