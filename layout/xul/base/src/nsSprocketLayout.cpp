/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsContainerFrame.h"
#include "nsBoxFrame.h"
#include "StackArena.h"

nsBoxLayout* nsSprocketLayout::gInstance = nullptr;

//#define DEBUG_GROW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8


nsresult
NS_NewSprocketLayout( nsIPresShell* aPresShell, nsCOMPtr<nsBoxLayout>& aNewLayout)
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

bool 
nsSprocketLayout::IsHorizontal(nsIFrame* aBox)
{
   return (aBox->GetStateBits() & NS_STATE_IS_HORIZONTAL) != 0;
}

void
nsSprocketLayout::GetFrameState(nsIFrame* aBox, nsFrameState& aState)
{
   aState = aBox->GetStateBits();
}

static PRUint8
GetFrameDirection(nsIFrame* aBox)
{
   return aBox->GetStyleVisibility()->mDirection;
}

static void
HandleBoxPack(nsIFrame* aBox, const nsFrameState& aFrameState, nscoord& aX, nscoord& aY, 
              const nsRect& aOriginalRect, const nsRect& aClientRect)
{
  // In the normal direction we lay out our kids in the positive direction (e.g., |x| will get
  // bigger for a horizontal box, and |y| will get bigger for a vertical box).  In the reverse
  // direction, the opposite is true.  We'll be laying out each child at a smaller |x| or
  // |y|.
  PRUint8 frameDirection = GetFrameDirection(aBox);

  if (aFrameState & NS_STATE_IS_HORIZONTAL) {
    if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL) {
      // The normal direction. |x| increases as we move through our children.
      aX = aClientRect.x;
    }
    else {
      // The reverse direction. |x| decreases as we move through our children.
      aX = aClientRect.x + aOriginalRect.width;
    }
    // |y| is always in the normal direction in horizontal boxes
    aY = aClientRect.y;    
  }
  else {
    // take direction property into account for |x| in vertical boxes
    if (frameDirection == NS_STYLE_DIRECTION_LTR) {
      // The normal direction. |x| increases as we move through our children.
      aX = aClientRect.x;
    }
    else {
      // The reverse direction. |x| decreases as we move through our children.
      aX = aClientRect.x + aOriginalRect.width;
    }
    if (aFrameState & NS_STATE_IS_DIRECTION_NORMAL) {
      // The normal direction. |y| increases as we move through our children.
      aY = aClientRect.y;
    }
    else {
      // The reverse direction. |y| decreases as we move through our children.
      aY = aClientRect.y + aOriginalRect.height;
    }
  }

  // Get our pack/alignment information.
  nsIFrame::Halignment halign = aBox->GetHAlign();
  nsIFrame::Valignment valign = aBox->GetVAlign();

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
nsSprocketLayout::Layout(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  // See if we are collapsed. If we are, then simply iterate over all our
  // children and give them a rect of 0 width and height.
  if (aBox->IsCollapsed()) {
    nsIFrame* child = aBox->GetChildBox();
    while(child) 
    {
      nsBoxFrame::LayoutChildAt(aState, child, nsRect(0,0,0,0));  
      child = child->GetNextBox();
    }
    return NS_OK;
  }

  nsBoxLayoutState::AutoReflowDepth depth(aState);
  mozilla::AutoStackArena arena;

  // ----- figure out our size ----------
  const nsSize originalSize = aBox->GetSize();

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
  nsBoxSize*         boxSizes = nullptr;
  nsComputedBoxSize* computedBoxSizes = nullptr;

  nscoord min = 0;
  nscoord max = 0;
  PRInt32 flexes = 0;
  PopulateBoxSizes(aBox, aState, boxSizes, min, max, flexes);
  
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
  bool needsRedraw = false;
  bool finished;
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
  bool childResized = false;

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
    finished = true;

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

    nsIFrame* child = aBox->GetChildBox();

    PRInt32 count = 0;
    while (child || (childBoxSize && childBoxSize->bogus))
    { 
      // If for some reason, our lists are not the same length, we guard
      // by bailing out of the loop.
      if (childBoxSize == nullptr) {
        NS_NOTREACHED("Lists not the same length.");
        break;
      }
        
      nscoord width = clientRect.width;
      nscoord height = clientRect.height;

      if (!childBoxSize->bogus) {
        // We have a valid box size entry.  This entry already contains information about our
        // sizes along the axis of the box (e.g., widths in a horizontal box).  If our default
        // ALIGN is not stretch, however, then we also need to know the child's size along the
        // opposite axis.
        if (!(frameState & NS_STATE_AUTO_STRETCH)) {
           nsSize prefSize = child->GetPrefSize(aState);
           nsSize minSize = child->GetMinSize(aState);
           nsSize maxSize = child->GetMaxSize(aState);
           prefSize = nsBox::BoundsCheck(minSize, prefSize, maxSize);
       
           AddMargin(child, prefSize);
           width = NS_MIN(prefSize.width, originalClientRect.width);
           height = NS_MIN(prefSize.height, originalClientRect.height);
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
      if (childRect.width > clientRect.width)
        clientRect.width = childRect.width;

      if (childRect.height > clientRect.height)
        clientRect.height = childRect.height;
    
      // Either |nextX| or |nextY| is updated by this function call, according
      // to our axis.
      ComputeChildsNextPosition(aBox, x, y, nextX, nextY, childRect);

      // Now we further update our nextX/Y along our axis.
      // We also set childRect.y/x along the opposite axis appropriately for a
      // stretch alignment.  (Non-stretch alignment is handled below.)
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          nextX += (childBoxSize->right);
        else
          nextX -= (childBoxSize->left);
        childRect.y = originalClientRect.y;
      }
      else {
        if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
          nextY += (childBoxSize->right);
        else 
          nextY -= (childBoxSize->left);
        childRect.x = originalClientRect.x;
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

      bool layout = true;

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
        layout = false;
      } else {
        // Always perform layout if we are dirty or have dirty children
        if (!NS_SUBTREE_DIRTY(child))
          layout = false;
      }

      nsRect oldRect(child->GetRect());

      // Non-stretch alignment will be handled in AlignChildren(), so don't
      // change child out-of-axis positions yet.
      if (!(frameState & NS_STATE_AUTO_STRETCH)) {
        if (frameState & NS_STATE_IS_HORIZONTAL) {
          childRect.y = oldRect.y;
        } else {
          childRect.x = oldRect.x;
        }
      }

      // We computed a childRect.  Now we want to set the bounds of the child to be that rect.
      // If our old rect is different, then we know our size changed and we cache that fact
      // in the |sizeChanged| variable.

      child->SetBounds(aState, childRect);
      bool sizeChanged = (childRect.width != oldRect.width ||
                            childRect.height != oldRect.height);

      if (sizeChanged) {
        // Our size is different.  Sanity check against our maximum allowed size to ensure
        // we didn't exceed it.
        nsSize minSize = child->GetMinSize(aState);
        nsSize maxSize = child->GetMaxSize(aState);
        maxSize = nsBox::BoundsCheckMinMax(minSize, maxSize);

        // make sure the size is in our max size.
        if (childRect.width > maxSize.width)
          childRect.width = maxSize.width;

        if (childRect.height > maxSize.height)
          childRect.height = maxSize.height;
           
        // set it again
        child->SetBounds(aState, childRect);

        // Since the child changed size, we know a redraw is probably going to be required.
        needsRedraw = true;
      }

      // if something moved then we might need to redraw
      if (oldRect.x != childRect.x || oldRect.y != childRect.y)
          needsRedraw = true;

      // If we already determined that layout was required or if our size has changed, then
      // we make sure to call layout on the child, since its children may need to be shifted
      // around as a result of the size change.
      if (layout || sizeChanged)
        child->Layout(aState);
      
      // If the child was a block or inline (e.g., HTML) it may have changed its rect *during* layout. 
      // We have to check for this.
      nsRect newChildRect(child->GetRect());

      if (!newChildRect.IsEqualInterior(childRect)) {
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
        childResized = true;

        // Now that a child resized, it's entirely possible that OUR rect is too small.  Now we
        // ensure that |originalClientRect| is grown to accommodate the size of |clientRect|.
        if (clientRect.width > originalClientRect.width)
          originalClientRect.width = clientRect.width;

        if (clientRect.height > originalClientRect.height)
          originalClientRect.height = clientRect.height;

        if (!(frameState & NS_STATE_IS_DIRECTION_NORMAL)) {
          // Our childRect had its XMost() or YMost() (depending on our layout
          // direction), positioned at a certain point.  Ensure that the
          // newChildRect satisfies the same constraint.  Note that this is
          // just equivalent to adjusting the x/y by the difference in
          // width/height between childRect and newChildRect.  So we don't need
          // to reaccount for the left and right of the box layout state again.
          if (frameState & NS_STATE_IS_HORIZONTAL)
            newChildRect.x = childRect.XMost() - newChildRect.width;
          else
            newChildRect.y = childRect.YMost() - newChildRect.height;
        }

        // If the child resized then recompute its position.
        ComputeChildsNextPosition(aBox, x, y, nextX, nextY, newChildRect);

        if (newChildRect.width >= margin.left + margin.right && newChildRect.height >= margin.top + margin.bottom) 
          newChildRect.Deflate(margin);

        if (childRect.width >= margin.left + margin.right && childRect.height >= margin.top + margin.bottom) 
          childRect.Deflate(margin);
            
        child->SetBounds(aState, newChildRect);

        // If we are the first box that changed size, then we don't need to do a second pass
        if (count == 0)
          finished = true;
      }

      // Now update our x/y finally.
      x = nextX;
      y = nextY;
     
      // Move to the next child.
      childComputedBoxSize = childComputedBoxSize->next;
      childBoxSize = childBoxSize->next;

      child = child->GetNextBox();
      count++;
    }

    // Sanity-checking code to ensure we don't do an infinite # of passes.
    passes++;
    NS_ASSERTION(passes < 10, "A Box's child is constantly growing!!!!!");
    if (passes > 10)
      break;
  } while (false == finished);

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

    if (tmpClientRect.width > originalSize.width || tmpClientRect.height > originalSize.height)
    {
      // if it did reset our bounds.
      nsRect bounds(aBox->GetRect());
      if (tmpClientRect.width > originalSize.width)
        bounds.width = tmpClientRect.width;

      if (tmpClientRect.height > originalSize.height)
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
    nsIFrame* child = aBox->GetChildBox();

    // reposition all our children
    while (child) 
    {
      nsRect childRect(child->GetRect());
      childRect.x += (x - origX);
      childRect.y += (y - origY);
      child->SetBounds(aState, childRect);
      child = child->GetNextBox();
    }
  }

  // Perform out-of-axis alignment for non-stretch alignments
  if (!(frameState & NS_STATE_AUTO_STRETCH)) {
    AlignChildren(aBox, aState, &needsRedraw);
  }

  // Now do our redraw.
  if (needsRedraw)
    aBox->Redraw(aState);

  // That's it!  If you made it this far without having a nervous breakdown, 
  // congratulations!  Go get yourself a beer.
  return NS_OK;
}

