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
 *   David Hyatt <hyatt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxFrame.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsBoxFrame.h"

nsIBoxLayout* nsSprocketLayout::gInstance = nsnull;

//#define DEBUG_GROW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8


nsresult
NS_NewSprocketLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  if (!nsSprocketLayout::gInstance) {
    nsSprocketLayout::gInstance = new nsSprocketLayout();
    NS_IF_ADDREF(nsSprocketLayout::gInstance);
  }
  // we have not instance variables so just return our static one.
  aNewLayout = nsSprocketLayout::gInstance;
  return NS_OK;
} 

/*static*/ void
nsSprocketLayout::Shutdown()
{
  NS_IF_RELEASE(gInstance);
}

nsSprocketLayout::nsSprocketLayout()
{
}

PRBool 
nsSprocketLayout::IsHorizontal(nsIBox* aBox)
{
   nsIFrame* frame = nsnull;
   aBox->GetFrame(&frame);
   return frame->GetStateBits() & NS_STATE_IS_HORIZONTAL;
}

void
nsSprocketLayout::GetFrameState(nsIBox* aBox, nsFrameState& aState)
{
   nsIFrame* frame = nsnull;
   aBox->GetFrame(&frame);
   aState = frame->GetStateBits();
}

static void
HandleBoxPack(nsIBox* aBox, const nsFrameState& aFrameState, nscoord& aX, nscoord& aY, 
              const nsRect& aOriginalRect, const nsRect& aClientRect)
{
  // In the normal direction we lay out our kids in the positive direction (e.g., |x| will get
  // bigger for a horizontal box, and |y| will get bigger for a vertical box).  In the reverse
  // direction, the opposite is true.  We'll be laying out each child at a smaller |x| or
  // |y|.
  if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL) {
    aX = aClientRect.x; // The normal direction. |x| and |y| increase as we move through our children.
    aY = aClientRect.y;
  }
  else {
    aX = aClientRect.x + aOriginalRect.width;  // The reverse direction. |x| and |y| decrease as we
    aY = aClientRect.y + aOriginalRect.height; // move through our children.
  }

  // Get our pack/alignment information.
  nsIBox::Halignment halign;
  nsIBox::Valignment valign;
  aBox->GetVAlign(valign);
  aBox->GetHAlign(halign);

  // The following code handles box PACKING.  Packing comes into play in the case where the computed size for 
  // all of our children (now stored in our client rect) is smaller than the size available for
  // the box (stored in |aOriginalRect|).  
  // 
  // Here we adjust our |x| and |y| variables accordingly so that we start at the beginning,
  // middle, or end of the box.
  //
  // XXXdwh JUSTIFY needs to be implemented!
  if (aFrameState & NS_STATE_IS_HORIZONTAL) {
    switch(halign) {
      case nsBoxFrame::hAlign_Left:
        break; // Nothing to do.  The default initialized us properly.

      case nsBoxFrame::hAlign_Center:
        if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL)
          aX += (aOriginalRect.width - aClientRect.width)/2;
        else 
          aX -= (aOriginalRect.width - aClientRect.width)/2;
        break;

      case nsBoxFrame::hAlign_Right:
        if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL)
          aX += (aOriginalRect.width - aClientRect.width);
        else
          aX -= (aOriginalRect.width - aClientRect.width);
        break; // Nothing to do for the reverse dir.  The default initialized us properly.
    }
  } else {
    switch(valign) {
      case nsBoxFrame::vAlign_Top:
      case nsBoxFrame::vAlign_BaseLine: // This value is technically impossible to specify for pack.
        break;  // Don't do anything.  We were initialized correctly.

      case nsBoxFrame::vAlign_Middle:
        if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL)
          aY += (aOriginalRect.height - aClientRect.height)/2;
        else
          aY -= (aOriginalRect.height - aClientRect.height)/2;
        break;

      case nsBoxFrame::vAlign_Bottom:
        if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL)
          aY += (aOriginalRect.height - aClientRect.height);
        else
          aY -= (aOriginalRect.height - aClientRect.height);
        break;
    }
  }
}

