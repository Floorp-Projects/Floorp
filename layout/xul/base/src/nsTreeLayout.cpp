/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"

// ------ nsTreeLayout ------


nsresult
NS_NewTreeLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsTreeLayout(aPresShell);

  return NS_OK;
} 

nsTreeLayout::nsTreeLayout(nsIPresShell* aPresShell):
#ifdef MOZ_GRID2
  nsGridRowGroupLayout(aPresShell)
#else
  nsTempleLayout(aPresShell)
#endif
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
#ifdef MOZ_GRID2
  nsresult rv = nsGridRowGroupLayout::GetPrefSize(aBox, aBoxLayoutState, aSize);
#else
  nsresult rv = nsTempleLayout::GetPrefSize(aBox, aBoxLayoutState, aSize);
#endif

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
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));
    nsAutoString sizeMode;
    content->GetAttr(kNameSpaceID_None, nsXULAtoms::sizemode, sizeMode);
    if (!sizeMode.IsEmpty()) {
      nscoord width = frame->ComputeIntrinsicWidth(aBoxLayoutState);
      if (width > aSize.width)
        aSize.width = width;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTreeLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
#ifdef MOZ_GRID2
  nsresult rv = nsGridRowGroupLayout::GetMinSize(aBox, aBoxLayoutState, aSize);
#else
  nsresult rv = nsTempleLayout::GetMinSize(aBox, aBoxLayoutState, aSize);
#endif

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
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));
    nsAutoString sizeMode;
    content->GetAttr(kNameSpaceID_None, nsXULAtoms::sizemode, sizeMode);
    if (!sizeMode.IsEmpty()) {
      nscoord width = frame->ComputeIntrinsicWidth(aBoxLayoutState);
      if (width > aSize.width)
        aSize.width = width;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsTreeLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
#ifdef MOZ_GRID2
  nsresult rv = nsGridRowGroupLayout::GetMaxSize(aBox, aBoxLayoutState, aSize);
#else
  nsresult rv = nsTempleLayout::GetMaxSize(aBox, aBoxLayoutState, aSize);
#endif

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

/**
 * Called to layout our our children. Does no frame construction
 */
NS_IMETHODIMP
nsTreeLayout::LayoutInternal(nsIBox* aBox, nsBoxLayoutState& aState)
{
  PRInt32 redrawStart = -1;

  // Get the start y position.
  nsXULTreeGroupFrame* group = GetGroupFrame(aBox);
  if (!group) {
    NS_ERROR("Frame encountered that isn't a tree row group!\n");
    return NS_ERROR_FAILURE;
  }

  nsXULTreeOuterGroupFrame* outer = group->GetOuterFrame();

  nsMargin margin;

  // Get our client rect.
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = group->GetAvailableHeight();
  nscoord yOffset = group->GetYPosition();
  
  if (availableHeight <= 0) {
    if (outer) {
      PRBool fixed = (outer->GetFixedRowSize() != -1);
      if (fixed)
        availableHeight = 10;
      else
        return NS_OK;
    }
    else 
      return NS_OK;
  }

  // run through all our currently created children
  nsIBox* box = nsnull;
  group->GetChildBox(&box);

  while (box) {  
    // If this box is dirty or if it has dirty children, we
    // call layout on it.
    PRBool dirty = PR_FALSE;           
    PRBool dirtyChildren = PR_FALSE;           
    box->IsDirty(dirty);
    box->HasDirtyChildren(dirtyChildren);
       
    PRBool isRow = PR_TRUE;
    nsXULTreeGroupFrame* childGroup = GetGroupFrame(box);
    if (childGroup) {
      // Set the available height.
      childGroup->SetAvailableHeight(availableHeight);
      isRow = PR_FALSE;
    }

    // if the reason is resize or initial we must relayout.
    PRBool relayout = (aState.GetLayoutReason() == nsBoxLayoutState::Resize || aState.GetLayoutReason() == nsBoxLayoutState::Initial);

    nsRect childRect;
    box->GetMargin(margin);
      
    // relayout if we must or we are dirty or some of our children are
    // dirty
    if (relayout || dirty || dirtyChildren) {      
      childRect.x = 0;
      childRect.y = yOffset;
      childRect.width = clientRect.width;
      
      // of we can determine the height of the child be getting the number
      // of row inside it and multiplying it by the height of a row.
      // remember this is on screen rows not total row. We create things
      // lazily and we only worry about what is on screen during layout.
      nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(box));
      PRInt32 rowCount = 0;
      slice->GetOnScreenRowCount(&rowCount);

      // if we are a row then we could potential change the row size. We
      // don't know the height of a row until layout so tell the outer group
      // now. If the row height is greater than the current. We may have to 
      // reflow everyone again!
      if (isRow) {
        nsSize size;
        box->GetPrefSize(aState, size);
        outer->SetRowHeight(size.height);
      }

      // of now get the row height and figure out our child's total height.
      nscoord rowHeight = outer->GetRowHeightTwips();

      childRect.height = rowHeight*rowCount;
      
      childRect.Deflate(margin);
      box->SetBounds(aState, childRect);
      box->Layout(aState);
    } else {
      // if the child did not need to be relayed out. Then its easy.
      // Place the child by just grabbing its rect and adjusting the y.
      box->GetBounds(childRect);
      PRInt32 newPos = yOffset+margin.top;

      // are we pushing down or pulling up any rows?
      // Then we may have to redraw everything below the the moved 
      // rows.
      if (redrawStart == -1 && childRect.y != newPos)
        redrawStart = newPos;

      childRect.y = newPos;
      box->SetBounds(aState, childRect);
    }

    // Ok now the available size gets smaller and we move the
    // starting position of the next child down some.
    nscoord size = childRect.height + margin.top + margin.bottom;

    yOffset += size;
    availableHeight -= size;
    
    box->GetNextBox(&box);
  }

  if (group && (group == outer)) {
    // We have enough available height left to add some more rows
    // Since we can't do this during layout, we post a callback
    // that will be processed after the reflow completes.
    outer->PostReflowCallback();
  }

  // if rows were pushed down or pulled up because some rows were added
  // before them then redraw everything under the inserted rows. The inserted
  // rows will automatically be redrawn because the were marked dirty on insertion.
  if (redrawStart > -1) {
    nsRect bounds;
    aBox->GetBounds(bounds);
    nsRect tempRect(0,redrawStart,bounds.width, bounds.height - redrawStart);
    aBox->Redraw(aState, &tempRect);
  }

  return NS_OK;
}