void
nsSprocketLayout::PopulateBoxSizes(nsIFrame* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
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

  bool isHorizontal;

  if (IsHorizontal(aBox))
     isHorizontal = true;
  else
     isHorizontal = false;

  // this is a nice little optimization
  // it turns out that if we only have 1 flexable child
  // then it does not matter what its preferred size is
  // there is nothing to flex it relative. This is great
  // because we can avoid asking for a preferred size in this
  // case. Why is this good? Well you might have html inside it
  // and asking html for its preferred size is rather expensive.
  // so we can just optimize it out this way.

  // set flexes
  nsIFrame* child = aBox->GetChildBox();

  aFlexes = 0;
  nsBoxSize* currentBox = nullptr;

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
    

      flex = child->GetFlex(aState);

      currentBox->flex = flex;
      currentBox->collapsed = child->IsCollapsed();
    } else {
      flex = start->flex;
      start = start->next;
    }
    
    if (flex > 0) 
       aFlexes++;
   
    child = child->GetNextBox();
  }
#endif

  // get pref, min, max
  child = aBox->GetChildBox();
  currentBox = aBoxSizes;
  nsBoxSize* last = nullptr;

  nscoord maxFlex = 0;
  PRInt32 childCount = 0;

  while(child)
  {
    while (currentBox && currentBox->bogus) {
      last = currentBox;
      currentBox = currentBox->next;
    }
    ++childCount;
    nsSize pref(0,0);
    nsSize minSize(0,0);
    nsSize maxSize(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
    nscoord ascent = 0;
    bool collapsed = child->IsCollapsed();

    if (!collapsed) {
    // only one flexible child? Cool we will just make its preferred size
    // 0 then and not even have to ask for it.
    //if (flexes != 1)  {

      pref = child->GetPrefSize(aState);
      minSize = child->GetMinSize(aState);
      maxSize = nsBox::BoundsCheckMinMax(minSize, child->GetMaxSize(aState));
      ascent = child->GetBoxAscent(aState);
      nsMargin margin;
      child->GetMargin(margin);
      ascent += margin.top;
    //}

      pref = nsBox::BoundsCheck(minSize, pref, maxSize);

      AddMargin(child, pref);
      AddMargin(child, minSize);
      AddMargin(child, maxSize);
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
          minWidth  = minSize.width;
          maxWidth  = maxSize.width;
          prefWidth = pref.width;
      } else {
          minWidth = minSize.height;
          maxWidth = maxSize.height;
          prefWidth = pref.height;
      }

      nscoord flex = child->GetFlex(aState);

      // set them if you collapsed you are not flexible.
      if (collapsed) {
        currentBox->flex = 0;
      }
      else {
        if (flex > maxFlex) {
          maxFlex = flex;
        }
        currentBox->flex = flex;
      }

      // we specified all our children are equal size;
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
      if (minSize.width > aMinSize)
        aMinSize = minSize.width;

      if (maxSize.width < aMaxSize)
        aMaxSize = maxSize.width;

    } else {
      if (minSize.height > aMinSize)
        aMinSize = minSize.height;

      if (maxSize.height < aMaxSize)
        aMaxSize = maxSize.height;
    }

    currentBox->collapsed = collapsed;
    aFlexes += currentBox->flex;

    child = child->GetNextBox();

    last = currentBox;
    currentBox = currentBox->next;

  }

  if (childCount > 0) {
    nscoord maxAllowedFlex = nscoord_MAX / childCount;
  
    if (NS_UNLIKELY(maxFlex > maxAllowedFlex)) {
      // clamp all the flexes
      currentBox = aBoxSizes;
      while (currentBox) {
        currentBox->flex = NS_MIN(currentBox->flex, maxAllowedFlex);
        currentBox = currentBox->next;      
      }
    }
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(maxFlex == 0, "How did that happen?");
  }