NS_IMETHODIMP
nsSprocketLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  // See if we are collapsed. If we are, then simply iterate over all our
  // children and give them a rect of 0 width and height.
  PRBool collapsed = PR_FALSE;
  aBox->IsCollapsed(aState, collapsed);
  if (collapsed) {
    nsIBox* child;
    aBox->GetChildBox(&child);
    while(child) 
    {
      nsContainerBox::LayoutChildAt(aState, child, nsRect(0,0,0,0));  
      child->GetNextBox(&child);
    }
    return NS_OK;
  }

  aState.PushStackMemory();

  // ----- figure out our size ----------
  nsRect contentRect;
  aBox->GetContentRect(contentRect);

  // -- make sure we remove our border and padding  ----
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  // |originalClientRect| represents the rect of the entire box (excluding borders
  // and padding).  We store it here because we're going to use |clientRect| to hold
  // the required size for all our kids.  As an example, consider an hbox with a
  // specified width of 300.  If the kids total only 150 pixels of width, then
  // we have 150 pixels left over.  |clientRect| is going to hold a width of 150 and
  // is going to be adjusted based off the value of the PACK property.  If flexible
  // objects are in the box, then the two rects will match.
  nsRect originalClientRect(clientRect);

  // The frame state contains cached knowledge about our box, such as our orientation
  // and direction.
  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);

  // Build a list of our children's desired sizes and computed sizes
  nsBoxSize*         boxSizes = nsnull;
  nsComputedBoxSize* computedBoxSizes = nsnull;

  nscoord maxAscent = 0;
  aBox->GetAscent(aState, maxAscent);

  nscoord min = 0;
  nscoord max = 0;
  PRInt32 flexes = 0;
  PopulateBoxSizes(aBox, aState, boxSizes, computedBoxSizes, min, max, flexes);
  
  // The |size| variable will hold the total size of children along the axis of
  // the box.  Continuing with the example begun in the comment above, size would
  // be 150 pixels.
  nscoord size = clientRect.width;
  if (!IsHorizontal(aBox))
    size = clientRect.height;
  ComputeChildSizes(aBox, aState, size, boxSizes, computedBoxSizes);

  // After the call to ComputeChildSizes, the |size| variable contains the
  // total required size of all the children.  We adjust our clientRect in the
  // appropriate dimension to match this size.  In our example, we now assign
  // 150 pixels into the clientRect.width.
  //
  // The variables |min| and |max| hold the minimum required size box must be 
  // in the OPPOSITE orientation, e.g., for a horizontal box, |min| is the minimum
  // height we require to enclose our children, and |max| is the maximum height
  // required to enclose our children.
  if (IsHorizontal(aBox)) {
    clientRect.width = size;
    if (clientRect.height < min)
      clientRect.height = min;

    if (frameState & NS_STATE_AUTO_STRETCH) {
      if (clientRect.height > max)
        clientRect.height = max;
    }
  } else {
    clientRect.height = size;
    if (clientRect.width < min)
      clientRect.width = min;

    if (frameState & NS_STATE_AUTO_STRETCH) {
      if (clientRect.width > max)
        clientRect.width = max;
    }
  }

  // With the sizes computed, now it's time to lay out our children.
  PRBool needsRedraw = PR_FALSE;
  PRBool finished;
  nscoord passes = 0;

  // We flow children at their preferred locations (along with the appropriate computed flex).  
  // After we flow a child, it is possible that the child will change its size.  If/when this happens,
  // we have to do another pass.  Typically only 2 passes are required, but the code is prepared to
  // do as many passes as are necessary to achieve equilibrium.
  nscoord x = 0;
  nscoord y = 0;
  nscoord origX = 0;
  nscoord origY = 0;

  // |childResized| lets us know if a child changed its size after we attempted to lay it out at
  // the specified size.  If this happens, we usually have to do another pass.
  PRBool childResized = PR_FALSE;

  // |passes| stores our number of passes.  If for any reason we end up doing more than, say, 10
  // passes, we assert to indicate that something is seriously screwed up.
  passes = 0;
  do 
  { 
#ifdef DEBUG_REFLOW
    if (passes > 0) {
      AddIndents();
      printf("ChildResized doing pass: %d\n", passes);
    }
#endif 

    // Always assume that we're done.  This will change if, for example, children don't stay
    // the same size after being flowed.
    finished = PR_TRUE;

    // Handle box packing.
    HandleBoxPack(aBox, frameState, x, y, originalClientRect, clientRect);

    // Now that packing is taken care of we set up a few additional
    // tracking variables.
    origX = x;
    origY = y;

    nscoord nextX = x;
    nscoord nextY = y;

    // Now we iterate over our box children and our box size lists in 
    // parallel.  For each child, we look at its sizes and figure out
    // where to place it.
    nsComputedBoxSize* childComputedBoxSize = computedBoxSizes;
    nsBoxSize* childBoxSize                 = boxSizes;

    nsIBox* child = nsnull;
    aBox->GetChildBox(&child);

    PRInt32 count = 0;
    while (child || (childBoxSize && childBoxSize->bogus))
    { 
      // If for some reason, our lists are not the same length, we guard
      // by bailing out of the loop.
      if (childBoxSize == nsnull) {
        NS_NOTREACHED("Lists not the same length.");
        break;
      }
        
      nscoord width = clientRect.width;
      nscoord height = clientRect.height;

      nsSize prefSize(0,0);
      nsSize minSize(0,0);
      nsSize maxSize(0,0);
      
      if (!childBoxSize->bogus) {
        // We have a valid box size entry.  This entry already contains information about our
        // sizes along the axis of the box (e.g., widths in a horizontal box).  If our default
        // ALIGN is not stretch, however, then we also need to know the child's size along the
        // opposite axis.
        if (!(frameState & NS_STATE_AUTO_STRETCH)) {
           child->GetPrefSize(aState, prefSize);
           child->GetMinSize(aState, minSize);
           child->GetMaxSize(aState, maxSize);
           nsBox::BoundsCheck(minSize, prefSize, maxSize);
       
           AddMargin(child, prefSize);
           width = PR_MIN(prefSize.width, originalClientRect.width);
           height = PR_MIN(prefSize.height, originalClientRect.height);
        }
      }

      // Obtain the computed size along the axis of the box for this child from the computedBoxSize entry.  
      // We store the result in |width| for horizontal boxes and |height| for vertical boxes.
      if (frameState & NS_STATE_IS_HORIZONTAL)
        width = childComputedBoxSize->size;
      else
        height = childComputedBoxSize->size;
      
      // Adjust our x/y for the left/right spacing.
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          x += (childBoxSize->left);
        else
          x -= (childBoxSize->right);
      } else {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          y += (childBoxSize->left);
        else
          y -= (childBoxSize->right);
      }

      nextX = x;
      nextY = y;

      // Now we build a child rect.
      nscoord rectX = x;
      nscoord rectY = y;
      if (!(frameState & NS_STATE_IS_DIRECTION_NORMAL)) {
        if (frameState & NS_STATE_IS_HORIZONTAL)
          rectX -= width;
        else
          rectY -= height;
      }

      // We now create an accurate child rect based off our computed size information.
      nsRect childRect(rectX, rectY, width, height);

      // Sanity check against our clientRect.  It is possible that a child specified
      // a size that is too large to fit.  If that happens, then we have to grow
      // our client rect.  Remember, clientRect is not the total rect of the enclosing
      // box.  It currently holds our perception of how big the children needed to
      // be.
      if (childRect.width > clientRect.width || childRect.height > clientRect.height) {
        if (childRect.width > clientRect.width)
          clientRect.width = childRect.width;

        if (childRect.height > clientRect.height)
          clientRect.height = childRect.height;
      }
    
      // |x|, |y|, |nextX|, and |nextY| are updated by this function call.  This call
      // also deals with box ALIGNMENT (when stretching is not turned on).
      ComputeChildsNextPosition(aBox, child, 
                                x, 
                                y, 
                                nextX, 
                                nextY, 
                                childRect, 
                                originalClientRect, 
                                childBoxSize->ascent,
                                maxAscent);

      // Now we update our nextX/Y along our axis and we update our x/y along the opposite
      // axis (since a non-stretching alignment could have caused an adjustment).
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          nextX += (childBoxSize->right);
        else
          nextX -= (childBoxSize->left);
        childRect.y = y;
      }
      else {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          nextY += (childBoxSize->right);
        else 
          nextY -= (childBoxSize->left);
        childRect.x = x;
      }
      
      // If we encounter a completely bogus box size, we just leave this child completely
      // alone and continue through the loop to the next child.
      if (childBoxSize->bogus) 
      {
        childComputedBoxSize = childComputedBoxSize->next;
        childBoxSize = childBoxSize->next;
        count++;
        x = nextX;
        y = nextY;
        continue;
      }

      nsMargin margin(0,0,0,0);

      PRBool layout = PR_TRUE;

      // Deflate the rect of our child by its margin.
      child->GetMargin(margin);
      childRect.Deflate(margin);
      if (childRect.width < 0)
        childRect.width = 0;
      if (childRect.height < 0)
        childRect.height = 0;

      // Now we're trying to figure out if we have to lay out this child, i.e., to call
      // the child's Layout method.
      if (passes > 0) {
        layout = PR_FALSE;
      } else {
        // Always perform layout if we are dirty or have dirty children
        PRBool dirty = PR_FALSE;           
        PRBool dirtyChildren = PR_FALSE;           
        child->IsDirty(dirty);
        child->HasDirtyChildren(dirtyChildren);
        if (!(dirty || dirtyChildren) && aState.LayoutReason() != nsBoxLayoutState::Initial)
          layout = PR_FALSE;
      }

      // We computed a childRect.  Now we want to set the bounds of the child to be that rect.
      // If our old rect is different, then we know our size changed and we cache that fact
      // in the |sizeChanged| variable.
      nsRect oldRect(0,0,0,0);
      PRBool sizeChanged = PR_FALSE;

      child->GetBounds(oldRect);
      child->SetBounds(aState, childRect);
      sizeChanged = (childRect.width != oldRect.width || childRect.height != oldRect.height);

      PRBool possibleRedraw = PR_FALSE;

      if (sizeChanged) {
        // Our size is different.  Sanity check against our maximum allowed size to ensure
        // we didn't exceed it.
        child->GetMaxSize(aState, maxSize);

        // make sure the size is in our max size.
        if (childRect.width > maxSize.width)
          childRect.width = maxSize.width;

        if (childRect.height > maxSize.height)
          childRect.height = maxSize.height;
           
        // set it again
        child->SetBounds(aState, childRect);

        // Since the child changed size, we know a redraw is probably going to be required.
        possibleRedraw = PR_TRUE;
      }

      // if something moved then we might need to redraw
      if (oldRect.x != childRect.x || oldRect.y != childRect.y)
          possibleRedraw = PR_TRUE;

      // If we already determined that layout was required or if our size has changed, then
      // we make sure to call layout on the child, since its children may need to be shifted
      // around as a result of the size change.
      if (layout || sizeChanged)
        child->Layout(aState);
      
      // If the child was a block or inline (e.g., HTML) it may have changed its rect *during* layout. 
      // We have to check for this.
      nsRect newChildRect;
      child->GetBounds(newChildRect);

      if (newChildRect != childRect) {
#ifdef DEBUG_GROW
        child->DumpBox(stdout);
        printf(" GREW from (%d,%d) -> (%d,%d)\n", childRect.width, childRect.height, newChildRect.width, newChildRect.height);
#endif
        newChildRect.Inflate(margin);
        childRect.Inflate(margin);

        // The child changed size during layout.  The ChildResized method handles this
        // scenario.
        ChildResized(aBox,
                     aState, 
                     child,
                     childBoxSize,
                     childComputedBoxSize,
                     boxSizes, 
                     computedBoxSizes, 
                     childRect,
                     newChildRect,
                     clientRect,
                     flexes,
                     finished);

        // We note that a child changed size, which means that another pass will be required.
        childResized = PR_TRUE;

        // Now that a child resized, it's entirely possible that OUR rect is too small.  Now we
        // ensure that |originalClientRect| is grown to accommodate the size of |clientRect|.
        if (clientRect.width > originalClientRect.width || clientRect.height > originalClientRect.height) {
          if (clientRect.width > originalClientRect.width)
            originalClientRect.width = clientRect.width;

          if (clientRect.height > originalClientRect.height)
            originalClientRect.height = clientRect.height;
        }

        // If the child resized then recompute its position.
        ComputeChildsNextPosition(aBox, child, 
                                  x, 
                                  y, 
                                  nextX, 
                                  nextY, 
                                  newChildRect, 
                                  originalClientRect, 
                                  childBoxSize->ascent,
                                  maxAscent);

        // Only update the variable in the opposite axis (since this is only here to deal with
        // a non-stretching ALIGNMENT)
        if (frameState & NS_STATE_IS_HORIZONTAL)
          newChildRect.y = y;
        else
          newChildRect.x = x;
         
        if (newChildRect.width >= margin.left + margin.right && newChildRect.height >= margin.top + margin.bottom) 
          newChildRect.Deflate(margin);

        if (childRect.width >= margin.left + margin.right && childRect.height >= margin.top + margin.bottom) 
          childRect.Deflate(margin);
            
        child->SetBounds(aState, newChildRect);

        // If we are the first box that changed size, then we don't need to do a second pass
        if (count == 0)
          finished = PR_TRUE;
      }

      // Now update our x/y finally.
      x = nextX;
      y = nextY;
     
      // If we get here and |possibleRedraw| is still set, then it's official.  We do need a repaint.
      if (possibleRedraw)
        needsRedraw = PR_TRUE;

      // Move to the next child.
      childComputedBoxSize = childComputedBoxSize->next;
      childBoxSize = childBoxSize->next;

      child->GetNextBox(&child);
      count++;
    }

    // Sanity-checking code to ensure we don't do an infinite # of passes.
    passes++;
    NS_ASSERTION(passes < 10, "A Box's child is constantly growing!!!!!");
    if (passes > 10)
      break;
  } while (PR_FALSE == finished);

  // Get rid of our size lists.
  while(boxSizes)
  {
    nsBoxSize* toDelete = boxSizes;
    boxSizes = boxSizes->next;
    delete toDelete;
  }

  while(computedBoxSizes)
  {
    nsComputedBoxSize* toDelete = computedBoxSizes;
    computedBoxSizes = computedBoxSizes->next;
    delete toDelete;
  }

  if (childResized) {
    // See if one of our children forced us to get bigger
    nsRect tmpClientRect(originalClientRect);
    nsMargin bp(0,0,0,0);
    aBox->GetBorderAndPadding(bp);
    tmpClientRect.Inflate(bp);
    aBox->GetInset(bp);
    tmpClientRect.Inflate(bp);

    if (tmpClientRect.width > contentRect.width || tmpClientRect.height > contentRect.height)
    {
      // if it did reset our bounds.
      nsRect bounds(0,0,0,0);
      aBox->GetBounds(bounds);
      if (tmpClientRect.width > contentRect.width)
        bounds.width = tmpClientRect.width;

      if (tmpClientRect.height > contentRect.height)
        bounds.height = tmpClientRect.height;

      aBox->SetBounds(aState, bounds);
    }
  }

  // Because our size grew, we now have to readjust because of box packing.  Repack
  // in order to update our x and y to the correct values.
  HandleBoxPack(aBox, frameState, x, y, originalClientRect, clientRect);

  // Compare against our original x and y and only worry about adjusting the children if
  // we really did have to change the positions because of packing (typically for 'center'
  // or 'end' pack values).
  if (x != origX || y != origY) {
    nsIBox* child = nsnull;
    // reposition all our children
    aBox->GetChildBox(&child);

    while (child) 
    {
      nsRect childRect;
      child->GetBounds(childRect);
      childRect.x += (x - origX);
      childRect.y += (y - origY);
      child->SetBounds(aState, childRect);
      child->GetNextBox(&child);
    }
  }

  // Now do our redraw.
  if (needsRedraw)
    aBox->Redraw(aState);

  aState.PopStackMemory();

  // That's it!  If you made it this far without having a nervous breakdown, 
  // congratulations!  Go get yourself a beer.
  return NS_OK;
}

