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

#include "nsXULAtoms.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"

#define NS_MENU_POPUP_LIST_INDEX   (NS_AREA_FRAME_ABSOLUTE_LIST_INDEX + 1)

//
// NS_NewMenuFrame
//
// Wrapper for creating a new menu popup container
//
nsresult
NS_NewMenuFrame(nsIFrame** aNewFrame, PRInt32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuFrame* it = new nsMenuFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  if (aFlags)
    it->SetIsMenu(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP nsMenuFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                  
  *aInstancePtr = NULL;
  return nsBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}

//
// nsMenuFrame cntr
//
nsMenuFrame::nsMenuFrame()
:mMenuOpen(PR_FALSE), mIsMenu(PR_FALSE), mMenuParent(nsnull)
{

} // cntr

NS_IMETHODIMP
nsMenuFrame::Init(nsIPresContext&  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Set our menu parent.
  nsCOMPtr<nsIMenuParent> menuparent = do_QueryInterface(aParent);
  mMenuParent = menuparent.get();
  return rv;
}

// The following methods are all overridden to ensure that the xpmenuchildren frame
// is placed in the appropriate list.
NS_IMETHODIMP
nsMenuFrame::FirstChild(nsIAtom*   aListName,
                        nsIFrame** aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsBoxFrame::FirstChild(aListName, aFirstChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {

    nsFrameList frames(aChildList);

    // We may have an xpmenuchildren in here. Get it out, and move it into
    // the popup frame list.
    nsIFrame* frame = frames.FirstChild();
    while (frame) {
      nsCOMPtr<nsIContent> content;
      frame->GetContent(getter_AddRefs(content));
      nsCOMPtr<nsIAtom> tag;
      content->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::xpmenuchildren) {
        // Remove this frame from the list and place it in the other list.
        frames.RemoveFrame(frame);
        mPopupFrames.AppendFrame(this, frame);
        rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
        return rv;
      }
      frame->GetNextSibling(&frame);
    }
    rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }
  return rv;
}

NS_IMETHODIMP
nsMenuFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const
{
   // Maintain a separate child list for the menu contents.
   // This is necessary because we don't want the menu contents to be included in the layout
   // of the menu's single item because it would take up space, when it is supposed to
   // be floating above the display.
  /*NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  
  *aListName = nsnull;
  if (NS_MENU_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
    return NS_OK;
  }*/
  return nsBoxFrame::GetAdditionalChildListName(aIndex, aListName);
}

NS_IMETHODIMP
nsMenuFrame::Destroy(nsIPresContext& aPresContext)
{
   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);
  return nsBoxFrame::Destroy(aPresContext);
}

// Called to prevent events from going to anything inside the menu.
NS_IMETHODIMP
nsMenuFrame::GetFrameForPoint(const nsPoint& aPoint, 
                              nsIFrame**     aFrame)
{
  *aFrame = this; // Capture all events so that we can perform selection
  return NS_OK;
}

NS_IMETHODIMP 
nsMenuFrame::HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eConsumeDoDefault;
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    // The menu item was clicked. Bring up the menu.
    nsIFrame* frame = mPopupFrames.FirstChild();
    if (frame) {
      // We have children.
      ToggleMenuState();
    }
  }
  else if (aEvent->message == NS_MOUSE_EXIT) {
    // Deactivate the menu.
    if (mMenuParent && !mMenuOpen)
      mMenuParent->SetCurrentMenuItem(nsnull);
  }
  else if (aEvent->message == NS_MOUSE_ENTER) {
    // Let the menu parent know we're the new item.
    if (mMenuParent)
      mMenuParent->SetCurrentMenuItem(this);
  }
  return NS_OK;
}

void
nsMenuFrame::ToggleMenuState()
{  
  if (mMenuOpen) {
    OpenMenu(PR_FALSE);
  }
  else {
    OpenMenu(PR_TRUE);
  }
}

void
nsMenuFrame::SelectMenu(PRBool aActivateFlag)
{
  if (aActivateFlag) {
    // Highlight the menu.
    mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);
  }
  else {
    // Unhighlight the menu.
    mContent->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
  }
}

void 
nsMenuFrame::OpenMenu(PRBool aActivateFlag) 
{
  if (!mIsMenu)
    return;

  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;

  if (aActivateFlag) {

    // Sync up the view.
    if (menuPopup)
      menuPopup->SyncViewWithFrame(PR_TRUE);
      
    // Open the menu.
    mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::open, "true", PR_TRUE);
    if (child) {
      // We've got some children for real.
      child->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);
      
      // Tell the menu bar we're active.
      mMenuParent->SetActive();
    }

    mMenuOpen = PR_TRUE;
    //if (menuPopup)
    //  menuPopup->CaptureMouseEvents(PR_TRUE);
  }
  else {
    // Close the menu.
    mContent->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::open, PR_TRUE);
    if (child)
      child->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
    mMenuOpen = PR_FALSE;

    // Make sure we clear out our own items.
    if (menuPopup)
      menuPopup->SetCurrentMenuItem(nsnull);
  }
}

void
nsMenuFrame::GetMenuChildrenElement(nsIContent** aResult)
{
  *aResult = nsnull;
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    frame->GetContent(aResult);
  }
}

NS_IMETHODIMP
nsMenuFrame::Reflow(nsIPresContext&   aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  nsIFrame* frame = mPopupFrames.FirstChild();
    
  if (!frame || (rv != NS_OK))
    return rv;

  // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame,
                                   availSize);
  kidReflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
  kidReflowState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
    
   // Reflow child
  nscoord w = aDesiredSize.width;
  nscoord h = aDesiredSize.height;
  
  rv = ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, aStatus);

   // Set the child's width and height to its desired size
  nsRect rect;
  frame->GetRect(rect);
  rect.width = aDesiredSize.width;
  rect.height = aDesiredSize.height;
  frame->SetRect(rect);

  // Don't let it affect our size.
  aDesiredSize.width = w;
  aDesiredSize.height = h;
  
  return rv;
}

void
nsMenuFrame::KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->KeyboardNavigation(aDirection, aHandledFlag);
  }
}

void
nsMenuFrame::SelectFirstItem()
{
}