#endif

  // we specified all our children are equal size;
  if (frameState & NS_STATE_EQUAL_SIZE) {
    smallestMaxWidth = NS_MAX(smallestMaxWidth, biggestMinWidth);
    biggestPrefWidth = nsBox::BoundsCheck(biggestMinWidth, biggestPrefWidth, smallestMaxWidth);

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
nsSprocketLayout::ComputeChildsNextPosition(nsIFrame* aBox, 
                                      const nscoord& aCurX, 
                                      const nscoord& aCurY, 
                                      nscoord& aNextX, 
                                      nscoord& aNextY, 
                                      const nsRect& aCurrentChildSize)
{
  // Get the position along the box axis for the child.
  // The out-of-axis position is not set.
  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);

  if (IsHorizontal(aBox)) {
    // horizontal box's children.
    if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
      aNextX = aCurX + aCurrentChildSize.width;
    else
      aNextX = aCurX - aCurrentChildSize.width;

  } else {
    // vertical box's children.
    if (frameState & NS_STATE_IS_DIRECTION_NORMAL)
      aNextY = aCurY + aCurrentChildSize.height;
    else
      aNextY = aCurY - aCurrentChildSize.height;
  }
}

void
nsSprocketLayout::AlignChildren(nsIFrame* aBox,
                                nsBoxLayoutState& aState,
                                bool* aNeedsRedraw)
{
  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);
  bool isHorizontal = (frameState & NS_STATE_IS_HORIZONTAL) != 0;
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  NS_PRECONDITION(!(frameState & NS_STATE_AUTO_STRETCH),
                  "Only AlignChildren() with non-stretch alignment");

  // These are only calculated if needed
  nsIFrame::Halignment halign;
  nsIFrame::Valignment valign;
  nscoord maxAscent;
  bool isLTR;

  if (isHorizontal) {
    valign = aBox->GetVAlign();
    if (valign == nsBoxFrame::vAlign_BaseLine) {
      maxAscent = aBox->GetBoxAscent(aState);
    }
  } else {
    isLTR = GetFrameDirection(aBox) == NS_STYLE_DIRECTION_LTR;
    halign = aBox->GetHAlign();
  }

  nsIFrame* child = aBox->GetChildBox();
  while (child) {

    nsMargin margin;
    child->GetMargin(margin);
    nsRect childRect = child->GetRect();

    if (isHorizontal) {
      const nscoord startAlign = clientRect.y + margin.top;
      const nscoord endAlign =
        clientRect.YMost() - margin.bottom - childRect.height;

      nscoord y;
      switch (valign) {
        case nsBoxFrame::vAlign_Top:
          y = startAlign;
          break;
        case nsBoxFrame::vAlign_Middle:
          // Should this center the border box?
          // This centers the margin box, the historical behavior.
          y = (startAlign + endAlign) / 2;
          break;
        case nsBoxFrame::vAlign_Bottom:
          y = endAlign;
          break;
        case nsBoxFrame::vAlign_BaseLine:
          // Alignments don't force the box to grow (only sizes do),
          // so keep the children within the box.
          y = maxAscent - child->GetBoxAscent(aState);
          y = NS_MAX(startAlign, y);
          y = NS_MIN(y, endAlign);
          break;
      }

      childRect.y = y;

    } else { // vertical box
      const nscoord leftAlign = clientRect.x + margin.left;
      const nscoord rightAlign =
        clientRect.XMost() - margin.right - childRect.width;

      nscoord x;
      switch (halign) {
        case nsBoxFrame::hAlign_Left: // start
          x = isLTR ? leftAlign : rightAlign;
          break;
        case nsBoxFrame::hAlign_Center:
          x = (leftAlign + rightAlign) / 2;
          break;
        case nsBoxFrame::hAlign_Right: // end
          x = isLTR ? rightAlign : leftAlign;
          break;
      }

      childRect.x = x;
    }

    if (childRect.TopLeft() != child->GetPosition()) {
      *aNeedsRedraw = true;
      child->SetBounds(aState, childRect);
    }

    child = child->GetNextBox();
  }
}