void
nsSprocketLayout::PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
{
  // used for the equal size flag
  nscoord biggestPrefWidth = 0;
  nscoord biggestMinWidth = 0;
  nscoord smallestMaxWidth = NS_INTRINSICSIZE;

  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);

  //if (frameState & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");

  aMinSize = 0;
  aMaxSize = NS_INTRINSICSIZE;

  PRBool isHorizontal;

  if (IsHorizontal(aBox))
     isHorizontal = PR_TRUE;
  else
     isHorizontal = PR_FALSE;

  // this is a nice little optimization
  // it turns out that if we only have 1 flexable child
  // then it does not matter what its preferred size is
  // there is nothing to flex it relative. This is great
  // because we can avoid asking for a preferred size in this
  // case. Why is this good? Well you might have html inside it
  // and asking html for its preferred size is rather expensive.
  // so we can just optimize it out this way.

  // set flexes
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);

  aFlexes = 0;
  nsBoxSize* currentBox = nsnull;

#if 0
  nsBoxSize* start = aBoxSizes;
  
  while(child)
  {
    // ok if we started with a list move down the list
    // until we reach the end. Then start looking at childen.
    // This feature is used extensively for Grid.
    nscoord flex = 0;    

    if (!start) {
      if (!currentBox) {
        aBoxSizes      = new (aState) nsBoxSize();
        currentBox      = aBoxSizes;
      } else {
        currentBox->next      = new (aState) nsBoxSize();
        currentBox      = currentBox->next;
      }
    

      child->GetFlex(aState, flex);
      PRBool collapsed = PR_FALSE;    
      child->IsCollapsed(aState, collapsed);

      currentBox->flex = flex;
      currentBox->collapsed = collapsed;
    } else {
      flex = start->flex;
      start = start->next;
    }
    
    if (flex > 0) 
       aFlexes++;
   
    child->GetNextBox(&child);
  }
