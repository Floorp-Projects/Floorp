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

#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsTreeCellFrame.h"
#include "nsTreeFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIStyleSet.h"
#include "nsIViewManager.h"
#include "nsCSSRendering.h"
#include "nsXULAtoms.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"

static void ForceDrawFrame(nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, &view);
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
NS_NewTreeCellFrame (nsIFrame*& aNewFrame)
{
  nsTreeCellFrame* theFrame = new nsTreeCellFrame();
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = theFrame;
  return NS_OK;
  
} // NS_NewTreeCellFrame


// Constructor
nsTreeCellFrame::nsTreeCellFrame()
:nsTableCellFrame() { mAllowEvents = PR_FALSE; mIsHeader = PR_FALSE; }

// Destructor
nsTreeCellFrame::~nsTreeCellFrame()
{
}

NS_IMETHODIMP
nsTreeCellFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  // Determine if we're a column header or not.
  
  // Get row group frame
  nsIFrame* pRowGroupFrame = nsnull;
  aParent->GetParent(&pRowGroupFrame);
  if (pRowGroupFrame != nsnull)
  {
		// Get the display type of the row group frame and see if it's a header or body
		nsCOMPtr<nsIStyleContext> parentContext;
		pRowGroupFrame->GetStyleContext(getter_AddRefs(parentContext));
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
			pRowGroupFrame->GetParent((nsIFrame**) &mTreeFrame);
		}
  }

  return nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext,
                                aPrevInFlow);
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
  aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_ENTER)
    HandleMouseEnterEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_EXIT)
    HandleMouseExitEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_LEFT_DOUBLECLICK)
		HandleDoubleClickEvent(aPresContext, aEvent, aEventStatus);


  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseDownEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Nothing to do?
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
nsTreeCellFrame::HandleMouseEnterEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Nothing to do?
  }
  else
  {
    // Set our hover to true
    Hover(aPresContext, PR_TRUE);
  }
  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseExitEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  if (mIsHeader)
  {
	  // Nothing to do?
  }
  else
  {
    // Set our hover to false
    Hover(aPresContext, PR_FALSE);
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
	  nsCOMPtr<nsIContent> pTreeItemContent;
	  mContent->GetParent(*getter_AddRefs(pTreeItemContent));
    nsCOMPtr<nsIDOMElement> pTreeItem( do_QueryInterface(pTreeItemContent) );
    NS_ASSERTION(pTreeItem, "not a DOM element");
    if (! pTreeItem)
      return NS_ERROR_UNEXPECTED;
	  
	  // Take the tree item content and toggle the value of its open attribute.
	  nsAutoString attrValue;
    nsresult result = pTreeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue=="true");
    if (isExpanded)
	  {
		  // We're collapsing and need to remove frames from the flow.
		  pTreeItem->RemoveAttribute("open");
	  }
	  else
	  {
		  // We're expanding and need to add frames to the flow.
		  pTreeItem->SetAttribute("open", "true");
	  }
  }
  return NS_OK;
}


void nsTreeCellFrame::Select(nsIPresContext& aPresContext, PRBool isSelected, PRBool notifyForReflow)
{
	nsCOMPtr<nsIAtom> kSelectedCellAtom(dont_AddRef(NS_NewAtom("selectedcell")));
  nsCOMPtr<nsIAtom> kSelectedAtom(dont_AddRef(NS_NewAtom("selected")));

  nsIContent* pParentContent = nsnull;
  mContent->GetParent(pParentContent);

  if (isSelected)
	{
		// We're selecting the node.
		mContent->SetAttribute(kNameSpaceID_None, kSelectedCellAtom, "true", notifyForReflow);
    if(pParentContent) {
      pParentContent->SetAttribute(kNameSpaceID_None, kSelectedAtom, "true", notifyForReflow);
    }
	}
	else
	{
		// We're deselecting the node.
		mContent->UnsetAttribute(kNameSpaceID_None, kSelectedCellAtom, notifyForReflow);
    if(pParentContent) {
      pParentContent->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, notifyForReflow);
    }
	}

  NS_IF_RELEASE(pParentContent);
}

void nsTreeCellFrame::Hover(nsIPresContext& aPresContext, PRBool isHover, PRBool notifyForReflow)
{
	nsCOMPtr<nsIAtom> kHoverCellAtom(dont_AddRef(NS_NewAtom("hovercell")));
  nsCOMPtr<nsIAtom> kHoverAtom(dont_AddRef(NS_NewAtom("hover")));

  nsIContent* pParentContent = nsnull;
  mContent->GetParent(pParentContent);

  if (isHover)
	{
		// We're hovering over the node.
		mContent->SetAttribute(kNameSpaceID_None, kHoverCellAtom, "true", notifyForReflow);
    if(pParentContent) {
      pParentContent->SetAttribute(kNameSpaceID_None, kHoverAtom, "true", notifyForReflow);
    }
	}
	else
	{
		// We're deselecting the node.
		mContent->UnsetAttribute(kNameSpaceID_None, kHoverCellAtom, notifyForReflow);
    if(pParentContent) {
      pParentContent->UnsetAttribute(kNameSpaceID_None, kHoverAtom, notifyForReflow);
    }
  }

  NS_IF_RELEASE(pParentContent);
}

// XXX This method will go away.
NS_IMETHODIMP
nsTreeCellFrame::AttributeChanged(nsIPresContext* aPresContext,
                                  nsIContent* aChild,
                                  nsIAtom* aAttribute,
                                  PRInt32 aHint)
{
  // redraw
  nsRect frameRect;
  GetRect(frameRect);
  nsRect rect(0, 0, frameRect.width, frameRect.height);
  Invalidate(rect, PR_TRUE);

  return NS_OK;
}