void
nsSprocketLayout::ChildResized(nsIFrame* aBox,
                         nsBoxLayoutState& aState, 
                         nsIFrame* aChild,
                         nsBoxSize* aChildBoxSize,
                         nsComputedBoxSize* aChildComputedSize,
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize* aComputedBoxSizes, 
                         const nsRect& aChildLayoutRect, 
                         nsRect& aChildActualRect, 
                         nsRect& aContainingRect,
                         PRInt32 aFlexes, 
                         bool& aFinished)
                         
{
      nsRect childCurrentRect(aChildLayoutRect);

      bool isHorizontal = IsHorizontal(aBox);
      nscoord childLayoutWidth  = GET_WIDTH(aChildLayoutRect,isHorizontal);
      nscoord& childActualWidth  = GET_WIDTH(aChildActualRect,isHorizontal);
      nscoord& containingWidth   = GET_WIDTH(aContainingRect,isHorizontal);   
      
      //nscoord childLayoutHeight = GET_HEIGHT(aChildLayoutRect,isHorizontal);
      nscoord& childActualHeight = GET_HEIGHT(aChildActualRect,isHorizontal);
      nscoord& containingHeight  = GET_HEIGHT(aContainingRect,isHorizontal);

      bool recompute = false;

      // if we are a horizontal box see if the child will fit inside us.
      if ( childActualHeight > containingHeight) {
            // if we are a horizontal box and the child is bigger than our height

            // ok if the height changed then we need to reflow everyone but us at the new height
            // so we will set the changed index to be us. And signal that we need a new pass.

            nsSize min = aChild->GetMinSize(aState);            
            nsSize max = nsBox::BoundsCheckMinMax(min, aChild->GetMaxSize(aState));
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
              aFinished = false;

              // only recompute if there are flexes.
              if (aFlexes > 0) {
                // relayout everything
                recompute = true;
                InvalidateComputedSizes(aComputedBoxSizes);
                nsComputedBoxSize* node = aComputedBoxSizes;

                while(node) {
                  node->resized = false;
                  node = node->next;
                }

              }              
            }
      } 
      
      if (childActualWidth > childLayoutWidth) {
            nsSize min = aChild->GetMinSize(aState);
            nsSize max = nsBox::BoundsCheckMinMax(min, aChild->GetMaxSize(aState));
            
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
               if (aChildBoxSize->max < childActualWidth)
                  aChildBoxSize->max = childActualWidth;

              // if we have flexible elements with us then reflex things. Otherwise we can skip doing it.
              if (aFlexes > 0) {
                InvalidateComputedSizes(aComputedBoxSizes);

                nsComputedBoxSize* node = aComputedBoxSizes;
                aChildComputedSize->resized = true;

                while(node) {
                  if (node->resized)
                      node->valid = true;
                
                  node = node->next;
                }

                recompute = true;
                aFinished = false;
              } else {
                containingWidth += aChildComputedSize->size - childLayoutWidth;
              }              
            }
      }

      if (recompute)
            ComputeChildSizes(aBox, aState, containingWidth, aBoxSizes, aComputedBoxSizes);

      if (!childCurrentRect.IsEqualInterior(aChildActualRect)) {
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
      aComputedBoxSizes->valid = false;
      aComputedBoxSizes = aComputedBoxSizes->next;
  }
}