#endif

  // get pref, min, max
  aBox->GetChildBox(&child);
  currentBox = aBoxSizes;
  nsBoxSize* last = nsnull;

  while(child)
  {
    nsSize pref(0,0);
    nsSize min(0,0);
    nsSize max(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
    nscoord ascent = 0;

    PRBool collapsed = PR_FALSE;    
    child->IsCollapsed(aState, collapsed);

    if (!collapsed) {
    // only one flexible child? Cool we will just make its preferred size
    // 0 then and not even have to ask for it.
    //if (flexes != 1)  {

      child->GetPrefSize(aState, pref);
      child->GetMinSize(aState, min);
      child->GetMaxSize(aState, max);
      child->GetAscent(aState, ascent);
      nsMargin margin;
      child->GetMargin(margin);
      ascent += margin.top;
    //}

      nsBox::BoundsCheck(min, pref, max);

      AddMargin(child, pref);
      AddMargin(child, min);
      AddMargin(child, max);
    }

    if (!currentBox) {
      // create one.
      currentBox = new (aState) nsBoxSize();
      if (!aBoxSizes) {
        aBoxSizes = currentBox;
        last = aBoxSizes;
      } else {
        last->next = currentBox;
        last = currentBox;
      }

      nscoord minWidth;
      nscoord maxWidth;
      nscoord prefWidth;

      // get sizes from child
      if (isHorizontal) {
          minWidth  = min.width;
          maxWidth  = max.width;
          prefWidth = pref.width;
      } else {
          minWidth = min.height;
          maxWidth = max.height;
          prefWidth = pref.height;
      }

      nscoord flex = 0;
      child->GetFlex(aState, flex);

      // set them if you collapsed you are not flexible.
      if (collapsed)
         currentBox->flex = 0;
      else
         currentBox->flex = flex;

      // we we specified all our children are equal size;
      if (frameState & NS_STATE_EQUAL_SIZE) {

        if (prefWidth > biggestPrefWidth) 
          biggestPrefWidth = prefWidth;

        if (minWidth > biggestMinWidth) 
          biggestMinWidth = minWidth;

        if (maxWidth < smallestMaxWidth) 
          smallestMaxWidth = maxWidth;
      } else { // not we can set our children right now.
        currentBox->pref    = prefWidth;
        currentBox->min     = minWidth;
        currentBox->max     = maxWidth;
      }

      NS_ASSERTION(minWidth <= prefWidth && prefWidth <= maxWidth,"Bad min, pref, max widths!");

    }

    if (!isHorizontal) {
      if (min.width > aMinSize)
        aMinSize = min.width;

      if (max.width < aMaxSize)
        aMaxSize = max.width;

    } else {
      if (min.height > aMinSize)
        aMinSize = min.height;

      if (max.height < aMaxSize)
        aMaxSize = max.height;
    }

    currentBox->ascent  = ascent;
    currentBox->collapsed = collapsed;
    aFlexes += currentBox->flex;

    child->GetNextBox(&child);

    last = currentBox;
    currentBox = currentBox->next;

  }

  // we we specified all our children are equal size;
  if (frameState & NS_STATE_EQUAL_SIZE) {
    currentBox = aBoxSizes;

    while(currentBox)
    {
      if (!currentBox->collapsed) {
        currentBox->pref = biggestPrefWidth;
        currentBox->min = biggestMinWidth;
        currentBox->max = smallestMaxWidth;
      } else {
        currentBox->pref = 0;
        currentBox->min = 0;
        currentBox->max = 0;
      }
      currentBox = currentBox->next;
    }
  }

}

