/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsTreeCellFrame.h"
#include "nsTreeFrame.h"
#include "nsTreeRowFrame.h"
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
#include "nsTableColFrame.h"

//
// NS_NewTreeCellFrame
//
// Creates a new tree cell frame
//
nsresult
NS_NewTreeCellFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeCellFrame* theFrame = new (aPresShell) nsTreeCellFrame();
  if (theFrame == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = theFrame;
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
nsTreeCellFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext,
                                aPrevInFlow);
  // Figure out if we allow events.
  nsAutoString attrValue;
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::allowevents, attrValue);
  attrValue.ToLowerCase();
  PRBool allowEvents =  (result == NS_CONTENT_ATTR_NO_VALUE ||
					    (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue.Equals("true")));
  SetAllowEvents(allowEvents);

  // Determine if we're a column header or not.
  // Get row group frame
  nsIFrame* rowGroupFrame = nsnull;
  aParent->GetParent(&rowGroupFrame);
  if (rowGroupFrame != nsnull)
  {
		// Get the display type of the row group frame and see if it's a header or body
		nsCOMPtr<nsIStyleContext> parentContext;
		rowGroupFrame->GetStyleContext(getter_AddRefs(parentContext));
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
      nsTableFrame* tableFrame = nsnull;
      rv = nsTableFrame::GetTableFrame(rowGroupFrame, tableFrame);
      if (NS_FAILED(rv) || (nsnull == tableFrame)) {
        return rv;
      }
      mTreeFrame = (nsTreeFrame*)tableFrame;
		}
  }

  return rv;
}

NS_IMETHODIMP nsTreeCellFrame::AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint)
{
  nsresult rv = nsTableCellFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  return rv;
}


nsTableFrame* nsTreeCellFrame::GetTreeFrame()
{
	return mTreeFrame;
}

NS_IMETHODIMP nsTreeCellFrame::Reflow(nsIPresContext* aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
  //printf("Tree Cell Width: %d, Tree Cell Height: %d\n", aReflowState.mComputedWidth, aReflowState.mComputedHeight);

	nsresult rv = nsTableCellFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

	return rv;
}

NS_IMETHODIMP
nsTreeCellFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                  const nsPoint& aPoint, 
                                  nsFramePaintLayer aWhichLayer,
                                  nsIFrame**     aFrame)
{
  if (mAllowEvents)
  {
    return nsTableCellFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
  else
  {
    if (! ( mRect.Contains(aPoint) || ( mState & NS_FRAME_OUTSIDE_CHILDREN)) )
    {
      return NS_ERROR_FAILURE;
    }
    nsresult result = nsTableCellFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
    nsCOMPtr<nsIContent> content;
    if (result == NS_OK) {
      (*aFrame)->GetContent(getter_AddRefs(content));
      if (content) {
        // This allows selective overriding for subcontent.
        nsAutoString value;
        content->GetAttribute(kNameSpaceID_None, nsXULAtoms::allowevents, value);
        if (value.Equals("true"))
          return result;
      }
    }
    if (mRect.Contains(aPoint)) {
      const nsStyleDisplay* disp = (const nsStyleDisplay*)
        mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->IsVisible()) {
        *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
        return NS_OK;
      }
    }
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP 
nsTreeCellFrame::HandleEvent(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    if (((nsMouseEvent*)aEvent)->clickCount == 2)
		  HandleDoubleClickEvent(aPresContext, aEvent, aEventStatus);
    else 
		  HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);
  }
  else if (aEvent->message == NS_MOUSE_ENTER)
    HandleMouseEnterEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_EXIT)
    HandleMouseExitEvent(aPresContext, aEvent, aEventStatus);
  else if (aEvent->message == NS_MOUSE_LEFT_DOUBLECLICK)
		HandleDoubleClickEvent(aPresContext, aEvent, aEventStatus);
 
  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseDownEvent(nsIPresContext* aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus*  aEventStatus)
{
  if (mIsHeader) {
    nsTableColFrame* leftFlex = nsnull;
    nsPoint point = ((nsMouseEvent*)aEvent)->point;
    if (CanResize(point, &leftFlex))
    {
	    // Begin capturing events.
      nsIFrame* frame;
      GetParent(&frame);
      nsTreeRowFrame* treeRow = (nsTreeRowFrame*)frame;
      treeRow->HeaderDrag(aPresContext, PR_TRUE);

      // Inform the tree row of the flexing column
      treeRow->SetFlexingColumn(leftFlex);
      nsRect rect;
      GetRect(rect);
      treeRow->SetHeaderPosition(point.x);
    }
  }
  else
  {
    // Perform a selection
	  if (((nsMouseEvent *)aEvent)->isShift)
	    mTreeFrame->RangedSelection(aPresContext, this); // Applying a ranged selection.
#ifdef XP_MAC
    else if (((nsMouseEvent *)aEvent)->isMeta)
      mTreeFrame->ToggleSelection(aPresContext, this);
#else
	  else if (((nsMouseEvent *)aEvent)->isControl)
	    mTreeFrame->ToggleSelection(aPresContext, this); // Applying a toggle selection.
#endif
	  else mTreeFrame->SetSelection(aPresContext, this); // Doing a single selection only.
  }
  return NS_OK;
}