void
nsSprocketLayout::ComputeChildSizes(nsIFrame* aBox,
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
    //  computedBoxSizes->valid = true;
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
            computedBoxSizes->valid = true;
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
    bool limit = true;
    for (int pass=1; true == limit; pass++) 
    {
      limit = false;
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
            PRInt32 newSize = pref + PRInt32(PRInt64(sizeRemaining) * flex / spacerConstantsRemaining);

            if (newSize<=min) {
              computedBoxSizes->size = min;
              computedBoxSizes->valid = true;
              spacerConstantsRemaining -= flex;
              sizeRemaining += pref;
              sizeRemaining -= min;
              limit = true;
            } else if (newSize>=max) {
              computedBoxSizes->size = max;
              computedBoxSizes->valid = true;
              spacerConstantsRemaining -= flex;
              sizeRemaining += pref;
              sizeRemaining -= max;
              limit = true;
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
        computedBoxSizes->size = pref + PRInt32(PRInt64(sizeRemaining) * flex / spacerConstantsRemaining);
        computedBoxSizes->valid = true;
      }

      aGivenSize += (boxSizes->left + boxSizes->right);
      aGivenSize += computedBoxSizes->size;

   // }

    boxSizes         = boxSizes->next;
    computedBoxSizes = computedBoxSizes->next;
  }
}


