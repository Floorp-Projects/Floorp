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

// XXX Notes to self
// 1. The menu bar should get its own view, and it should capture all events when the ALT key
// goes down.
//
// 2. Collapsing the menu bar will destroy the frame. The menu bar will need to unregister its
// listeners when this happens.
//

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

void
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (mIsActive) {
    // Deactivate the menu bar
    mIsActive = PR_FALSE;
    if (mCurrentMenu) {
      // Deactivate the menu.
      nsCOMPtr<nsIContent> content;
      mCurrentMenu->GetContent(getter_AddRefs(content));
      content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
      mCurrentMenu = nsnull;
    }
  }
  else {
    // Activate the menu bar
    mIsActive = PR_TRUE;

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "active" to track the current active menu.
    nsCOMPtr<nsIContent> firstMenuItem;
    nsIFrame* firstFrame;
    GetNextMenuItem(nsnull, &firstFrame);
    if (firstFrame) {
      // Activate the item.
      firstFrame->GetContent(getter_AddRefs(firstMenuItem));
      firstMenuItem->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);

      // Track this item for keyboard navigation.
      mCurrentMenu = firstFrame;
    }
  }
}

void
nsMenuBarFrame::KeyboardNavigation(PRUint32 aDirection)
{
  if (!mCurrentMenu)
    return;

  if (aDirection == NS_VK_RIGHT ||
      aDirection == NS_VK_LEFT) {
    
    // Activate the child at the position specified by index.
    nsIFrame* nextItem;
    
    if (aDirection == NS_VK_RIGHT)
      GetNextMenuItem(mCurrentMenu, &nextItem);
    else GetPreviousMenuItem(mCurrentMenu, &nextItem);

    SetCurrentMenuItem(nextItem);
  }
}

NS_IMETHODIMP
nsMenuBarFrame::GetNextMenuItem(nsIFrame* aStart, nsIFrame** aResult)
{
  nsIFrame* currFrame = aStart ? aStart : mFrames.FirstChild();
  currFrame->GetNextSibling(&currFrame);
  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
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
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
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
  nsIFrame* currFrame = aStart ? aStart : mFrames.LastChild();
  currFrame = mFrames.GetPrevSiblingFor(currFrame);
  while (currFrame) {
    nsCOMPtr<nsIContent> current;
    currFrame->GetContent(getter_AddRefs(current));

    // See if it's a menu item.
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
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
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::xpmenu) {
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

 // Unset the current child.
  if (mCurrentMenu) {
    nsCOMPtr<nsIContent> content;
    mCurrentMenu->GetContent(getter_AddRefs(content));
    content->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
  }

  // Set the new child.
  if (aMenuItem) {
    nsCOMPtr<nsIContent> content;
    aMenuItem->GetContent(getter_AddRefs(content));
    content->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);
  }
  mCurrentMenu = aMenuItem;

  return NS_OK;
}