void
nsSprocketLayout::ComputeChildsNextPosition(nsIBox* aBox, 
                                      nsIBox* aChild, 
                                      nscoord& aCurX, 
                                      nscoord& aCurY, 
                                      nscoord& aNextX, 
                                      nscoord& aNextY, 
                                      const nsRect& aCurrentChildSize, 
                                      const nsRect& aBoxRect,
                                      nscoord childAscent,
                                      nscoord aMaxAscent)
{
  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);

  nsIBox::Halignment halign;
  nsIBox::Valignment valign;
  aBox->GetVAlign(valign);
  aBox->GetHAlign(halign);

  if (IsHorizontal(aBox)) {
    // Handle alignment of a horizontal box's children.
    if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
      aNextX = aCurX + aCurrentChildSize.width;
    else aNextX = aCurX - aCurrentChildSize.width;

    if (frameState & NS_STATE_AUTO_STRETCH)
      aCurY = aBoxRect.y;
    else {
      switch (valign) 
      {
         case nsBoxFrame::vAlign_BaseLine:
             aCurY = aBoxRect.y + (aMaxAscent - childAscent);
         break;

         case nsBoxFrame::vAlign_Top:
             aCurY = aBoxRect.y;
             break;
         case nsBoxFrame::vAlign_Middle:
             aCurY = aBoxRect.y + (aBoxRect.height/2 - aCurrentChildSize.height/2);
             break;
         case nsBoxFrame::vAlign_Bottom:
             aCurY = aBoxRect.y + aBoxRect.height - aCurrentChildSize.height;
             break;
      }
    }
  } else {
    // Handle alignment of a vertical box's children.
    if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
      aNextY = aCurY + aCurrentChildSize.height;
    else
      aNextY = aCurY - aCurrentChildSize.height;

    if (frameState & NS_STATE_AUTO_STRETCH)
      aCurX = aBoxRect.x;
    else {
      switch (halign) 
      {
         case nsBoxFrame::hAlign_Left:
             aCurX = aBoxRect.x;
             break;
         case nsBoxFrame::hAlign_Center:
             aCurX = aBoxRect.x + (aBoxRect.width/2 - aCurrentChildSize.width/2);
             break;
         case nsBoxFrame::hAlign_Right:
             aCurX = aBoxRect.x + aBoxRect.width - aCurrentChildSize.width;
             break;
      }
    }
  }
}

void
nsSprocketLayout::ChildResized(nsIBox* aBox,
                         nsBoxLayoutState& aState, 
                         nsIBox* aChild,
                         nsBoxSize* aChildBoxSize,
                         nsComputedBoxSize* aChildComputedSize,
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize* aComputedBoxSizes, 
                         const nsRect& aChildLayoutRect, 
                         nsRect& aChildActualRect, 
                         nsRect& aContainingRect,
                         PRInt32 aFlexes, 
                         PRBool& aFinished)
                         
