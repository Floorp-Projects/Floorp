/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsMenuBarFrame.h"

#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsMenuFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"

//
// NS_NewMenuBarFrame
//
// Wrapper for creating a new menu Bar container
//
nsresult
NS_NewMenuBarFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuBarFrame* it = new nsMenuBarFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuBarFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuBarFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP nsMenuBarFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(nsIMenuParent::GetIID())) {                                         
    *aInstancePtr = (void*)(nsIMenuParent*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}
//
// nsMenuBarFrame cntr
//
nsMenuBarFrame::nsMenuBarFrame()
:mIsActive(PR_FALSE)
{

} // cntr

NS_IMETHODIMP
nsMenuBarFrame::Init(nsIPresContext&  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Create the menu bar listener.
  mMenuBarListener = new nsMenuBarListener(this);

  // Hook up the menu bar as a key listener (capturer) on the whole document.  It will see every
  // key press that occurs before anyone else does and will know when to take control.
  nsCOMPtr<nsIDocument> doc;
  aContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(doc);
  nsIDOMEventListener* domEventListener = (nsIDOMKeyListener*)mMenuBarListener;

  target->AddEventListener("keypress", domEventListener, PR_TRUE); 
	target->AddEventListener("keydown", domEventListener, PR_TRUE);  
  target->AddEventListener("keyup", domEventListener, PR_TRUE);   
  
  return rv;
}

NS_IMETHODIMP
nsMenuBarFrame::SetActive(PRBool aActiveFlag)
{
  mIsActive = aActiveFlag;
  return NS_OK;
}

void
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    mIsActive = PR_FALSE;
    if (mCurrentMenu) {
      // Deactivate the menu.
      nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
      menuFrame->OpenMenu(PR_FALSE);
      menuFrame->SelectMenu(PR_FALSE);
      mCurrentMenu = nsnull;
    }
  }
  else {
    // Activate the menu bar
    SetActive(PR_TRUE);

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "active" to track the current active menu.
    nsCOMPtr<nsIContent> firstMenuItem;
    nsIFrame* firstFrame;
    GetNextMenuItem(nsnull, &firstFrame);
    if (firstFrame) {
      nsMenuFrame* menuFrame = (nsMenuFrame*)firstFrame;
      menuFrame->SelectMenu(PR_TRUE);
      
      // Track this item for keyboard navigation.
      mCurrentMenu = firstFrame;
    }
  }
}

nsIFrame*
nsMenuBarFrame::FindMenuWithShortcut(PRUint32 aLetter)
{
  // Enumerate over our list of frames.
  nsIFrame* currFrame = mFrames.FirstChild();
  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      // Get the shortcut attribute.
      nsString shortcutKey = "";
      current->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey, shortcutKey);
      shortcutKey.ToUpperCase();
      if (shortcutKey.Length() > 0) {
        // We've got something.
        PRUnichar shortcutChar = shortcutKey.CharAt(0);
        if (shortcutChar == aLetter) {
          // We match!
          return currFrame;
        }
      }
    }
    currFrame->GetNextSibling(&currFrame);
  }
  return nsnull;
}

