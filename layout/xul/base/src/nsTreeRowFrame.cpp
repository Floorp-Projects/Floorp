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
#include "nsHTMLAtoms.h"
#include "nsTableColFrame.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULTreeElement.h"
#include "nsTreeTwistyListener.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresContext.h"

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
:nsTableRowFrame(), mIsHeader(PR_FALSE), mGeneration(0), mDraggingHeader(PR_FALSE), 
 mHitFrame(nsnull), mFlexingCol(nsnull), mHeaderPosition(0)
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
			else 
      {
        mIsHeader = PR_FALSE;
        
        // Determine the row's generation.
        nsTableFrame* tableFrame;
        nsTableFrame::GetTableFrame(aParent, tableFrame);
        nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;
        mGeneration = treeFrame->GetCurrentGeneration();
      }
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
        mDraggingHeader = PR_TRUE;
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
        mDraggingHeader = PR_FALSE;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsTreeRowFrame::Reflow(nsIPresContext&          aPresContext,
							         nsHTMLReflowMetrics&     aDesiredSize,
							         const nsHTMLReflowState& aReflowState,
							         nsReflowStatus&          aStatus)
{

  if (aReflowState.reason != eReflowReason_Incremental) {
    // Determine the row's generation.
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
    nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;
    if (treeFrame->UseGeneration()) {
      PRInt32 currGeneration = treeFrame->GetCurrentGeneration();
      if (currGeneration > mGeneration) {
        nsRect rect;
        GetRect(rect);
        aDesiredSize.width = rect.width;
        aDesiredSize.height = rect.height;
        aStatus = NS_FRAME_COMPLETE;
        return NS_OK;
      }
    }
  }

 /* static int i = 0;
  i++;
  printf("Full row reflow! Number %d\n", i);
*/
  return nsTableRowFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

nsresult
nsTreeRowFrame::HandleMouseUpEvent(nsIPresContext& aPresContext, 
									                  nsGUIEvent*     aEvent,
									                  nsEventStatus&  aEventStatus)
{
  if (DraggingHeader()) {
    HeaderDrag(PR_FALSE);
    
    nsTableFrame* tableFrame = nsnull;
    nsTableFrame::GetTableFrame(this, tableFrame);
    nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;
    treeFrame->SetUseGeneration(PR_FALSE); // Cached rows have to reflow.

    nsCellMap* cellMap = tableFrame->GetCellMap();
    PRInt32 numCols = cellMap->GetColCount();
    nsTableColFrame* colFrame = tableFrame->GetColFrame(0);
    nsCOMPtr<nsIContent> colElement;
    colFrame->GetContent(getter_AddRefs(colElement));
    if (colElement) {
      colElement->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, "40", PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeRowFrame::HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
    HandleMouseUpEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_MOVE && mDraggingHeader && mHitFrame)
    HandleHeaderDragEvent(aPresContext, aEvent, aEventStatus);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeRowFrame::GetFrameForPoint(const nsPoint& aPoint, // Overridden to capture events
                                 nsIFrame**     aFrame)
{
  nsresult rv = nsTableRowFrame::GetFrameForPoint(aPoint, aFrame);
  if (mDraggingHeader) {
    mHitFrame = *aFrame;
    *aFrame = this;
    nsRect rect;
    GetRect(rect);
    if (rect.Contains(aPoint)) {
      mHitFrame = nsnull;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsTreeRowFrame::GetCursor(nsIPresContext& aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  if (mDraggingHeader) {
    nsRect rect;
    GetRect(rect);
    if (rect.Contains(aPoint)) {
      aCursor = NS_STYLE_CURSOR_W_RESIZE;
    }
    else {
      aCursor = NS_STYLE_CURSOR_DEFAULT;
    }
  }
  else aCursor = NS_STYLE_CURSOR_DEFAULT;
  return NS_OK;
}

nsresult
nsTreeRowFrame::HandleHeaderDragEvent(nsIPresContext& aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus&  aEventStatus)
{
  // Grab our tree frame.
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  
  // Figure out how much we shifted the mouse.
  char ch[100];
  nsPoint point = ((nsMouseEvent*)aEvent)->point;
  PRInt32 delta = mHeaderPosition - point.x;
  mHeaderPosition = point.x;

  // The proportional columns to the right will gain or lose space
  // according to the percentages they currently consume.
  PRInt32 numCols = tableFrame->GetColCount();
  nscoord* propInfo = new PRInt32[numCols];
  nsCRT::memset(propInfo, 0, numCols*sizeof(nscoord));
  nscoord propTotal = 0;

  // For every column, determine its proportional width
  PRInt32 colX;
  for (colX = 0; colX < numCols; colX++) { 
    // Get column information
    nsTableColFrame* colFrame = tableFrame->GetColFrame(colX);
    if (colFrame == mFlexingCol)
      break;
  }

  colX++;
  for ( ; colX < numCols; colX++) {
    // Retrieve the current widths for these columns and compute
    // the total amount of space they occupy.
    nsTableColFrame* colFrame = tableFrame->GetColFrame(colX);

    // Get the column's width from the corresponding child cell in the header.
    nsCOMPtr<nsIContent> colContent;
    colFrame->GetContent(getter_AddRefs(colContent));

    nsCOMPtr<nsIAtom> fixedAtom = dont_AddRef(NS_NewAtom("fixed"));
    nsAutoString fixedVal;
    colContent->GetAttribute(kNameSpaceID_None, fixedAtom, fixedVal);
    if (fixedVal != "true") {
      PRInt32 colWidth = tableFrame->GetColumnWidth(colX);
      propInfo[colX] = colWidth;
      propTotal += propInfo[colX];
    }
  }

  // Iterate over the columns to the right of the flexing column, 
  // and give them a percentage of the delta based off their proportions.
  nsCOMPtr<nsIContent> colContent;
  PRInt32 remaining = delta;
  for (colX = 0; colX < numCols; colX++) {
    if (propInfo[colX] > 0) {
      nsTableColFrame* colFrame = tableFrame->GetColFrame(colX);
      float percentage = ((float)propInfo[colX])/((float)propTotal);
      PRInt32 mod = (PRInt32)(percentage * (float)delta);
      PRInt32 colWidth = propInfo[colX] + mod;
     
      sprintf(ch,"%d*", colWidth);
      nsAutoString propColWidth(ch);
      
      colFrame->GetContent(getter_AddRefs(colContent));
      if (colContent)
        colContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, propColWidth,
                                 PR_FALSE); // Suppress a notification.
      remaining -= mod;
    }
  }

  // Fix the spillover. We'll probably be off by a little.
  if (remaining != 0 && colContent) {
    sprintf(ch,"%d*", remaining);
    nsAutoString propColWidth(ch);
    colContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, propColWidth,
                             PR_FALSE); // Suppress a notification.
  }

  // Delete the propInfo array.
  delete [] propInfo;

  // Modify the flexing column by the delta.
  nsCOMPtr<nsIContent> flexContent;
  mFlexingCol->GetContent(getter_AddRefs(flexContent));
  if (flexContent) {
    sprintf(ch,"%d*", delta);
    nsAutoString propColWidth(ch);
    flexContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, propColWidth,
                              PR_TRUE); // NOW we send the notification that causes the reflow.
  }

  return NS_OK;
}