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
NS_NewTreeCellFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeCellFrame* theFrame = new nsTreeCellFrame();
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
nsTreeCellFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsTableCellFrame::Init(aPresContext, aContent, aParent, aContext,
                                aPrevInFlow);
  // Figure out if we allow events.
  nsString attrValue;
  nsresult result = aContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::treeallowevents, attrValue);
  attrValue.ToLowerCase();
  PRBool allowEvents =  (result == NS_CONTENT_ATTR_NO_VALUE ||
					    (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));
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
      nsresult rv = nsTableFrame::GetTableFrame(rowGroupFrame, tableFrame);
      if (NS_FAILED(rv) || (nsnull == tableFrame)) {
        return rv;
      }
      mTreeFrame = (nsTreeFrame*)tableFrame;
		}
  }

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
    nsresult result = nsTableCellFrame::GetFrameForPoint(aPoint, aFrame);
    nsCOMPtr<nsIContent> content;
    if (*aFrame) {
      (*aFrame)->GetContent(getter_AddRefs(content));
      if (content) {
        // This allows selective overriding for subcontent.
        nsAutoString value;
        content->GetAttribute(kNameSpaceID_None, nsXULAtoms::treeallowevents, value);
        if (value == "true")
          return result;
      }
    }
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
#ifdef XP_MAC
    else if (((nsMouseEvent *)aEvent)->isCommand)
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
  }
  else
  {
    // Set our hover to false
    Hover(aPresContext, PR_FALSE);
  }
  return NS_OK;
}

PRBool
nsTreeCellFrame::CanResize(nsPoint& aPoint) {
  nsRect rect;
  GetRect(rect);
  PRInt32 diff = (rect.x + rect.width) - aPoint.x;

  nsCOMPtr<nsIContent> parent;
  mContent->GetParent(*getter_AddRefs(parent));
  PRInt32 index;
  parent->IndexOf(mContent, index);
  PRInt32 count;
  parent->ChildCount(count);
  count--;

  if ((index > 0 && diff <= 60) || ((rect.width - diff) <= 60 && index < count)) {
    return PR_TRUE;
  }
  
  return PR_FALSE;
}

NS_IMETHODIMP
nsTreeCellFrame::GetCursor(nsIPresContext& aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  if (mIsHeader) {
    // Figure out if the point is over the resize stuff.
    nsRect rect;
    GetRect(rect);
    PRInt32 diff = (rect.x + rect.width) - aPoint.x;

    if (CanResize(aPoint)) {
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
    nsresult result = treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue=="true");
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
    nsresult result = treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue=="true");
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
    nsresult result = treeItem->GetAttribute("open", attrValue);
    attrValue.ToLowerCase();
    PRBool isExpanded = (attrValue=="true");
    if (isExpanded) {
		  // We're expanding and need to add frames to the flow.
		  treeItem->RemoveAttribute("open");
	  }
  }
}

nsresult
nsTreeCellFrame::HandleDoubleClickEvent(nsIPresContext& aPresContext, 
									    nsGUIEvent*     aEvent,
									    nsEventStatus&  aEventStatus)
{
  ToggleOpenClose();
  return NS_OK;
}

void nsTreeCellFrame::Hover(nsIPresContext& aPresContext, PRBool isHover, PRBool notifyForReflow)
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
nsTreeCellFrame::Destroy(nsIPresContext& aPresContext)
{
  return nsTableCellFrame::Destroy(aPresContext);
}
