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

// Original Author:
// David W Hyatt (hyatt@netscape.com)
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsTreeLayout.h"
#include "nsBoxLayoutState.h"
#include "nsIBox.h"
#include "nsIScrollableFrame.h"
#include "nsBox.h"

// ------ nsTreeLayout ------


nsresult
NS_NewTreeLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsTreeLayout(aPresShell);

  return NS_OK;
} 

nsTreeLayout::nsTreeLayout(nsIPresShell* aPresShell):nsTempleLayout(aPresShell)
{
}

nsXULTreeOuterGroupFrame* nsTreeLayout::GetOuterFrame(nsIBox* aBox)
{
  nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(aBox));
  if (slice) {
    PRBool outer;
    slice->IsOutermostFrame(&outer);
    if (outer)
      return (nsXULTreeOuterGroupFrame*) aBox;
  }
  return nsnull;
}

nsXULTreeGroupFrame* nsTreeLayout::GetGroupFrame(nsIBox* aBox)
{
  nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(aBox));
  if (slice) {
    PRBool group;
    slice->IsGroupFrame(&group);
    if (group)
      return (nsXULTreeGroupFrame*) aBox;
  }
  return nsnull;
}

nsXULTreeSliceFrame* nsTreeLayout::GetRowFrame(nsIBox* aBox)
{
  nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(aBox));
  if (slice) {
    PRBool row;
    slice->IsRowFrame(&row);
    if (row)
      return (nsXULTreeSliceFrame*) aBox;
  }
  return nsnull;
}