nsSize
nsSprocketLayout::GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
   nsSize vpref (0, 0); 
   bool isHorizontal = IsHorizontal(aBox);

   nscoord biggestPref = 0;

   // run through all the children and get their min, max, and preferred sizes
   // return us the size of the box

   nsIFrame* child = aBox->GetChildBox();
   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   bool isEqual = !!(frameState & NS_STATE_EQUAL_SIZE);
   PRInt32 count = 0;
   
   while (child) 
   {  
      // ignore collapsed children
      if (!child->IsCollapsed())
      {
        nsSize pref = child->GetPrefSize(aState);
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

        AddLargestSize(vpref, pref, isHorizontal);
        count++;
      }

      child = child->GetNextBox();
   }

   if (isEqual) {
      if (isHorizontal)
         vpref.width = biggestPref*count;
      else
         vpref.height = biggestPref*count;
   }
    
   // now add our border and padding
   AddBorderAndPadding(aBox, vpref);

  return vpref;
}

nsSize
nsSprocketLayout::GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
   nsSize minSize (0, 0);
   bool isHorizontal = IsHorizontal(aBox);

   nscoord biggestMin = 0;


   // run through all the children and get their min, max, and preferred sizes
   // return us the size of the box

   nsIFrame* child = aBox->GetChildBox();
   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   bool isEqual = !!(frameState & NS_STATE_EQUAL_SIZE);
   PRInt32 count = 0;

   while (child) 
   {  
       // ignore collapsed children
      if (!child->IsCollapsed())
      {
        nsSize min = child->GetMinSize(aState);
        nsSize pref(0,0);
        
        // if the child is not flexible then
        // its min size is its pref size.
        if (child->GetFlex(aState) == 0) {
            pref = child->GetPrefSize(aState);
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
        AddLargestSize(minSize, min, isHorizontal);
        count++;
      }

      child = child->GetNextBox();
   }

   
   if (isEqual) {
      if (isHorizontal)
         minSize.width = biggestMin*count;
      else
         minSize.height = biggestMin*count;
   }

  // now add our border and padding
  AddBorderAndPadding(aBox, minSize);

  return minSize;
}

