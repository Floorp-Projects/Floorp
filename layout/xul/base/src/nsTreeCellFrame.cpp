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

#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsTreeCellFrame.h"
#include "nsTreeFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIStyleSet.h"
#include "nsIViewManager.h"
#include "nsCSSRendering.h"
#include "nsXULAtoms.h"
#include "nsCOMPtr.h"


static void ForceDrawFrame(nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, 0);
  }

}

//
// NS_NewTreeCellFrame
//
// Creates a new tree cell frame
//
nsresult
NS_NewTreeCellFrame (nsIFrame*& aNewFrame, PRBool allowEvents)
{
  nsTreeCellFrame* theFrame = new nsTreeCellFrame(allowEvents);
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = theFrame;
  return NS_OK;
  
} // NS_NewTreeCellFrame


// Constructor
nsTreeCellFrame::nsTreeCellFrame(PRBool allowEvents)
:nsTableCellFrame() { mAllowEvents = allowEvents; mIsHeader = PR_FALSE; }

// Destructor
nsTreeCellFrame::~nsTreeCellFrame()
{
}

NS_IMETHODIMP
nsTreeCellFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext)
{
  // Determine if we're a column header or not.
  
  // Get row group frame
  nsIFrame* pRowGroupFrame = nsnull;
  aParent->GetParent(pRowGroupFrame);
  if (pRowGroupFrame != nsnull)
  {
		// Get the display type of the row group frame and see if it's a header or body
		nsCOMPtr<nsIStyleContext> parentContext;
		pRowGroupFrame->GetStyleContext(*getter_AddRefs(parentContext));
		if (parentContext)
		{
			const nsStyleDisplay* display = (const nsStyleDisplay*)
				parentContext->GetStyleData(eStyleStruct_Display);
			if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP)
			{
				mIsHeader = PR_TRUE;
			}
			else mIsHeader = PR_FALSE;

			// Get the table frame.
			pRowGroupFrame->GetParent((nsIFrame*&)mTreeFrame);
		}
  }

  nsresult rv = nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext);

  return rv;
}

nsTableFrame* nsTreeCellFrame::GetTreeFrame()
{
	return mTreeFrame;
}

NS_METHOD nsTreeCellFrame::Reflow(nsIPresContext& aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
	nsresult rv = nsTableCellFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

	return rv;
}

NS_IMETHODIMP
nsTreeCellFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  if (mAllowEvents)
  {
	  return nsTableCellFrame::GetFrameForPoint(aPoint, aFrame);
  }
  else
  {
    *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
    return NS_OK;
  }
}

NS_IMETHODIMP 
nsTreeCellFrame::HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {

    aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);
	else if (aEvent->message == NS_MOUSE_LEFT_DOUBLECLICK)
		HandleDoubleClickEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseDownEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Toggle the sort state
  }
  else
  {
    // Perform a selection
	if (((nsMouseEvent *)aEvent)->isShift)
	  mTreeFrame->RangedSelection(aPresContext, this); // Applying a ranged selection.
	else if (((nsMouseEvent *)aEvent)->isControl)
	  mTreeFrame->ToggleSelection(aPresContext, this); // Applying a toggle selection.
	else mTreeFrame->SetSelection(aPresContext, this); // Doing a single selection only.
  }
  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleDoubleClickEvent(nsIPresContext& aPresContext, 
									    nsGUIEvent*     aEvent,
									    nsEventStatus&  aEventStatus)
{
  if (!mIsHeader)
  {
    // Perform an expand/collapse
	// Iterate up the chain to the row and then to the item.
	nsCOMPtr<nsIContent> pRowContent;
	nsCOMPtr<nsIContent> pTreeItemContent;
	mContent->GetParent(*getter_AddRefs(pRowContent));
	pRowContent->GetParent(*getter_AddRefs(pTreeItemContent));

	// Take the tree item content and toggle the value of its open attribute.
	nsString attrValue;
    nsCOMPtr<nsIAtom> kOpenAtom ( dont_AddRef(NS_NewAtom("open")) );
    nsresult result = pTreeItemContent->GetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded =  (result == NS_CONTENT_ATTR_NO_VALUE ||
						 (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
    if (isExpanded)
	{
		// We're collapsing and need to remove frames from the flow.
		pTreeItemContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, PR_FALSE);
	}
	else
	{
		// We're expanding and need to add frames to the flow.
		pTreeItemContent->SetAttribute(nsXULAtoms::nameSpaceID, kOpenAtom, "true", PR_FALSE);
	}

	// Ok, try out the hack of doing frame reconstruction
	nsCOMPtr<nsIPresShell> pShell ( dont_AddRef(aPresContext.GetShell()) );
	nsCOMPtr<nsIStyleSet> pStyleSet ( dont_AddRef(pShell->GetStyleSet()) );
	nsCOMPtr<nsIDocument> pDocument ( dont_AddRef(pShell->GetDocument()) );
	nsCOMPtr<nsIContent> pRoot ( dont_AddRef(pDocument->GetRootContent()) );
	
	if (pRoot) {
		nsIFrame*   docElementFrame;
		nsIFrame*   parentFrame;
    
		// Get the frame that corresponds to the document element
		pShell->GetPrimaryFrameFor(pRoot, docElementFrame);
		if (nsnull != docElementFrame) {
		  docElementFrame->GetParent(parentFrame);
      
		  pShell->EnterReflowLock();
		  pStyleSet->ReconstructFrames(&aPresContext, pRoot,
									   parentFrame, docElementFrame);
		  pShell->ExitReflowLock();
		}
    }

  }
  return NS_OK;
}


void
nsTreeCellFrame::Select(nsIPresContext& aPresContext, PRBool isSelected, PRBool notifyForReflow)
{
	nsCOMPtr<nsIAtom> kSelectedAtom ( dont_AddRef(NS_NewAtom("selected")) );
    if (isSelected)
	{
		// We're selecting the node.
		mContent->SetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, "true", notifyForReflow);
		
	}
	else
	{
		// We're unselecting the node.
		mContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, notifyForReflow);
	}

}
