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

  target->AddEventListener("keypress", domEventListener, PR_FALSE, PR_TRUE); 
	target->AddEventListener("keydown", domEventListener, PR_FALSE, PR_TRUE);  
  target->AddEventListener("keyup", domEventListener, PR_FALSE, PR_TRUE);   
  
  // The menu bar should also observe all mouse events that happen within it
  // (which it can do using bubbling).
  target->AddEventListenerByIID((nsIDOMMouseListener*)mMenuBarListener, nsIDOMMouseListener::GetIID());
  target->AddEventListenerByIID((nsIDOMMouseMotionListener*)mMenuBarListener, nsIDOMMouseMotionListener::GetIID());

  return rv;
}

void
nsMenuBarFrame::ToggleMenuActiveState()
{
  if (IsActive()) {
    // Deactivate the menu bar
    mIsActive = PR_FALSE;
    if (mCurrentMenu) {
      // Deactivate the menu.
      mCurrentMenu->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
      mCurrentMenu = nsnull;
    }
  }
  else {
    // Activate the menu bar
    mIsActive = PR_TRUE;

    // Set the active menu to be the top left item (e.g., the File menu).
    // We use an attribute called "active" to track the current active menu.
    nsCOMPtr<nsIContent> firstMenuItem;
    GetFirstMenuItem(getter_AddRefs(firstMenuItem));
    if (firstMenuItem) {
      // Activate the item.
      firstMenuItem->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);

      // Track this item for keyboard navigation.
      mCurrentMenu = firstMenuItem.get();
    }
  }
}

void 
nsMenuBarFrame::GetFirstMenuItem(nsIContent** aMenuItem)
{
  PRInt32 childCount;
  *aMenuItem = nsnull;
  mContent->ChildCount(childCount);
  if (childCount > 0) {
    // We have at least one menu item. Good. Fetch it.
    mContent->ChildAt(0, *aMenuItem);
  }
}