nsSize
nsSprocketLayout::GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{

  bool isHorizontal = IsHorizontal(aBox);

   nscoord smallestMax = NS_INTRINSICSIZE;
   nsSize maxSize (NS_INTRINSICSIZE, NS_INTRINSICSIZE);

   // run through all the children and get their min, max, and preferred sizes
   // return us the size of the box

   nsIFrame* child = aBox->GetChildBox();
   nsFrameState frameState = 0;
   GetFrameState(aBox, frameState);
   bool isEqual = !!(frameState & NS_STATE_EQUAL_SIZE);
   PRInt32 count = 0;

   while (child) 
   {  
      // ignore collapsed children
      if (!child->IsCollapsed())
      {
        // if completely redefined don't even ask our child for its size.
        nsSize min = child->GetMinSize(aState);
        nsSize max = nsBox::BoundsCheckMinMax(min, child->GetMaxSize(aState));

        AddMargin(child, max);
        AddSmallestSize(maxSize, max, isHorizontal);

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

      child = child->GetNextBox();
   }

   if (isEqual) {
     if (isHorizontal) {
         if (smallestMax != NS_INTRINSICSIZE)
            maxSize.width = smallestMax*count;
         else
            maxSize.width = NS_INTRINSICSIZE;
     } else {
         if (smallestMax != NS_INTRINSICSIZE)
            maxSize.height = smallestMax*count;
         else
            maxSize.height = NS_INTRINSICSIZE;
     }
   }

  // now add our border and padding
  AddBorderAndPadding(aBox, maxSize);

  return maxSize;
}


nscoord
nsSprocketLayout::GetAscent(nsIFrame* aBox, nsBoxLayoutState& aState)
{
   nscoord vAscent = 0;

   bool isHorizontal = IsHorizontal(aBox);

   // run through all the children and get their min, max, and preferred sizes
   // return us the size of the box
   
   nsIFrame* child = aBox->GetChildBox();
   
   while (child) 
   {  
      // ignore collapsed children
      //if (!child->IsCollapsed())
      //{
        // if completely redefined don't even ask our child for its size.
        nscoord ascent = child->GetBoxAscent(aState);

        nsMargin margin;
        child->GetMargin(margin);
        ascent += margin.top;

        if (isHorizontal)
        {
          if (ascent > vAscent)
            vAscent = ascent;
        } else {
          if (vAscent == 0)
            vAscent = ascent;
        }
      //}

      child = child->GetNextBox();      
   }

   nsMargin borderPadding;
   aBox->GetBorderAndPadding(borderPadding);

   return vAscent + borderPadding.top;
}

void
nsSprocketLayout::SetLargestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal)
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
nsSprocketLayout::SetSmallestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal)
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
nsSprocketLayout::AddLargestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal)
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
nsSprocketLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal)
{
  if (aIsHorizontal)
    AddCoord(aSize.width, aSizeToAdd.width);
  else
    AddCoord(aSize.height, aSizeToAdd.height);
    
  SetSmallestSize(aSize, aSizeToAdd, aIsHorizontal);
}

bool
nsSprocketLayout::GetDefaultFlex(PRInt32& aFlex)
{
    aFlex = 0;
    return true;
}

nsComputedBoxSize::nsComputedBoxSize()
{
  resized = false;
  valid = false;
  size = 0;
  next = nullptr;
}

nsBoxSize::nsBoxSize()
{
  pref = 0;
  min = 0;
  max = NS_INTRINSICSIZE;
  collapsed = false;
  left = 0;
  right = 0;
  flex = 0;
  next = nullptr;
  bogus = false;
}


void* 
nsBoxSize::operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW
{
  return mozilla::AutoStackArena::Allocate(sz);
}


void 
nsBoxSize::operator delete(void* aPtr, size_t sz)
{
}


void* 
nsComputedBoxSize::operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW
{
   return mozilla::AutoStackArena::Allocate(sz);
}

void 
nsComputedBoxSize::operator delete(void* aPtr, size_t sz)
{
}