nsresult
nsTreeCellFrame::HandleMouseEnterEvent(nsIPresContext* aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus*  aEventStatus)
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
nsTreeCellFrame::HandleMouseExitEvent(nsIPresContext* aPresContext, 
									                    nsGUIEvent*     aEvent,
									                    nsEventStatus*  aEventStatus)
{
  if (mIsHeader)
  {
  }
  else
  {
    // Set our hover to false
    Hover(aPresContext, PR_FALSE);
  }
  return NS_OK;
}

PRBool
nsTreeCellFrame::CanResize(nsPoint& aPoint, nsTableColFrame** aResult) {
  nsRect rect;
  GetRect(rect);
  PRInt32 diff = (rect.x + rect.width) - aPoint.x;

  nsCOMPtr<nsIContent> parent;
  mContent->GetParent(*getter_AddRefs(parent));
  PRInt32 index;
  parent->IndexOf(mContent, index);
  PRInt32 count;
  parent->ChildCount(count);
  
  PRBool onLeftEdge = (index > 0 && (rect.width - diff) <= 90);
  PRBool onRightEdge = (index < (count-1) && diff <= 90);

  if (onLeftEdge || onRightEdge) {
    // We're over the right place.
    // Ensure that we have flexible columns to the left and to the right.
    nsTableFrame* tableFrame = nsnull;
    nsTableFrame::GetTableFrame(this, tableFrame);
    nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;  
    
    if (onLeftEdge)
      index--;

    return (treeFrame->ContainsFlexibleColumn(0, index, aResult) &&
            treeFrame->ContainsFlexibleColumn(index+1, count-1, nsnull)); 
  }
  
  return PR_FALSE;
}

NS_IMETHODIMP
nsTreeCellFrame::GetCursor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  if (mIsHeader) {
    // Figure out if the point is over the resize stuff.
    nsTableColFrame* dummy = nsnull;
    if (CanResize(aPoint, &dummy)) {
      aCursor = NS_STYLE_CURSOR_W_RESIZE;
    }
    else {
      aCursor = NS_STYLE_CURSOR_DEFAULT;
    }
  }
  else aCursor = NS_STYLE_CURSOR_DEFAULT;
  return NS_OK;
}

void
nsTreeCellFrame::ToggleOpenClose()
{
  if (!mIsHeader)
  {
    // Perform an expand/collapse
	  // Iterate up the chain to the row and then to the item.
	  nsCOMPtr<nsIContent> treeItemContent;
    nsCOMPtr<nsIContent> treeRowContent;
	  mContent->GetParent(*getter_AddRefs(treeRowContent));
    treeRowContent->GetParent(*getter_AddRefs(treeItemContent));

    nsCOMPtr<nsIDOMElement> treeItem( do_QueryInterface(treeItemContent) );
    NS_ASSERTION(treeItem, "not a DOM element");
    if (! treeItem)
      return;
	  
	  // Take the tree item content and toggle the value of its open attribute.
	  nsAutoString attrValue;
    treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue.Equals("true"));
    if (isExpanded)
	  {
		  // We're collapsing and need to remove frames from the flow.
		  treeItem->RemoveAttribute("open");
	  }
	  else
	  {
		  // We're expanding and need to add frames to the flow.
		  treeItem->SetAttribute("open", "true");
	  }
  }
}

