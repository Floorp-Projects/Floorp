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

#include "nsCOMPtr.h"
#include "nsTreeFrame.h"
#include "nsTreeRowFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIDOMXULTreeElement.h"
#include "nsINameSpaceManager.h"
#include "nsTreeRowGroupFrame.h"
#include "nsXULAtoms.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULTreeElement.h"
#include "nsTreeTwistyListener.h"
#include "nsIViewManager.h"
#include "nsIView.h"

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeRowFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeRowFrame* it = new nsTreeRowFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeRowFrame


// Constructor
nsTreeRowFrame::nsTreeRowFrame()
:nsTableRowFrame(), mIsHeader(PR_FALSE)
{ }

// Destructor
nsTreeRowFrame::~nsTreeRowFrame()
{
}

NS_METHOD nsTreeRowFrame::IR_TargetIsChild(nsIPresContext&      aPresContext,
                                            nsHTMLReflowMetrics& aDesiredSize,
                                            RowReflowState&      aReflowState,
                                            nsReflowStatus&      aStatus,
                                            nsIFrame *           aNextFrame)
{
  nsRect rect;
  GetRect(rect);
  nsresult rv = nsTableRowFrame::IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, aNextFrame);
  if (rv == NS_OK) {
    // Find out if our height changed.  With the tree widget, changing the height of a row is a
    // big deal, since it may force us to dynamically isntantiate newly exposed frames.
    if (rect.height != aDesiredSize.height) {
      // Retrieve the table frame and invalidate the cell map.
      nsTableFrame* tableFrame = nsnull;
      nsTableFrame::GetTableFrame(this, tableFrame);
  
      tableFrame->InvalidateCellMap(); 
    }
  }
  return rv;
}


NS_IMETHODIMP
nsTreeRowFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsTableRowFrame::Init(aPresContext, aContent, aParent, aContext,
                                aPrevInFlow);
  
  // Determine if we're a column header or not.
  // Get row group frame
  if (aParent != nsnull)
  {
		// Get the display type of the row group frame and see if it's a header or body
		nsCOMPtr<nsIStyleContext> parentContext;
		aParent->GetStyleContext(getter_AddRefs(parentContext));
		if (parentContext)
		{
			const nsStyleDisplay* display = (const nsStyleDisplay*)
				parentContext->GetStyleData(eStyleStruct_Display);
			if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_HEADER_GROUP)
			{
				mIsHeader = PR_TRUE;

        // headers get their own views, so that they can capture events
        CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
        nsIView* view;
        GetView(&view);
        view->SetContentTransparency(PR_TRUE);
			}
			else mIsHeader = PR_FALSE;
		}
  }

  return rv;
}

NS_IMETHODIMP
nsTreeRowFrame::HeaderDrag(PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
  }

  return NS_OK;
}

PRBool
nsTreeRowFrame::DraggingHeader()
{
    // get its view
  nsIView* view = nsnull;
  GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
      nsIView* grabbingView;
      viewMan->GetMouseEventGrabber(grabbingView);
      if (grabbingView == view)
        return PR_TRUE;
    }
  }

  return PR_FALSE;
}
