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


// Constants (styles used for selection rendering)
const char * kNormal        = "";
const char * kSelected      = "TREESELECTION";
const char * kSelectedFocus = "TREESELECTIONFOCUS";

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
  if (view != nsnull) {
    nsIViewManager * viewMgr;
    view->GetViewManager(viewMgr);
    if (viewMgr != nsnull) {
      viewMgr->UpdateView(view, rect, 0);
      NS_RELEASE(viewMgr);
    }
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
:nsTableCellFrame() { mAllowEvents = allowEvents; mIsHeader = PR_FALSE; mBeenReflowed = PR_FALSE; }

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
		nsIStyleContext* parentContext = nsnull;
		pRowGroupFrame->GetStyleContext(parentContext);
		if (parentContext)
		{
			const nsStyleDisplay* display = (const nsStyleDisplay*)
				parentContext->GetStyleData(eStyleStruct_Display);
			if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP)
			{
				mIsHeader = PR_TRUE;
			}
			else mIsHeader = PR_FALSE;
			NS_IF_RELEASE(parentContext);

			// Get the table frame.
			pRowGroupFrame->GetParent((nsIFrame*&)mTreeFrame);
		}
  }

  mNormalContext = aContext;
  
  nsresult rv = nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext);

  return rv;
}

NS_METHOD nsTreeCellFrame::Reflow(nsIPresContext& aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
	PRBool isSelected = PR_FALSE;
	if (!mBeenReflowed)
	{
		/*
		mBeenReflowed = PR_TRUE; // XXX Eventually move this to Init()
		nsIAtom * selectedPseudo = NS_NewAtom(":TREE-SELECTION");
		mSelectedContext = aPresContext.ResolvePseudoStyleContextFor(mContent, selectedPseudo, mStyleContext);
		NS_RELEASE(selectedPseudo); 

		// Find out if we're selected or not.
	    nsString attrValue;
	    nsIAtom* kSelectedAtom = NS_NewAtom("selected");
	    nsresult result = mContent->GetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, attrValue);
	    attrValue.ToLowerCase();
	    isSelected =  (result == NS_CONTENT_ATTR_NO_VALUE ||
							 (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
		if (isSelected)
			mTreeFrame->SetSelection(aPresContext, this);

	    NS_RELEASE(kSelectedAtom);
		*/
	}

	//if (isSelected)
	//	mTreeFrame->SetSelection(aPresContext, this);

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
	mTreeFrame->SetSelection(aPresContext, this);
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
	nsIContent* pRowContent;
	nsIContent* pTreeItemContent;
	mContent->GetParent(pRowContent);
	pRowContent->GetParent(pTreeItemContent);

	// Take the tree item content and toggle the value of its open attribute.
	nsString attrValue;
    nsIAtom* kOpenAtom = NS_NewAtom("open");
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
	nsIPresShell* pShell = aPresContext.GetShell();
	nsIStyleSet* pStyleSet = pShell->GetStyleSet();
	nsIDocument* pDocument = pShell->GetDocument();
	nsIContent* pRoot = pDocument->GetRootContent();
	
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

	NS_RELEASE(pShell);
	NS_RELEASE(pStyleSet);
	NS_RELEASE(pDocument);
	NS_IF_RELEASE(pRoot);

	NS_RELEASE(kOpenAtom);
    NS_RELEASE(pTreeItemContent);
	NS_RELEASE(pRowContent);
  }
  return NS_OK;
}


void
nsTreeCellFrame::Select(nsIPresContext& aPresContext, PRBool isSelected)
{
	nsIAtom * selectedPseudo = NS_NewAtom(":TREE-SELECTION");
	mSelectedContext = aPresContext.ResolvePseudoStyleContextFor(mContent, selectedPseudo, mStyleContext);
	NS_RELEASE(selectedPseudo); 

	if (isSelected)
		SetStyleContext(&aPresContext, mSelectedContext);
	else SetStyleContext(&aPresContext, mNormalContext);
		
	ForceDrawFrame(this);

    nsIAtom* kSelectedAtom = NS_NewAtom("selected");
    if (isSelected)
	{
		// We're selecting the node.
		mContent->SetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, "true", PR_FALSE);
		
	}
	else
	{
		// We're unselecting the node.
		mContent->UnsetAttribute(nsXULAtoms::nameSpaceID, kSelectedAtom, PR_FALSE);
	}

	NS_RELEASE(kSelectedAtom);
}