void
nsTreeCellFrame::Open()
{
  if (!mIsHeader)
  {
    // Perform an expand/collapse
	  // Iterate up the chain to the row and then to the item.
	  nsCOMPtr<nsIContent> treeItemContent;
    nsCOMPtr<nsIContent> treeRowContent;
	  mContent->GetParent(*getter_AddRefs(treeRowContent));
    treeRowContent->GetParent(*getter_AddRefs(treeItemContent));

    nsCOMPtr<nsIDOMElement> treeItem( do_QueryInterface(treeItemContent) );
    NS_ASSERTION(treeItem, "not a DOM element");
    if (! treeItem)
      return;
	  
	  // Take the tree item content and toggle the value of its open attribute.
	  nsAutoString attrValue;
    treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue.Equals("true"));
    if (!isExpanded) {
		  // We're expanding and need to add frames to the flow.
		  treeItem->SetAttribute("open", "true");
	  }
  }
}

void
nsTreeCellFrame::Close()
{
  if (!mIsHeader)
  {
    // Perform an expand/collapse
	  // Iterate up the chain to the row and then to the item.
	  nsCOMPtr<nsIContent> treeItemContent;
    nsCOMPtr<nsIContent> treeRowContent;
	  mContent->GetParent(*getter_AddRefs(treeRowContent));
    treeRowContent->GetParent(*getter_AddRefs(treeItemContent));

    nsCOMPtr<nsIDOMElement> treeItem( do_QueryInterface(treeItemContent) );
    NS_ASSERTION(treeItem, "not a DOM element");
    if (! treeItem)
      return;
	  
	  // Take the tree item content and toggle the value of its open attribute.
	  nsAutoString attrValue;
    treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue.Equals("true"));
    if (isExpanded) {
		  // We're expanding and need to add frames to the flow.
		  treeItem->RemoveAttribute("open");
	  }
  }
}

nsresult
nsTreeCellFrame::HandleDoubleClickEvent(nsIPresContext* aPresContext, 
									    nsGUIEvent*     aEvent,
									    nsEventStatus*  aEventStatus)
{
  ToggleOpenClose();
  return NS_OK;
}

void nsTreeCellFrame::Hover(nsIPresContext* aPresContext, PRBool isHover, PRBool notifyForReflow)
{
	nsCOMPtr<nsIAtom> kHoverAtom(dont_AddRef(NS_NewAtom("hover")));

  nsCOMPtr<nsIContent> rowContent;
  nsCOMPtr<nsIContent> itemContent;
  mContent->GetParent(*getter_AddRefs(rowContent));
  rowContent->GetParent(*getter_AddRefs(itemContent));

  if (isHover)
	{
		// We're selecting the node.
		mContent->SetAttribute(kNameSpaceID_None, kHoverAtom, "true", notifyForReflow);
    rowContent->SetAttribute(kNameSpaceID_None, kHoverAtom, "true", notifyForReflow);
    itemContent->SetAttribute(kNameSpaceID_None, kHoverAtom, "true", notifyForReflow);
	}
	else
	{
		// We're deselecting the node.
    mContent->UnsetAttribute(kNameSpaceID_None, kHoverAtom, notifyForReflow);
    rowContent->UnsetAttribute(kNameSpaceID_None, kHoverAtom, notifyForReflow);
    itemContent->UnsetAttribute(kNameSpaceID_None, kHoverAtom, notifyForReflow);
	}
}

NS_IMETHODIMP
nsTreeCellFrame::Destroy(nsIPresContext* aPresContext)
{
  return nsTableCellFrame::Destroy(aPresContext);
}

PRBool nsTreeCellFrame::ShouldBuildCell(nsIFrame* aParentFrame, nsIContent* aCellContent)
{
  return PR_TRUE;
  
  /* Working on column show/hide
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(aParentFrame, tableFrame);
  nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;  
  
  // Get the index of the child content.
  nsCOMPtr<nsIContent> parent;
  aParentFrame->GetContent(getter_AddRefs(parent));
  PRInt32 index;
  parent->IndexOf(aCellContent, index);

  if (index == -1)
    return PR_TRUE;

  nsTableColFrame* colFrame = tableFrame->GetColFrame(index);
  if (!colFrame)
    return PR_FALSE;

  // See if the column's index matches.
  nsCOMPtr<nsIContent> colContent; 
  colFrame->GetContent(getter_AddRefs(colContent));
  if (!colContent)
    return PR_TRUE;

  // Get the index attribute.
  nsAutoString value;
  colContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::index, value);
  
  nsAutoString value2;
  value2.Append(index);
  if (value == "" || value == value2)
    return PR_TRUE;

  return PR_FALSE;
  */
}
