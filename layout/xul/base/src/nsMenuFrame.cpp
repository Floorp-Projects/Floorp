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
#include "nsAreaFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsMenuPopupFrame.h"

#define NS_MENU_POPUP_LIST_INDEX   (NS_AREA_FRAME_ABSOLUTE_LIST_INDEX + 1)

//
// NS_NewMenuFrame
//
// Wrapper for creating a new menu popup container
//
nsresult
NS_NewMenuFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuFrame* it = new nsMenuFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}


//
// nsMenuFrame cntr
//
nsMenuFrame::nsMenuFrame()
:mMenuOpen(PR_FALSE),mOnMenuBar(PR_TRUE)
{

} // cntr

// The following methods are all overridden to ensure that the xpmenuchildren frame
// is placed in the appropriate list.
NS_IMETHODIMP
nsMenuFrame::FirstChild(nsIAtom*   aListName,
                        nsIFrame** aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsAreaFrame::FirstChild(aListName, aFirstChild);
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
        rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);
        return rv;
      }
      frame->GetNextSibling(&frame);
    }
    rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);
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
  return nsAreaFrame::GetAdditionalChildListName(aIndex, aListName);
}

NS_IMETHODIMP
nsMenuFrame::DeleteFrame(nsIPresContext& aPresContext)
{
   // Cleanup frames in popup child list
  mPopupFrames.DeleteFrames(aPresContext);
  return nsAreaFrame::DeleteFrame(aPresContext);
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
      nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
      popup->SyncViewWithFrame(mOnMenuBar);
      ToggleMenuState();
    }
  }
  return NS_OK;
}

void
nsMenuFrame::ToggleMenuState()
{
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
    
  if (mMenuOpen) {
    // Close the menu.
    child->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "false", PR_TRUE);
    mMenuOpen = PR_FALSE;
  }
  else {
    // Open the menu.
    child->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, "true", PR_TRUE);
    mMenuOpen = PR_TRUE;
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
  nsresult rv = nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  nsIFrame* frame = mPopupFrames.FirstChild();
    
  if (rv == NS_OK && frame) {
    // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
    nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame,
                                     availSize);
    kidReflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
    kidReflowState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
      
     // Reflow child
    nscoord w = aDesiredSize.width;
    nscoord h = aDesiredSize.height;
    nsresult rv = ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, aStatus);
 
     // Set the child's width and height to its desired size
    nsRect rect;
    frame->GetRect(rect);
    rect.width = aDesiredSize.width;
    rect.height = aDesiredSize.height;
    frame->SetRect(rect);

    // Don't let it affect our size.
    aDesiredSize.width = w;
    aDesiredSize.height = h;
  }
  return rv;
}
