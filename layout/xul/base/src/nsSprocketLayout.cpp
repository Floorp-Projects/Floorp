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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxFrame.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsBoxFrame.h"

nsCOMPtr<nsIBoxLayout> nsSprocketLayout::gInstance = new nsSprocketLayout();

//#define DEBUG_GROW

#define DEBUG_SPRING_SIZE 8
#define DEBUG_BORDER_SIZE 2
#define COIL_SIZE 8


nsresult
NS_NewSprocketLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  // we have not instance variables so just return our static one.
  aNewLayout = nsSprocketLayout::gInstance;
  return NS_OK;
} 

nsSprocketLayout::nsSprocketLayout()
{
}

PRBool 
nsSprocketLayout::IsHorizontal(nsIBox* aBox) const
{
   nsIFrame* frame = nsnull;
   aBox->GetFrame(&frame);
   nsFrameState state;
   frame->GetFrameState(&state);
   return state & NS_STATE_IS_HORIZONTAL;
}

void
nsSprocketLayout::GetFrameState(nsIBox* aBox, nsFrameState& aState)
{
   nsIFrame* frame = nsnull;
   aBox->GetFrame(&frame);
   frame->GetFrameState(&aState);
}

void
nsSprocketLayout::SetFrameState(nsIBox* aBox, nsFrameState aState)
{
   nsIFrame* frame = nsnull;
   aBox->GetFrame(&frame);
   frame->SetFrameState(aState);
}