{
      nsRect childCurrentRect(aChildLayoutRect);

      PRBool isHorizontal = IsHorizontal(aBox);
      nscoord childLayoutWidth  = GET_WIDTH(aChildLayoutRect,isHorizontal);
      nscoord& childActualWidth  = GET_WIDTH(aChildActualRect,isHorizontal);
      nscoord& containingWidth   = GET_WIDTH(aContainingRect,isHorizontal);   
      
      //nscoord childLayoutHeight = GET_HEIGHT(aChildLayoutRect,isHorizontal);
      nscoord& childActualHeight = GET_HEIGHT(aChildActualRect,isHorizontal);
      nscoord& containingHeight  = GET_HEIGHT(aContainingRect,isHorizontal);

      PRBool recompute = PR_FALSE;

      // if we are a horizontal box see if the child will fit inside us.
      if ( childActualHeight > containingHeight) {
            // if we are a horizontal box and the the child it bigger than our height

            // ok if the height changed then we need to reflow everyone but us at the new height
            // so we will set the changed index to be us. And signal that we need a new pass.

            nsSize max(0,0);
            aChild->GetMaxSize(aState, max);
            AddMargin(aChild, max);

            if (isHorizontal)
              childActualHeight = max.height < childActualHeight ? max.height : childActualHeight;
            else
              childActualHeight = max.width < childActualHeight ? max.width : childActualHeight;

            // only set if it changes
            if (childActualHeight > containingHeight) {
                 containingHeight = childActualHeight;

              // remember we do not need to clear the resized list because changing the height of a horizontal box
              // will not affect the width of any of its children because block flow left to right, top to bottom. Just trust me
              // on this one.
              aFinished = PR_FALSE;

              // only recompute if there are flexes.
              if (aFlexes > 0) {
                // relayout everything
                recompute = PR_TRUE;
                InvalidateComputedSizes(aComputedBoxSizes);
                nsComputedBoxSize* node = aComputedBoxSizes;

                while(node) {
                  node->resized = PR_FALSE;
                  node = node->next;
                }

              }              
            }
      } 
      
      if (childActualWidth > childLayoutWidth) {
            nsSize max(0,0);
            aChild->GetMaxSize(aState, max);
            AddMargin(aChild, max);

            // our width now becomes the new size

            if (isHorizontal)
              childActualWidth = max.width < childActualWidth ? max.width : childActualWidth;
            else
              childActualWidth = max.height < childActualWidth ? max.height : childActualWidth;

            if (childActualWidth > childLayoutWidth) {
               aChildComputedSize->size = childActualWidth;
               aChildBoxSize->min = childActualWidth;
               if (aChildBoxSize->pref < childActualWidth)
                  aChildBoxSize->pref = childActualWidth;

              // if we have flexible elements with us then reflex things. Otherwise we can skip doing it.
              if (aFlexes > 0) {
                InvalidateComputedSizes(aComputedBoxSizes);

                nsComputedBoxSize* node = aComputedBoxSizes;
                aChildComputedSize->resized = PR_TRUE;

                while(node) {
                  if (node->resized)
                      node->valid = PR_TRUE;
                
                  node = node->next;
                }

                recompute = PR_TRUE;
                aFinished = PR_FALSE;
              } else {
                containingWidth += aChildComputedSize->size - childLayoutWidth;
              }              
            }
      }

      if (recompute)
            ComputeChildSizes(aBox, aState, containingWidth, aBoxSizes, aComputedBoxSizes);

      if (childCurrentRect != aChildActualRect) {
        // the childRect includes the margin
        // make sure we remove it before setting 
        // the bounds.
        nsMargin margin(0,0,0,0);
        aChild->GetMargin(margin);
        nsRect rect(aChildActualRect);
        if (rect.width >= margin.left + margin.right && rect.height >= margin.top + margin.bottom) 
          rect.Deflate(margin);

        aChild->SetBounds(aState, rect);
        aChild->Layout(aState);
      }

}

void
nsSprocketLayout::InvalidateComputedSizes(nsComputedBoxSize* aComputedBoxSizes)
{
  while(aComputedBoxSizes) {
      aComputedBoxSizes->valid = PR_FALSE;
      aComputedBoxSizes = aComputedBoxSizes->next;
  }
}

void
nsSprocketLayout::ComputeChildSizes(nsIBox* aBox,
                           nsBoxLayoutState& aState, 
                           nscoord& aGivenSize, 
                           nsBoxSize* aBoxSizes, 
                           nsComputedBoxSize*& aComputedBoxSizes)
{  

  //nscoord onePixel = aState.PresContext()->IntScaledPixelsToTwips(1);

  PRInt32 sizeRemaining            = aGivenSize;
  PRInt32 spacerConstantsRemaining = 0;

   // ----- calculate the spacers constants and the size remaining -----

  if (!aComputedBoxSizes)
      aComputedBoxSizes = new (aState) nsComputedBoxSize();
  
  nsBoxSize*         boxSizes = aBoxSizes;
  nsComputedBoxSize* computedBoxSizes = aComputedBoxSizes;
  PRInt32 count = 0;
  PRInt32 validCount = 0;

  while (boxSizes) 
  {

    NS_ASSERTION((boxSizes->min <= boxSizes->pref && boxSizes->pref <= boxSizes->max),"bad pref, min, max size");

    
     // ignore collapsed children
  //  if (boxSizes->collapsed) 
  //  {
    //  computedBoxSizes->valid = PR_TRUE;
    //  computedBoxSizes->size = boxSizes->pref;
     // validCount++;
  //      boxSizes->flex = 0;
   // }// else {
    
      if (computedBoxSizes->valid) { 
        sizeRemaining -= computedBoxSizes->size;
        validCount++;
      } else {
          if (boxSizes->flex == 0)
          {
            computedBoxSizes->valid = PR_TRUE;
            computedBoxSizes->size = boxSizes->pref;
            validCount++;
          }

          spacerConstantsRemaining += boxSizes->flex;
          sizeRemaining -= boxSizes->pref;
      }

      sizeRemaining -= (boxSizes->left + boxSizes->right);

    //} 

    boxSizes = boxSizes->next;

    if (boxSizes && !computedBoxSizes->next) 
      computedBoxSizes->next = new (aState) nsComputedBoxSize();

    computedBoxSizes = computedBoxSizes->next;
    count++;
  }

  // everything accounted for?
  if (validCount < count)
  {
    // ----- Ok we are give a size to fit into so stretch or squeeze to fit
    // ----- Make sure we look at our min and max size
    PRBool limit = PR_TRUE;
    for (int pass=1; PR_TRUE == limit; pass++) 
    {
      limit = PR_FALSE;
      boxSizes = aBoxSizes;
      computedBoxSizes = aComputedBoxSizes;

      while (boxSizes) { 

        // ignore collapsed spacers

   //    if (!boxSizes->collapsed) {
      
          nscoord pref = 0;
          nscoord max  = NS_INTRINSICSIZE;
          nscoord min  = 0;
          nscoord flex = 0;

          pref = boxSizes->pref;
          min  = boxSizes->min;
          max  = boxSizes->max;
          flex = boxSizes->flex;

          // ----- look at our min and max limits make sure we aren't too small or too big -----
          if (!computedBoxSizes->valid) {
            PRInt32 newSize = pref + sizeRemaining*flex/spacerConstantsRemaining; //NSToCoordRound(float((sizeRemaining*flex)/spacerConstantsRemaining));

            if (newSize<=min) {
              computedBoxSizes->size = min;
              computedBoxSizes->valid = PR_TRUE;
              spacerConstantsRemaining -= flex;
              sizeRemaining += pref;
              sizeRemaining -= min;
              limit = PR_TRUE;
            } else if (newSize>=max) {
              computedBoxSizes->size = max;
              computedBoxSizes->valid = PR_TRUE;
              spacerConstantsRemaining -= flex;
              sizeRemaining += pref;
              sizeRemaining -= max;
              limit = PR_TRUE;
            }
          }
       // }
        boxSizes         = boxSizes->next;
        computedBoxSizes = computedBoxSizes->next;
      }
    }
  }          

  // ---- once we have removed and min and max issues just stretch us out in the remaining space
  // ---- or shrink us. Depends on the size remaining and the spacer constants
  aGivenSize = 0;
  boxSizes = aBoxSizes;
  computedBoxSizes = aComputedBoxSizes;

  while (boxSizes) { 

    // ignore collapsed spacers
  //  if (!(boxSizes && boxSizes->collapsed)) {
    
      nscoord pref = 0;
      nscoord flex = 0;
      pref = boxSizes->pref;
      flex = boxSizes->flex;

      if (!computedBoxSizes->valid) {
        computedBoxSizes->size = pref + flex*sizeRemaining/spacerConstantsRemaining; //NSToCoordFloor(float((flex*sizeRemaining)/spacerConstantsRemaining));
        computedBoxSizes->valid = PR_TRUE;
      }

      aGivenSize += (boxSizes->left + boxSizes->right);
      aGivenSize += computedBoxSizes->size;

   // }

    boxSizes         = boxSizes->next;
    computedBoxSizes = computedBoxSizes->next;
  }
}


