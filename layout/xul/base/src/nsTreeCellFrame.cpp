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
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsCSSRendering.h"

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
NS_NewTreeCellFrame (nsIFrame*& aNewFrame)
{
  nsTreeCellFrame* theFrame = new nsTreeCellFrame;
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  aNewFrame = theFrame;
  return NS_OK;
  
} // NS_NewTreeCellFrame


// Constructor
nsTreeCellFrame::nsTreeCellFrame()
:nsTableCellFrame() { mIsHeader = PR_FALSE; }

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
		}
  }
  return nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext);
}

NS_IMETHODIMP
nsTreeCellFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
  return NS_OK;
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

    aEventStatus = nsEventStatus_eConsumeNoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);
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
		SetSelection(aPresContext);

  }
  return NS_OK;
}

void
nsTreeCellFrame::SetSelection(nsIPresContext& aPresContext)
{
  nsIAtom * selectedPseudo = NS_NewAtom(":TREE-SELECTION");
  mSelectedContext = aPresContext.ResolvePseudoStyleContextFor(mContent, selectedPseudo, mStyleContext);
	SetStyleContext(&aPresContext, mSelectedContext);
	
  ForceDrawFrame(this);

  NS_RELEASE(selectedPseudo); 
}