NS_IMETHODIMP
nsSprocketLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  // see if we are collapsed. If we are? its easy. Layout out each child at 0,0,0,0;
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

  nsRect originalClientRect(clientRect);

  nsFrameState frameState = 0;
  GetFrameState(aBox, frameState);

  //if (frameState & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");

  // ----- build a list of our child desired sizes and computeds sizes ------

  nsBoxSize*         boxSizes = nsnull;
  nsComputedBoxSize* computedBoxSizes = nsnull;

  nscoord maxAscent = 0;
  aBox->GetAscent(aState, maxAscent);

  nscoord min = 0;
  nscoord max = 0;
  PRInt32 flexes = 0;
  PopulateBoxSizes(aBox, aState, boxSizes, computedBoxSizes, min, max, flexes);
  
  nscoord size = clientRect.width;
  if (!IsHorizontal(aBox))
      size = clientRect.height;

  ComputeChildSizes(aBox, aState, size, boxSizes, computedBoxSizes);


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

  // ----------------------
  // layout all children 
  // ----------------------

  PRBool needsRedraw = PR_FALSE;
  PRBool finished;
  nscoord passes = 0;


  // ok what we want to do if flow each child at the location given in the spring.
  // unfortunately after flowing a child it might get bigger. We have not control over this
  // so it the child gets bigger or smaller than we expected we will have to do a 2nd, 3rd, 4th pass to 
  // adjust. 

  nscoord x = 0;
  nscoord y = 0;
  nsIBox::Halignment halign;
  nsIBox::Valignment valign;
  aBox->GetVAlign(valign);
  aBox->GetHAlign(halign);
  nscoord origX = 0;
  nscoord origY = 0;

  PRBool childResized = PR_FALSE;


  passes = 0;
  do 
  { 
    #ifdef DEBUG_REFLOW
    if (passes > 0) {
      AddIndents();
      printf("ChildResized doing pass: %d\n", passes);
    }
    #endif 

    finished = PR_TRUE;

    x = clientRect.x;
    y = clientRect.y;

    //if (!(frameState & NS_STATE_AUTO_STRETCH)) {
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        switch(halign) {
          case nsBoxFrame::hAlign_Left:
            x = clientRect.x;
          break;

          case nsBoxFrame::hAlign_Center:
            x = clientRect.x + (originalClientRect.width - clientRect.width)/2;
          break;

          case nsBoxFrame::hAlign_Right:
            x = clientRect.x + (originalClientRect.width - clientRect.width);
          break;
        }
      } else {
        switch(valign) {
          case nsBoxFrame::vAlign_Top:
          case nsBoxFrame::vAlign_BaseLine:
          y = clientRect.y;
          break;

          case nsBoxFrame::vAlign_Middle:
            y = clientRect.y + (originalClientRect.height - clientRect.height)/2;
          break;

          case nsBoxFrame::vAlign_Bottom:
            y = clientRect.y + (originalClientRect.height - clientRect.height);
          break;
        }
      }
   // }

    origX = x;
    origY = y;

    nscoord nextX = x;
    nscoord nextY = y;

    nsComputedBoxSize* childComputedBoxSize = computedBoxSizes;
    nsBoxSize* childBoxSize                 = boxSizes;

    nsIBox* child = nsnull;
    aBox->GetChildBox(&child);

    PRInt32 count = 0;
    while (child || (childBoxSize && childBoxSize->bogus))
    {    
      nscoord width = clientRect.width;
      nscoord height = clientRect.height;

      nsSize prefSize(0,0);
      nsSize minSize(0,0);
      nsSize maxSize(0,0);

      if (!childBoxSize->bogus) {
        if (!(frameState & NS_STATE_AUTO_STRETCH)) {
           child->GetPrefSize(aState, prefSize);
           child->GetMinSize(aState, minSize);
           child->GetMaxSize(aState, maxSize);
           nsBox::BoundsCheck(minSize, prefSize, maxSize);
       
           AddMargin(child, prefSize);
           width = prefSize.width > originalClientRect.width ? originalClientRect.width : prefSize.width;
           height = prefSize.height > originalClientRect.height ? originalClientRect.height : prefSize.height;
        }
      }

      // figure our our child's computed width and height
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        width = childComputedBoxSize->size;
      } else  {
        height = childComputedBoxSize->size;
      }
      
      if (frameState & NS_STATE_IS_HORIZONTAL) 
         x += (childBoxSize->left);
      else
         y += (childBoxSize->left);

      nextX = x;
      nextY = y;

      nsRect childRect(x, y, width, height);

     // if (!childBoxSize->collapsed) {
        
        if (childRect.width > clientRect.width || childRect.height > clientRect.height) {
           if (childRect.width > clientRect.width)
              clientRect.width = childRect.width;

            if (childRect.height > clientRect.height)
              clientRect.height = childRect.height;
        }
     // }      

      ComputeChildsNextPosition(aBox, child, 
                    x, 
                    y, 
                    nextX, 
                    nextY, 
                    childRect, 
                    originalClientRect, 
                    childBoxSize->ascent,
                    maxAscent);


      if (frameState & NS_STATE_IS_HORIZONTAL) 
         nextX += (childBoxSize->right);
      else
         nextY += (childBoxSize->right);

      childRect.x = x;
      childRect.y = y;

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

      child->GetMargin(margin);
      //if (childRect.width >= margin.left + margin.right && childRect.height >= margin.top + margin.bottom) 
      childRect.Deflate(margin);
      if (childRect.width < 0)
        childRect.width = 0;

      if (childRect.height < 0)
        childRect.height = 0;

      if (passes > 0) {
        layout = PR_FALSE;
      } else {
        // always layout if we are dirty or have dirty children
        PRBool dirty = PR_FALSE;           
        PRBool dirtyChildren = PR_FALSE;           
        child->IsDirty(dirty);
        child->HasDirtyChildren(dirtyChildren);
        if (!(dirty || dirtyChildren) && aState.GetLayoutReason() != nsBoxLayoutState::Initial) {
           layout = PR_FALSE;
        }
      }

      nsRect oldRect(0,0,0,0);
      PRBool sizeChanged = PR_FALSE;

    //  if (childBoxSize->collapsed || layout) {
        child->GetBounds(oldRect);
        child->SetBounds(aState, childRect);
        sizeChanged = (childRect.width != oldRect.width || childRect.height != oldRect.height);
     // }

      PRBool possibleRedraw = PR_FALSE;

      if (sizeChanged) {
         nsSize maxSize;
         child->GetMaxSize(aState, maxSize);

         // make sure the size is in our max size.
         if (childRect.width > maxSize.width)
            childRect.width = maxSize.width;

         if (childRect.height > maxSize.height)
            childRect.height = maxSize.height;
           
         // set it again
         child->SetBounds(aState, childRect);

         // ok a child changed size so we should redraw the whole box. But it still has the possibility of 
         // redeeming itself. If it happens to get bigger during layout like
         // wrapping text might. It may still end up the same size as it was. Then
         // we don't need to redraw ourselves.
         possibleRedraw = PR_TRUE;
      }


      if (layout || sizeChanged) {
        child->Layout(aState);
      }
      
      // make collapsed children not show up
     // if (childBoxSize && childBoxSize->collapsed) {
     //   if (layout || sizeChanged) 
     //       child->Collapse(aState);
      //  else
      //      child->SetBounds(aState, nsRect(0,0,0,0));
      //} else 
      {
          // if the child was html it may have changed its rect. Lets look
          nsRect newChildRect;
          child->GetBounds(newChildRect);

          if (newChildRect != childRect)
          {
#ifdef DEBUG_GROW
            child->DumpBox(stdout);
            printf(" GREW from (%d,%d) -> (%d,%d)\n", childRect.width, childRect.height, newChildRect.width, newChildRect.height);
#endif
            newChildRect.Inflate(margin);
            childRect.Inflate(margin);

            // ok the child changed size
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

            childResized = PR_TRUE;

            if (clientRect.width > originalClientRect.width || clientRect.height > originalClientRect.height) {
               if (clientRect.width > originalClientRect.width)
                  originalClientRect.width = clientRect.width;

                if (clientRect.height > originalClientRect.height)
                  originalClientRect.height = clientRect.height;
            }


            // if the child resized then recompute it position.
            ComputeChildsNextPosition(aBox, child, 
                                      x, 
                                      y, 
                                      nextX, 
                                      nextY, 
                                      newChildRect, 
                                      originalClientRect, 
                                      childBoxSize->ascent,
                                      maxAscent);

            newChildRect.x = x;
            newChildRect.y = y;

            if (newChildRect.width >= margin.left + margin.right && newChildRect.height >= margin.top + margin.bottom) 
              newChildRect.Deflate(margin);

            if (childRect.width >= margin.left + margin.right && childRect.height >= margin.top + margin.bottom) 
              childRect.Deflate(margin);
            

            child->SetBounds(aState, newChildRect);

            // if the child resized but was the exact size it was. Don't redraw.
            if (oldRect.width == newChildRect.width || oldRect.height == newChildRect.height)
               possibleRedraw = PR_FALSE;

              // if we are the first we don't need to do a second pass
              if (count == 0)
                 finished = PR_TRUE;
          }
      
        x = nextX;
        y = nextY;
      }

      if (possibleRedraw)
        needsRedraw = PR_TRUE;

      childComputedBoxSize = childComputedBoxSize->next;
      childBoxSize = childBoxSize->next;

      child->GetNextBox(&child);
      count++;
    }

    passes++;
    NS_ASSERTION(passes < 10, "A Box's child is constantly growing!!!!!");
    if (passes > 10) {
      break;
    }
  } while (PR_FALSE == finished);

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
    // see if one of our children forced us to get bigger
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

    x = clientRect.x;
    y = clientRect.y;

    //if (!(frameState & NS_STATE_AUTO_STRETCH)) {
      if (frameState & NS_STATE_IS_HORIZONTAL) {
        switch(halign) {
          case nsBoxFrame::hAlign_Left:
            x = clientRect.x;
          break;

          case nsBoxFrame::hAlign_Center:
            x = clientRect.x + (originalClientRect.width - clientRect.width)/2;
          break;

          case nsBoxFrame::hAlign_Right:
            x = clientRect.x + (originalClientRect.width - clientRect.width);
          break;
        }
      } else {
        switch(valign) {
          case nsBoxFrame::vAlign_Top:
          case nsBoxFrame::vAlign_BaseLine:
          y = clientRect.y;
          break;

          case nsBoxFrame::vAlign_Middle:
            y = clientRect.y + (originalClientRect.height - clientRect.height)/2;
          break;

          case nsBoxFrame::vAlign_Bottom:
            y = clientRect.y + (originalClientRect.height - clientRect.height);
          break;
        }
      }
    //}

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

  if (needsRedraw)
    aBox->Redraw(aState);

  aState.PopStackMemory();

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
  // then it does not matter what its prefered size is
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

  /*
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
  */

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
      ascent += margin.top + margin.bottom;
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
        aNextX = aCurX + aCurrentChildSize.width;

        if (frameState & NS_STATE_AUTO_STRETCH) {
               aCurY = aBoxRect.y;
        } else {
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
        aNextY = aCurY + aCurrentChildSize.height;
        if (frameState & NS_STATE_AUTO_STRETCH) {
                   aCurX = aBoxRect.x;
        } else {
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

 // float p2t;
 // aState.GetPresContext()->GetScaledPixelsToTwips(&p2t);
  //nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  PRInt32 sizeRemaining            = aGivenSize;
  PRInt32 springConstantsRemaining = 0;

   // ----- calculate the springs constants and the size remaining -----

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

          springConstantsRemaining += boxSizes->flex;
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

        // ignore collapsed springs

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
            PRInt32 newSize = pref + sizeRemaining*flex/springConstantsRemaining; //NSToCoordRound(float((sizeRemaining*flex)/springConstantsRemaining));

            if (newSize<=min) {
              computedBoxSizes->size = min;
              computedBoxSizes->valid = PR_TRUE;
              springConstantsRemaining -= flex;
              sizeRemaining += pref;
              sizeRemaining -= min;
              limit = PR_TRUE;
            } else if (newSize>=max) {
              computedBoxSizes->size = max;
              computedBoxSizes->valid = PR_TRUE;
              springConstantsRemaining -= flex;
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
  // ---- or shrink us. Depends on the size remaining and the spring constants
  nscoord oldsize = aGivenSize;
  aGivenSize = 0;
  boxSizes = aBoxSizes;
  computedBoxSizes = aComputedBoxSizes;

  while (boxSizes) { 

    // ignore collapsed springs
  //  if (!(boxSizes && boxSizes->collapsed)) {
    
      nscoord pref = 0;
      nscoord flex = 0;
      pref = boxSizes->pref;
      flex = boxSizes->flex;

      if (!computedBoxSizes->valid) {
        computedBoxSizes->size = pref + flex*sizeRemaining/springConstantsRemaining; //NSToCoordFloor(float((flex*sizeRemaining)/springConstantsRemaining));
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
        ascent += margin.top + margin.bottom;

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
nsBoxSize::operator new(size_t sz, nsBoxLayoutState& aState)
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
nsComputedBoxSize::operator new(size_t sz, nsBoxLayoutState& aState)
{
  
   void* mem = 0;
   aState.AllocateStackMemory(sz,&mem);
   return mem;
}

void 
nsComputedBoxSize::operator delete(void* aPtr, size_t sz)
{
}