NS_IMETHODIMP
nsSprocketLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
   PRBool isHorizontal = IsHorizontal(aBox);

   nscoord biggestPref = 0;

   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);

   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   PRBool isEqual = frameState & NS_STATE_EQUAL_SIZE;
   PRInt32 count = 0;
   
   while (child) 
   {  
      // ignore collapsed children
      PRBool isCollapsed = PR_FALSE;
      child->IsCollapsed(aState, isCollapsed);

      if (!isCollapsed)
      {
        nsSize pref(0,0);
        child->GetPrefSize(aState, pref);
        AddMargin(child, pref);

        if (isEqual) {
          if (isHorizontal)
          {
            if (pref.width > biggestPref)
              biggestPref = pref.width;
          } else {
            if (pref.height > biggestPref)
              biggestPref = pref.height;
          }
        }

        AddLargestSize(aSize, pref, isHorizontal);
        count++;
      }

      child->GetNextBox(&child);
   }

   if (isEqual) {
      if (isHorizontal)
         aSize.width = biggestPref*count;
      else
         aSize.height = biggestPref*count;
   }
    
   // now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox, aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsSprocketLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
   PRBool isHorizontal = IsHorizontal(aBox);

   nscoord biggestMin = 0;

   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   PRBool isEqual = frameState & NS_STATE_EQUAL_SIZE;
   PRInt32 count = 0;

   while (child) 
   {  
       // ignore collapsed children
      PRBool isCollapsed = PR_FALSE;
      aBox->IsCollapsed(aState, isCollapsed);

      if (!isCollapsed)
      {
        nsSize min(0,0);
        nsSize pref(0,0);
        nscoord flex = 0;

        child->GetMinSize(aState, min);        
        child->GetFlex(aState, flex);
        
        // if the child is not flexible then
        // its min size is its pref size.
        if (flex == 0)  {
            child->GetPrefSize(aState, pref);
            if (isHorizontal)
               min.width = pref.width;
            else
               min.height = pref.height;
        }

        if (isEqual) {
          if (isHorizontal)
          {
            if (min.width > biggestMin)
              biggestMin = min.width;
          } else {
            if (min.height > biggestMin)
              biggestMin = min.height;
          }
        }

        AddMargin(child, min);
        AddLargestSize(aSize, min, isHorizontal);
        count++;
      }

      child->GetNextBox(&child);
   }

   
   if (isEqual) {
      if (isHorizontal)
         aSize.width = biggestMin*count;
      else
         aSize.height = biggestMin*count;
   }

// now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox,aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsSprocketLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{

  PRBool isHorizontal = IsHorizontal(aBox);

   nscoord smallestMax = NS_INTRINSICSIZE;

   aSize.width = NS_INTRINSICSIZE;
   aSize.height = NS_INTRINSICSIZE;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box


   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   PRBool isEqual = frameState & NS_STATE_EQUAL_SIZE;
   PRInt32 count = 0;

   while (child) 
   {  
      // ignore collapsed children
      PRBool isCollapsed = PR_FALSE;
      aBox->IsCollapsed(aState, isCollapsed);

      if (!isCollapsed)
      {
        // if completely redefined don't even ask our child for its size.
        nsSize max(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
        child->GetMaxSize(aState, max);

        AddMargin(child, max);
        AddSmallestSize(aSize, max, isHorizontal);

        if (isEqual) {
          if (isHorizontal)
          {
            if (max.width < smallestMax)
              smallestMax = max.width;
          } else {
            if (max.height < smallestMax)
              smallestMax = max.height;
          }
        }
        count++;
      }

      child->GetNextBox(&child);  
   }

   if (isEqual) {
     if (isHorizontal) {
         if (smallestMax != NS_INTRINSICSIZE)
            aSize.width = smallestMax*count;
         else
            aSize.width = NS_INTRINSICSIZE;
     } else {
         if (smallestMax != NS_INTRINSICSIZE)
            aSize.height = smallestMax*count;
         else
            aSize.height = NS_INTRINSICSIZE;
     }
   }

   // now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox, aSize);

  return NS_OK;
}