void 
nsMenuBarFrame::ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag)
{
  if (mCurrentMenu) {
    nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
    if (menuFrame->IsOpen()) {
      // No way this applies to us. Give it to our child.
      menuFrame->ShortcutNavigation(aLetter, aHandledFlag);
      return;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  nsIFrame* result = FindMenuWithShortcut(aLetter);
  if (result) {
    // We got one!
    aHandledFlag = PR_TRUE;
    mIsActive = PR_TRUE;
    nsMenuFrame* menuFrame = (nsMenuFrame*)result;
    SetCurrentMenuItem(result);
    menuFrame->OpenMenu(PR_TRUE);
    menuFrame->SelectFirstItem();
  }
}

void
nsMenuBarFrame::KeyboardNavigation(PRUint32 aDirection)
{
  if (!mCurrentMenu)
    return;

  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
   
  PRBool handled = PR_FALSE;
  if (menuFrame->IsOpen()) {
    // Let the child menu try to handle it.
    menuFrame->KeyboardNavigation(aDirection, handled);
  }

  if (handled)
    return;

  if (aDirection == NS_VK_RIGHT || aDirection == NS_VK_LEFT) {
    
    nsIFrame* nextItem;
    
    if (aDirection == NS_VK_RIGHT)
      GetNextMenuItem(mCurrentMenu, &nextItem);
    else GetPreviousMenuItem(mCurrentMenu, &nextItem);

    SetCurrentMenuItem(nextItem);
    if (nextItem) {
      nsMenuFrame* menu = (nsMenuFrame*)nextItem;
      if (menu->IsOpen()) {
        // Select the first item.
        menu->SelectFirstItem();
      }
    }
  }
  else if (aDirection == NS_VK_UP || aDirection == NS_VK_DOWN) {
    // Open the menu and select its first item.
    menuFrame->OpenMenu(PR_TRUE);
    menuFrame->SelectFirstItem();
  }
}

NS_IMETHODIMP
nsMenuBarFrame::GetNextMenuItem(nsIFrame* aStart, nsIFrame** aResult)
{
  nsIFrame* currFrame;
  if (aStart) {
    currFrame = aStart;
    if (currFrame)
      currFrame->GetNextSibling(&currFrame);
  }
  else currFrame = mFrames.FirstChild();

  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }
    currFrame->GetNextSibling(&currFrame);
  }

  currFrame = mFrames.FirstChild();

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != aStart) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }

    currFrame->GetNextSibling(&currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::GetPreviousMenuItem(nsIFrame* aStart, nsIFrame** aResult)
{
  nsIFrame* currFrame;
  if (aStart) {
    currFrame = aStart;
    if (currFrame)
      currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }
  else currFrame = mFrames.LastChild();

  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }
    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  currFrame = mFrames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != aStart) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      *aResult = currFrame;
      return NS_OK;
    }

    currFrame = mFrames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  *aResult = aStart;
  return NS_OK;
}

NS_IMETHODIMP nsMenuBarFrame::SetCurrentMenuItem(nsIFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  PRBool wasOpen = PR_FALSE;
  
  // Unset the current child.
  if (mCurrentMenu) {
    wasOpen = menuFrame->IsOpen();
    if (wasOpen)
      menuFrame->OpenMenu(PR_FALSE);
    menuFrame->SelectMenu(PR_FALSE);
  }

  // Set the new child.
  if (aMenuItem) {
    nsMenuFrame* newFrame = (nsMenuFrame*)aMenuItem;
    newFrame->SelectMenu(PR_TRUE);
    
    newFrame->MarkAsGenerated(); // Have the menu building. Get it ready to be shown.

    if (wasOpen)
      newFrame->OpenMenu(PR_TRUE);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

void 
nsMenuBarFrame::Escape()
{
  if (!mCurrentMenu)
    return;

  // See if our menu is open.
  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  if (menuFrame->IsOpen()) {
    // Let the child menu handle this.
    PRBool handled = PR_FALSE;
    menuFrame->Escape(handled);
    if (!handled) {
      // Close up this menu but keep our current menu item
      // designation.
      menuFrame->OpenMenu(PR_FALSE);
    }
    return;
  }

  // It's us. Just set our active flag to false.
  mIsActive = PR_FALSE;

  // Clear our current menu item if we've got one.
  SetCurrentMenuItem(nsnull);
}

void 
nsMenuBarFrame::Enter()
{
  if (!mCurrentMenu)
    return;

  // See if our menu is open.
  nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
  if (menuFrame->IsOpen()) {
    // Let the child menu handle this.
    menuFrame->Enter();
    return;
  }

  // It's us. Open the current menu.
  menuFrame->OpenMenu(PR_TRUE);
  menuFrame->SelectFirstItem();
}

NS_IMETHODIMP
nsMenuBarFrame::HideChain()
{
  if (mCurrentMenu) {
    nsMenuFrame* menuFrame = (nsMenuFrame*)mCurrentMenu;
    menuFrame->ActivateMenu(PR_FALSE);
    menuFrame->SelectMenu(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuBarFrame::DismissChain()
{
  SetCurrentMenuItem(nsnull);
  SetActive(PR_FALSE);
  return NS_OK;
}


PRBool 
nsMenuBarFrame::IsValidItem(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  if (tag && (tag.get() == nsXULAtoms::menu ||
              tag.get() == nsXULAtoms::menuitem) &&
      !IsDisabled(aContent))
      return PR_TRUE;

  return PR_FALSE;
}

PRBool 
nsMenuBarFrame::IsDisabled(nsIContent* aContent)
{
  nsString disabled = "";
  aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled == "true")
    return PR_TRUE;
  return PR_FALSE;
}