/**
 * This method creates or removes rows lazily. This is done after layout because 
 * It is illegal to add or remove frames during layout in the box system.
 */
NS_IMETHODIMP
nsTreeLayout::LazyRowCreator(nsBoxLayoutState& aState, nsXULTreeGroupFrame* aGroup)
{
  nsXULTreeOuterGroupFrame* outer = aGroup->GetOuterFrame();

  // Get our client rect.
  nsRect clientRect;
  aGroup->GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = aGroup->GetAvailableHeight();
  
  if (availableHeight <= 0) {
    if (outer) {
      PRBool fixed = (outer->GetFixedRowSize() != -1);
      if (fixed)
        availableHeight = 10;
      else
        return NS_OK;
    }
    else 
      return NS_OK;
  }
  
  // get the first tree box. If there isn't one create one.
  PRBool created = PR_FALSE;
  nsIBox* box = aGroup->GetFirstTreeBox(&created);
  while (box) {  

    // if its a group recursizely dive into it to build its rows.
    PRBool isRow = PR_TRUE;
    nsXULTreeGroupFrame* childGroup = GetGroupFrame(box);
    if (childGroup) {
      childGroup->SetAvailableHeight(availableHeight);
      LazyRowCreator(aState, childGroup);
      isRow = PR_FALSE;
    }

    nscoord rowHeight = outer->GetRowHeightTwips();

    // if the row height is 0 then fail. Wait until someone 
    // laid out and sets the row height.
    if (rowHeight == 0)
        return NS_OK;
     
    // figure out the child's height. Its the number of rows
    // the child contains * the row height. Remember this is
    // on screen rows not total rows.
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(box));

    PRInt32 rowCount = 0;
    slice->GetOnScreenRowCount(&rowCount);

    availableHeight -= rowHeight*rowCount;
    
    // should we continue? Is the enought height?
    if (!aGroup->ContinueReflow(availableHeight))
      break;

    // get the next tree box. Create one if needed.
    box = aGroup->GetNextTreeBox(box, &created);
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
    if (outer->IsBatching())
      return NS_OK;

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
  }
  else
    return LayoutInternal(aBox, aState);

  return NS_OK;
}

