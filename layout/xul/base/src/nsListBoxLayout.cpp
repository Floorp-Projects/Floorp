/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David W. Hyatt (hyatt@netscape.com) (Original Author)
 *   Joe Hewitt (hewitt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsListBoxLayout.h"

#include "nsListBoxBodyFrame.h"
#include "nsIFrame.h"
#include "nsBox.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsIReflowCallback.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"

nsListBoxLayout::nsListBoxLayout(nsIPresShell* aPresShell)
  : nsGridRowGroupLayout(aPresShell)
{
}

////////// nsIBoxLayout //////////////

NS_IMETHODIMP
nsListBoxLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsGridRowGroupLayout::GetPrefSize(aBox, aBoxLayoutState, aSize);

  nsListBoxBodyFrame* frame = NS_STATIC_CAST(nsListBoxBodyFrame*, aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightAppUnits();
    aSize.height = frame->GetRowCount() * rowheight;
    // Pad the height.
    nscoord y = frame->GetAvailableHeight();
    if (aSize.height > y && y > 0 && rowheight > 0) {
      nscoord m = (aSize.height-y)%rowheight;
      nscoord remainder = m == 0 ? 0 : rowheight - m;
      aSize.height += remainder;
    }
    if (nsContentUtils::HasNonEmptyAttr(frame->GetContent(), kNameSpaceID_None,
                                        nsGkAtoms::sizemode)) {
      nscoord width = frame->ComputeIntrinsicWidth(aBoxLayoutState);
      if (width > aSize.width)
        aSize.width = width;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsListBoxLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsGridRowGroupLayout::GetMinSize(aBox, aBoxLayoutState, aSize);

  nsListBoxBodyFrame* frame = NS_STATIC_CAST(nsListBoxBodyFrame*, aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightAppUnits();
    aSize.height = frame->GetRowCount() * rowheight;
    // Pad the height.
    nscoord y = frame->GetAvailableHeight();
    if (aSize.height > y && y > 0 && rowheight > 0) {
      nscoord m = (aSize.height-y)%rowheight;
      nscoord remainder = m == 0 ? 0 : rowheight - m;
      aSize.height += remainder;
    }
    if (nsContentUtils::HasNonEmptyAttr(frame->GetContent(), kNameSpaceID_None,
                                        nsGkAtoms::sizemode)) {
      nscoord width = frame->ComputeIntrinsicWidth(aBoxLayoutState);
      if (width > aSize.width)
        aSize.width = width;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsListBoxLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = nsGridRowGroupLayout::GetMaxSize(aBox, aBoxLayoutState, aSize);

  nsListBoxBodyFrame* frame = NS_STATIC_CAST(nsListBoxBodyFrame*, aBox);
  if (frame) {
    nscoord rowheight = frame->GetRowHeightAppUnits();
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
nsListBoxLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsListBoxBodyFrame* frame = NS_STATIC_CAST(nsListBoxBodyFrame*, aBox);

  // Always ensure an accurate scrollview position
  // This is an edge case that was caused by the row height
  // changing after a scroll had occurred.  (Bug #51084)
  PRInt32 index;
  frame->GetIndexOfFirstVisibleRow(&index);
  if (index > 0) {
    nscoord pos = frame->GetYPosition();
    PRInt32 rowHeight = frame->GetRowHeightAppUnits();
    if (pos != (rowHeight*index)) {
      frame->VerticalScroll(rowHeight*index);
      frame->Redraw(aState, nsnull, PR_FALSE);
    }
  }

  nsresult rv = LayoutInternal(aBox, aState);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


/////////// nsListBoxLayout /////////////////////////

/**
 * Called to layout our our children. Does no frame construction
 */
NS_IMETHODIMP
nsListBoxLayout::LayoutInternal(nsIBox* aBox, nsBoxLayoutState& aState)
{
  PRInt32 redrawStart = -1;

  // Get the start y position.
  nsListBoxBodyFrame* body = NS_STATIC_CAST(nsListBoxBodyFrame*, aBox);
  if (!body) {
    NS_ERROR("Frame encountered that isn't a listboxbody!\n");
    return NS_ERROR_FAILURE;
  }

  nsMargin margin;

  // Get our client rect.
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = body->GetAvailableHeight();
  nscoord yOffset = body->GetYPosition();
  
  if (availableHeight <= 0) {
    PRBool fixed = (body->GetFixedRowSize() != -1);
    if (fixed)
      availableHeight = 10;
    else
      return NS_OK;
  }

  // run through all our currently created children
  nsIBox* box = nsnull;
  body->GetChildBox(&box);

  // if the reason is resize or initial we must relayout.
  nscoord rowHeight = body->GetRowHeightAppUnits();

  while (box) {
    // If this box is dirty or if it has dirty children, we
    // call layout on it.
    nsRect childRect(box->GetRect());
    box->GetMargin(margin);
    
    // relayout if we must or we are dirty or some of our children are dirty
    //   or the client area is wider than us
    // XXXldb There should probably be a resize check here too!
    if ((box->GetStateBits() & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) || childRect.width < clientRect.width) {
      childRect.x = 0;
      childRect.y = yOffset;
      childRect.width = clientRect.width;
      
      nsSize size = box->GetPrefSize(aState);
      body->SetRowHeight(size.height);
      
      childRect.height = rowHeight;

      childRect.Deflate(margin);
      box->SetBounds(aState, childRect);
      box->Layout(aState);
    } else {
      // if the child did not need to be relayed out. Then its easy.
      // Place the child by just grabbing its rect and adjusting the y.
      PRInt32 newPos = yOffset+margin.top;

      // are we pushing down or pulling up any rows?
      // Then we may have to redraw everything below the moved 
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
  
  // We have enough available height left to add some more rows
  // Since we can't do this during layout, we post a callback
  // that will be processed after the reflow completes.
  body->PostReflowCallback();
    
  // if rows were pushed down or pulled up because some rows were added
  // before them then redraw everything under the inserted rows. The inserted
  // rows will automatically be redrawn because the were marked dirty on insertion.
  if (redrawStart > -1) {
    nsRect bounds(aBox->GetRect());
    nsRect tempRect(0,redrawStart,bounds.width, bounds.height - redrawStart);
    aBox->Redraw(aState, &tempRect);
  }

  return NS_OK;
}

// Creation Routines ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListBoxLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsListBoxLayout(aPresShell);

  return NS_OK;
} 