NS_IMETHODIMP
nsSprocketLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aState, nscoord& aAscent)
{

  PRBool isHorizontal = IsHorizontal(aBox);

   aAscent = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box
   
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
      // ignore collapsed children
      //PRBool isCollapsed = PR_FALSE;
      //aBox->IsCollapsed(aState, isCollapsed);

      //if (!isCollapsed)
      //{
        // if completely redefined don't even ask our child for its size.
        nscoord ascent = 0;
        child->GetAscent(aState, ascent);

        nsMargin margin;
        child->GetMargin(margin);
        ascent += margin.top;

        if (isHorizontal)
        {
          if (ascent > aAscent)
            aAscent = ascent;
        } else {
          if (aAscent == 0)
            aAscent = ascent;
        }
      //}
      child->GetNextBox(&child);
      
   }

  return NS_OK;
}

NS_IMETHODIMP
nsSprocketLayout::GetFlex(nsIBox* aBox, nsBoxLayoutState& aState, nscoord& aFlex)
{
  return aBox->GetFlex(aState, aFlex);
}


NS_IMETHODIMP
nsSprocketLayout::IsCollapsed(nsIBox* aBox, nsBoxLayoutState& aState, PRBool& aIsCollapsed)
{
  return aBox->IsCollapsed(aState, aIsCollapsed);
}

void
nsSprocketLayout::SetLargestSize(nsSize& aSize1, const nsSize& aSize2, PRBool aIsHorizontal)
{
  if (aIsHorizontal)
  {
    if (aSize1.height < aSize2.height)
       aSize1.height = aSize2.height;
  } else {
    if (aSize1.width < aSize2.width)
       aSize1.width = aSize2.width;
  }
}

void
nsSprocketLayout::SetSmallestSize(nsSize& aSize1, const nsSize& aSize2, PRBool aIsHorizontal)
{
  if (aIsHorizontal)
  {
    if (aSize1.height > aSize2.height)
       aSize1.height = aSize2.height;
  } else {
    if (aSize1.width > aSize2.width)
       aSize1.width = aSize2.width;

  }
}

void
nsSprocketLayout::AddLargestSize(nsSize& aSize, const nsSize& aSizeToAdd, PRBool aIsHorizontal)
{
  if (aIsHorizontal)
    AddCoord(aSize.width, aSizeToAdd.width);
  else
    AddCoord(aSize.height, aSizeToAdd.height);

  SetLargestSize(aSize, aSizeToAdd, aIsHorizontal);
}

void
nsSprocketLayout::AddCoord(nscoord& aCoord, nscoord aCoordToAdd)
{
  if (aCoord != NS_INTRINSICSIZE) 
  {
    if (aCoordToAdd == NS_INTRINSICSIZE)
      aCoord = aCoordToAdd;
    else
      aCoord += aCoordToAdd;
  }
}
void
nsSprocketLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSizeToAdd, PRBool aIsHorizontal)
{
  if (aIsHorizontal)
    AddCoord(aSize.width, aSizeToAdd.width);
  else
    AddCoord(aSize.height, aSizeToAdd.height);
    
  SetSmallestSize(aSize, aSizeToAdd, aIsHorizontal);
}

PRBool
nsSprocketLayout::GetDefaultFlex(PRInt32& aFlex)
{
    aFlex = 0;
    return PR_TRUE;
}

void
nsBoxSize::Add(const nsSize& minSize, 
               const nsSize& prefSize,
               const nsSize& maxSize,
               nscoord aAscent,
               nscoord aFlex,
               PRBool aIsHorizontal)
{
  nscoord pref2;
  nscoord min2;
  nscoord max2;

  if (aIsHorizontal) {
    pref2 = prefSize.width;
    min2  = minSize.width;
    max2  = maxSize.width;
  } else {
    pref2 = prefSize.height;
    min2  = minSize.height;
    max2  = maxSize.height;
  }

  if (min2 > min)
    min = min2;

  if (pref2 > pref)
    pref = pref2;

  if (max2 < max)
    max = max2;

  flex = aFlex;

  if (!aIsHorizontal) {
    if (aAscent > ascent)
      ascent = aAscent;
    }
}

void
nsBoxSize::Add(const nsMargin& aMargin, PRBool aIsHorizontal)
{
  if (aIsHorizontal) {
    left  += aMargin.left;
    right += aMargin.right;
    pref -= (aMargin.left + aMargin.right);
  } else {
    left  += aMargin.top;
    right += aMargin.bottom;
    pref -= (aMargin.top + aMargin.bottom);
  }

  if (pref < min)
     min = pref;
}

nsComputedBoxSize::nsComputedBoxSize()
{
  Clear();
}

void
nsComputedBoxSize::Clear()
{
  resized = PR_FALSE;
  valid = PR_FALSE;
  size = 0;
  next = nsnull;
}

nsBoxSize::nsBoxSize()
{
  Clear();
}

void
nsBoxSize::Clear()
{
  pref = 0;
  min = 0;
  max = NS_INTRINSICSIZE;
  collapsed = PR_FALSE;
  ascent = 0;
  left = 0;
  right = 0;
  flex = 0;
  next = nsnull;
  bogus = PR_FALSE;
}


void* 
nsBoxSize::operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW
{
   void* mem = 0;
   aState.AllocateStackMemory(sz,&mem);
   return mem;
}


void 
nsBoxSize::operator delete(void* aPtr, size_t sz)
{
}


void* 
nsComputedBoxSize::operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW
{
  
   void* mem = 0;
   aState.AllocateStackMemory(sz,&mem);
   return mem;
}

void 
nsComputedBoxSize::operator delete(void* aPtr, size_t sz)
{
}