NS_IMETHODIMP
nsTreeLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsTempleLayout::GetPrefSize(aBox, aBoxLayoutState, aSize);
  nsXULTreeOuterGroupFrame* frame = GetOuterFrame(aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightTwips();
    aSize.height = frame->GetRowCount() * rowheight;
    // Pad the height.
    nscoord y = frame->GetAvailableHeight();
    if (aSize.height > y && y > 0 && rowheight > 0) {
      nscoord m = (aSize.height-y)%rowheight;
      nscoord remainder = m == 0 ? 0 : rowheight - m;
      aSize.height += remainder;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTreeLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsTempleLayout::GetMinSize(aBox, aBoxLayoutState, aSize);
  nsXULTreeOuterGroupFrame* frame = GetOuterFrame(aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightTwips();
    aSize.height = frame->GetRowCount() * rowheight;
    // Pad the height.
    nscoord y = frame->GetAvailableHeight();
    if (aSize.height > y && y > 0 && rowheight > 0) {
      nscoord m = (aSize.height-y)%rowheight;
      nscoord remainder = m == 0 ? 0 : rowheight - m;
      aSize.height += remainder;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTreeLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsTempleLayout::GetMaxSize(aBox, aBoxLayoutState, aSize);
  nsXULTreeOuterGroupFrame* frame = GetOuterFrame(aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightTwips();
    aSize.height = frame->GetRowCount() * rowheight;
    // Pad the height.
    nscoord y = frame->GetAvailableHeight();
    if (aSize.height > y && y > 0 && rowheight > 0) {
      nscoord m = (aSize.height-y)%rowheight;
      nscoord remainder = m == 0 ? 0 : rowheight - m;
      aSize.height += remainder;
    }
  }
  return rv;
}


NS_IMETHODIMP
nsTreeLayout::LayoutInternal(nsIBox* aBox, nsBoxLayoutState& aState)
{
  // Get the start y position.
  nsXULTreeGroupFrame* frame = GetGroupFrame(aBox);
  if (!frame) {
    NS_ERROR("Frame encountered that isn't a tree row group!\n");
    return NS_ERROR_FAILURE;
  }

  nsMargin margin;

  // Get our client rect.
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = frame->GetAvailableHeight();
  nscoord yOffset = frame->GetYPosition();
  
  if (availableHeight <= 0)
    return NS_OK;

  // Walk our frames, building them dynamically as needed.
  nsIBox* box = frame->GetFirstTreeBox();
  while (box) {  
    // If this box is dirty or if it has dirty children, we
    // call layout on it.
    PRBool dirty = PR_FALSE;           
    PRBool dirtyChildren = PR_FALSE;           
    box->IsDirty(dirty);
    box->HasDirtyChildren(dirtyChildren);
    
    nsIFrame* childFrame;
    box->GetFrame(&childFrame);
    nsFrameState state;
    childFrame->GetFrameState(&state);
    
    PRBool isRow = PR_TRUE;
    nsXULTreeGroupFrame* childGroup = GetGroupFrame(box);
    if (childGroup) {
      // Set the available height.
      childGroup->SetAvailableHeight(availableHeight);
      isRow = PR_FALSE;
    }

    PRBool relayoutAll = (frame->GetOuterFrame()->GetTreeLayoutState() == eTreeLayoutDirtyAll);

    nsRect childRect;
    PRBool sizeChanged = PR_FALSE;
    if (isRow) {
      nsSize size;
      box->GetPrefSize(aState, size);
      if (size.width != clientRect.width)
        sizeChanged = PR_TRUE;
    }

    if (relayoutAll || childGroup || sizeChanged || dirty || dirtyChildren || aState.GetLayoutReason() == nsBoxLayoutState::Initial) {      
      nsRect childRect;
      childRect.x = 0;
      childRect.y = yOffset;
      childRect.width = clientRect.width;
      
      if (isRow)
        childRect.height = frame->GetOuterFrame()->GetRowHeightTwips();

      box->GetMargin(margin);
      childRect.Deflate(margin);
      box->SetBounds(aState, childRect);
      box->Layout(aState);

      nsSize size;
      if (!isRow) {
        // We are a row group that might have dynamically
        // constructed new rows.  We need to clear out
        // and recompute our pref size and then adjust our
        // rect accordingly.
        box->NeedsRecalc();
        box->GetPrefSize(aState, size);
        childRect.height = size.height;
        box->SetBounds(aState, childRect);
      }
      else {// Check to see if the row height of the tree has changed.
        box->GetPrefSize(aState, size);
        frame->GetOuterFrame()->SetRowHeight(size.height);
      }
    }

    // Place the child by just grabbing its rect and adjusting the x,y.
    box->GetContentRect(childRect);
    childRect.x = 0;
    childRect.y = yOffset;
    yOffset += childRect.height;
    availableHeight -= childRect.height;
    box->GetMargin(margin);
    childRect.Deflate(margin);
    childRect.width = childRect.width < 0 ? 0 : childRect.width;
    childRect.height = childRect.height < 0 ? 0 : childRect.height;
    
    box->SetBounds(aState, childRect);

    if ((frame->GetOuterFrame()->GetTreeLayoutState() == eTreeLayoutAbort) || 
        (!frame->ContinueReflow(availableHeight)))
      break;

    box = frame->GetNextTreeBox(box);
  }

  return NS_OK;
}



NS_IMETHODIMP
nsTreeLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsXULTreeGroupFrame* frame = GetGroupFrame(aBox);
  PRBool isOuterGroup;
  frame->IsOutermostFrame(&isOuterGroup);

  if (isOuterGroup) {
    nsXULTreeOuterGroupFrame* outer = (nsXULTreeOuterGroupFrame*) frame;
    nsTreeLayoutState state = outer->GetTreeLayoutState();

    // Always ensure an accurate scrollview position
    // This is an edge case that was caused by the row height
    // changing after a scroll had occurred.  (Bug #51084)
    PRInt32 index;
    outer->GetIndexOfFirstVisibleRow(&index);
    if (index > 0) {
      nscoord pos = outer->GetYPosition();
      PRInt32 rowHeight = outer->GetRowHeightTwips();
      if (pos != (rowHeight*index)) {
        outer->VerticalScroll(rowHeight*index);
        outer->Redraw(aState, nsnull, PR_FALSE);
      }
    }

    nsresult rv = LayoutInternal(aBox, aState);
    if (NS_FAILED(rv)) return rv;
    state = outer->GetTreeLayoutState();
    if (state == eTreeLayoutDirtyAll)
      outer->SetTreeLayoutState(eTreeLayoutNormal);
    else if (state == eTreeLayoutAbort)
      outer->SetTreeLayoutState(eTreeLayoutDirtyAll);
    state = outer->GetTreeLayoutState();
  }
  else
    return LayoutInternal(aBox, aState);

  return NS_OK;
}

