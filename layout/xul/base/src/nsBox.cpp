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
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsISelfScrollingFrame.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"

nsBox::nsBox(nsIPresShell* aShell):mParentBox(nsnull),mNextChild(nsnull),mMouseThrough(sometimes)
{
  //mX = 0;
  //mY = 0;
}

NS_IMETHODIMP 
nsBox::GetInset(nsMargin& margin)
{
  margin.SizeTo(0,0,0,0);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::IsDirty(PRBool& aDirty)
{
  nsFrameState state;
  nsIFrame* frame;
  GetFrame(&frame);
  frame->GetFrameState(&state);
  aDirty = (state & NS_FRAME_IS_DIRTY);  
  return NS_OK;
}

NS_IMETHODIMP
nsBox::HasDirtyChildren(PRBool& aDirty)
{
  nsFrameState state;
  nsIFrame* frame;
  GetFrame(&frame);
  frame->GetFrameState(&state);
  aDirty = (state & NS_FRAME_HAS_DIRTY_CHILDREN);  
  return NS_OK;
}

NS_IMETHODIMP
nsBox::MarkDirty(nsBoxLayoutState& aState)
{
  NeedsRecalc();

  nsFrameState state;
  nsIFrame* frame;
  GetFrame(&frame);
  frame->GetFrameState(&state);

  // only reflow if we aren't already dirty.
  if (state & NS_FRAME_IS_DIRTY)      
      return NS_OK;

  state |= NS_FRAME_IS_DIRTY;
  frame->SetFrameState(state);

  nsIFrame* parent = nsnull;
  frame->GetParent(&parent);
  nsCOMPtr<nsIPresShell> shell;
  aState.GetPresShell(getter_AddRefs(shell));

  return parent->ReflowDirtyChild(shell, frame);
}

NS_IMETHODIMP
nsBox::MarkDirtyChildren(nsBoxLayoutState& aState)
{
  nsFrameState state;
  nsIFrame* frame;
  GetFrame(&frame);
  frame->GetFrameState(&state);

  // only reflow if we aren't already dirty.
  if (state & NS_FRAME_HAS_DIRTY_CHILDREN)      
      return NS_OK;

  state |= NS_FRAME_HAS_DIRTY_CHILDREN;
  frame->SetFrameState(state);

  NeedsRecalc();
  nsIFrame* parent = nsnull;
  frame->GetParent(&parent);
  nsCOMPtr<nsIPresShell> shell;
  aState.GetPresShell(getter_AddRefs(shell));

  return parent->ReflowDirtyChild(shell, frame);
}

NS_IMETHODIMP
nsBox::GetVAlign(Valignment& aAlign)
{
  aAlign = vAlign_Top;
   return NS_OK;
}

NS_IMETHODIMP
nsBox::GetHAlign(Halignment& aAlign)
{
  aAlign = hAlign_Left;
   return NS_OK;
}

NS_IMETHODIMP
nsBox::GetClientRect(nsRect& aClientRect)
{
  GetContentRect(aClientRect);

  nsMargin borderPadding;
  GetBorderAndPadding(borderPadding);

  aClientRect.Deflate(borderPadding);

  nsMargin insets;
  GetInset(insets);

  aClientRect.Deflate(insets);
  if (aClientRect.width < 0)
     aClientRect.width = 0;

  if (aClientRect.height < 0)
     aClientRect.height = 0;

 // NS_ASSERTION(aClientRect.width >=0 && aClientRect.height >= 0, "Content Size < 0");

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetContentRect(nsRect& aContentRect)
{
  GetBounds(aContentRect);
  aContentRect.x = 0;
  aContentRect.y = 0;
  NS_ASSERTION(aContentRect.width >=0 && aContentRect.height >= 0, "Content Size < 0");
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetBounds(nsRect& aRect)
{
   nsIFrame* frame = nsnull;
   GetFrame(&frame);

   return frame->GetRect(aRect);
}

NS_IMETHODIMP
nsBox::SetBounds(nsBoxLayoutState& aLayoutState, const nsRect& aRect)
{
    NS_ASSERTION(aRect.width >=0 && aRect.height >= 0, "SetBounds Size < 0");

    nsRect rect(0,0,0,0);
    GetBounds(rect);

    nsIFrame* frame = nsnull;
    GetFrame(&frame);

    nsIPresContext* presContext = aLayoutState.GetPresContext();
    frame->SetRect(presContext, aRect);

    
    // only if the origin changed
    if ((rect.x != aRect.x) || (rect.y != aRect.y))  {
      nsIView*  view;
      frame->GetView(presContext, &view);
      if (view) {
          nsContainerFrame::PositionFrameView(presContext, frame, view);
      } else {
          nsContainerFrame::PositionChildViews(presContext, frame);
      }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsBox::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleSpacing* spacing;
        nsresult rv = frame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

  nsMargin border;
  nsMargin padding;
  GetBorder(border);
  GetPadding(padding);
  aBorderAndPadding.SizeTo(0,0,0,0);
  aBorderAndPadding += border;
  aBorderAndPadding += padding;

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetBorder(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleSpacing* spacing;
        nsresult rv = frame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

  aMargin.SizeTo(0,0,0,0);
  spacing->GetBorder(aMargin);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetPadding(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleSpacing* spacing;
        nsresult rv = frame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

  aMargin.SizeTo(0,0,0,0);
  spacing->GetPadding(aMargin);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMargin(nsMargin& aMargin)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleSpacing* spacing;
        nsresult rv = frame->GetStyleData(eStyleStruct_Spacing,
        (const nsStyleStruct*&) spacing);

  aMargin.SizeTo(0,0,0,0);
  spacing->GetMargin(aMargin);

  return NS_OK;
}

NS_IMETHODIMP 
nsBox::GetChildBox(nsIBox** aBox)
{
    *aBox = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsBox::GetNextBox(nsIBox** aBox)
{
  *aBox = mNextChild;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::SetNextBox(nsIBox* aBox)
{
  mNextChild = aBox;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::SetParentBox(nsIBox* aParent)
{
  mParentBox = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetParentBox(nsIBox** aParent)
{
  *aParent = mParentBox;
  return NS_OK;
}

NS_IMETHODIMP
nsBox::NeedsRecalc()
{
  return NS_OK;
}

nsresult
nsBox::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
    return NS_OK;
}


void
nsBox::SizeNeedsRecalc(nsSize& aSize)
{
  aSize.width  = -1;
  aSize.height = -1;
}

void
nsBox::CoordNeedsRecalc(PRInt32& aFlex)
{
  aFlex = -1;
}

PRBool
nsBox::DoesNeedRecalc(const nsSize& aSize)
{
  return (aSize.width == -1 || aSize.height == -1);
}

PRBool
nsBox::DoesNeedRecalc(nscoord aCoord)
{
  return (aCoord == -1);
}

PRBool
nsBox::GetWasCollapsed(nsBoxLayoutState& aState)
{
  nsIFrame* frame = nsnull;
  GetFrame(&frame);
  nsFrameState state;
  frame->GetFrameState(&state);
 
  return (state & NS_STATE_IS_COLLAPSED);
}

void
nsBox::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  nsIFrame* frame;
  GetFrame(&frame);
  nsFrameState state;
  frame->GetFrameState(&state);
  if (aCollapsed)
     state |= NS_STATE_IS_COLLAPSED;
  else
     state &= ~NS_STATE_IS_COLLAPSED;

  frame->SetFrameState(state);
}

NS_IMETHODIMP 
nsBox::Collapse(nsBoxLayoutState& aState)
{
 // if (GetWasCollapsed(aState))
 //   return NS_OK;

  SetWasCollapsed(aState, PR_TRUE);
  nsIFrame* frame;
  GetFrame(&frame);
  SetBounds(aState, nsRect(0,0,0,0));

  return CollapseChild(aState, frame, PR_TRUE);
}

nsresult 
nsBox::CollapseChild(nsBoxLayoutState& aState, nsIFrame* aFrame, PRBool aHide)
{
      nsIPresContext* presContext = aState.GetPresContext();

    // shrink the view
      nsIView* view = nsnull;
      aFrame->GetView(presContext, &view);

      // if we find a view stop right here. All views under it
      // will be clipped.
      if (view) {
         // already hidden? We are done.
         nsViewVisibility v;
         view->GetVisibility(v);
         //if (v == nsViewVisibility_kHide)
           //return NS_OK;

         nsCOMPtr<nsIWidget> widget;
         view->GetWidget(*getter_AddRefs(widget));
         if (aHide) {
             view->SetVisibility(nsViewVisibility_kHide);
         } else {
             view->SetVisibility(nsViewVisibility_kShow);
         }
         if (widget) {

           return NS_OK;
         }
      }

      // Trees have to collapse their scrollbars manually, since you can't
      // get to the scrollbar via the normal frame list.
      nsISelfScrollingFrame* treeFrame;
      if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsISelfScrollingFrame), (void**)&treeFrame)) && treeFrame) {
        // Tell the tree frame to collapse its scrollbar.
        treeFrame->CollapseScrollbar(presContext, aHide);
      }
      
      // collapse the child
      nsIFrame* child = nsnull;
      aFrame->FirstChild(presContext, nsnull,&child);
       
      while (nsnull != child) 
      {
         CollapseChild(aState, child, aHide);
         nsresult rv = child->GetNextSibling(&child);
         NS_ASSERTION(rv == NS_OK,"failed to get next child");
      }

      return NS_OK;
}

NS_IMETHODIMP 
nsBox::UnCollapse(nsBoxLayoutState& aState)
{
  if (!GetWasCollapsed(aState))
    return NS_OK;

  SetWasCollapsed(aState, PR_FALSE);

  return UnCollapseChild(aState, this);
}

nsresult 
nsBox::UnCollapseChild(nsBoxLayoutState& aState, nsIBox* aBox)
{
      nsIPresContext* presContext = aState.GetPresContext();
      nsIFrame* frame;
      aBox->GetFrame(&frame);
     
      // collapse the child
      nsIBox* child = nsnull;
      aBox->GetChildBox(&child);

      if (child == nsnull) {
          nsFrameState childState;
          frame->GetFrameState(&childState);
          childState |= NS_FRAME_IS_DIRTY;
          frame->SetFrameState(childState);
      } else {        
          child->GetFrame(&frame);
          nsFrameState childState;
          frame->GetFrameState(&childState);
          childState |= NS_FRAME_HAS_DIRTY_CHILDREN;
          frame->SetFrameState(childState);
       
          while (nsnull != child) 
          {
             UnCollapseChild(aState, child);
             nsresult rv = child->GetNextBox(&child);
             NS_ASSERTION(rv == NS_OK,"failed to get next child");
          }
      }

      return NS_OK;
}

NS_IMETHODIMP
nsBox::SetLayoutManager(nsIBoxLayout* aLayout)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetLayoutManager(nsIBoxLayout** aLayout)
{
  *aLayout = nsnull;
  return NS_OK;
}


NS_IMETHODIMP
nsBox::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetFlex(nsBoxLayoutState& aState, nscoord& aFlex)
{
  aFlex = 0;
  PRBool collapsed = PR_FALSE;
  nsIBox::AddCSSFlex(aState, this, aFlex);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  nsSize size(0,0);
  nsresult rv = GetPrefSize(aState, size);
  aAscent = size.height;
  return rv;
}

NS_IMETHODIMP
nsBox::IsCollapsed(nsBoxLayoutState& aState, PRBool& aCollapsed)
{
  aCollapsed = PR_FALSE;
  nsIBox::AddCSSCollapsed(aState, this, aCollapsed);

  return NS_OK;
}

NS_IMETHODIMP
nsBox::Layout(nsBoxLayoutState& aState)
{
  SyncLayout(aState);

  return NS_OK;
}

NS_IMETHODIMP 
nsBox::GetOrientation(PRBool& aIsHorizontal)
{
   nsIFrame* frame = nsnull;
   GetFrame(&frame);
   nsFrameState state;
   frame->GetFrameState(&state);
   aIsHorizontal = (state & NS_STATE_IS_HORIZONTAL);
   return NS_OK;
}

nsresult
nsBox::SyncLayout(nsBoxLayoutState& aState)
{
  /*
  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed) {
    CollapseChild(aState, this, PR_TRUE);
    return NS_OK;
  }
  */
  

  PRBool dirty = PR_FALSE;
  IsDirty(dirty);
  if (dirty || aState.GetLayoutReason() == nsBoxLayoutState::Initial)
     Redraw(aState);

  nsFrameState state;
  nsIFrame* frame;
  GetFrame(&frame);
  frame->GetFrameState(&state);
  state &= ~(NS_FRAME_HAS_DIRTY_CHILDREN | NS_FRAME_IS_DIRTY | NS_FRAME_FIRST_REFLOW);
  frame->SetFrameState(state);

  nsIPresContext* presContext = aState.GetPresContext();
  nsRect rect(0,0,0,0);
  GetBounds(rect);
  nsIView*  view;
  frame->GetView(presContext, &view);

/*
      // only if the origin changed
    if ((mX != rect.x) || (mY != rect.y)) {
        if (view) {
            nsContainerFrame::PositionFrameView(presContext, frame, view);
        } else
            nsContainerFrame::PositionChildViews(presContext, frame);

        mX = rect.x;
        mY = rect.y;
    }
*/

  if (view) {
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    nsHTMLContainerFrame::SyncFrameViewAfterReflow(
                             presContext, 
                             frame, 
                             view,
                             nsnull,
                             NS_FRAME_NO_MOVE_VIEW);

  } 


  return NS_OK;
}

NS_IMETHODIMP
nsBox::Redraw(nsBoxLayoutState& aState,
              const nsRect*   aDamageRect,
              PRBool          aImmediate)
{
  nsIPresContext* presContext = aState.GetPresContext();
  const nsHTMLReflowState* s = aState.GetReflowState();
  if (s) {
    if (s->reason != eReflowReason_Incremental)
      return NS_OK;
  }

  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  nsCOMPtr<nsIViewManager> viewManager;
  nsRect damageRect(0,0,0,0);
  if (aDamageRect)
    damageRect = *aDamageRect;
  else 
    GetContentRect(damageRect);

  // Checks to see if the damaged rect should be infalted 
  // to include the outline
  const nsStyleSpacing* spacing;
  frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
  nscoord width;
  spacing->GetOutlineWidth(width);
  if (width > 0) {
    damageRect.Inflate(width, width);
  }

  PRUint32 flags = aImmediate ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;
  nsIView* view;

  frame->GetView(presContext, &view);
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->UpdateView(view, damageRect, flags);
    
  } else {
    nsRect    rect(damageRect);
    nsPoint   offset;
  
    frame->GetOffsetFromView(presContext, offset, &view);
    NS_ASSERTION(nsnull != view, "no view");
    rect += offset;
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->UpdateView(view, rect, flags);
  }

  return NS_OK;
}

PRBool 
nsIBox::AddCSSPrefSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{
    nsIPresContext* presContext = aState.GetPresContext();
    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;
    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // add in the css min, max, pref
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.width = position->mWidth.GetCoordValue();
        widthSet = PR_TRUE;
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.height = position->mHeight.GetCoordValue();     
        heightSet = PR_TRUE;
    }
    
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));

    if (content) {
        nsAutoString value;
        PRInt32 error;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::width, value))
        {
            float p2t;
            presContext->GetScaledPixelsToTwips(&p2t);

            value.Trim("%");

            aSize.width = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
            widthSet = PR_TRUE;
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::height, value))
        {
            float p2t;
            presContext->GetScaledPixelsToTwips(&p2t);

            value.Trim("%");

            aSize.height = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
            heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}


PRBool 
nsIBox::AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{
    nsIPresContext* presContext = aState.GetPresContext();

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // add in the css min, max, pref
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min != 0) {
           aSize.width = min;
           widthSet = PR_TRUE;
        }
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min != 0) {
           aSize.height = min;
           heightSet = PR_TRUE;
        }
    }

    return (widthSet && heightSet);
}

PRBool 
nsIBox::AddCSSMaxSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize)
{  
    nsIPresContext* presContext = aState.GetPresContext();

    PRBool widthSet = PR_FALSE;
    PRBool heightSet = PR_FALSE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // add in the css min, max, pref
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.width = max;
        widthSet = PR_TRUE;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.height = max;
        heightSet = PR_TRUE;
    }


    return NS_OK;
}

PRBool 
nsIBox::AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex)
{
    PRBool flexSet = PR_FALSE;

    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    // get the flexibility
    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));

    if (content) {
        PRInt32 error;
        nsAutoString value;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
        {
            value.Trim("%");
            aFlex = value.ToInteger(&error);
            flexSet = PR_TRUE;
        }
    }

    return flexSet;
}

PRBool 
nsIBox::AddCSSCollapsed(nsBoxLayoutState& aState, nsIBox* aBox, PRBool& aCollapsed)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);
  const nsStyleDisplay* disp;
  frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)disp));
  aCollapsed = disp->mVisible == NS_STYLE_VISIBILITY_COLLAPSE;
  return PR_TRUE;
}

void
nsBox::AddBorderAndPadding(nsSize& aSize)
{
  AddBorderAndPadding(this, aSize);
}

void
nsBox::AddMargin(nsSize& aSize)
{
  AddMargin(this, aSize);
}

void
nsBox::AddInset(nsSize& aSize)
{
  AddInset(this, aSize);
}

void
nsBox::AddBorderAndPadding(nsIBox* aBox, nsSize& aSize)
{
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);
  AddMargin(aSize, borderPadding);
}

void
nsBox::AddMargin(nsIBox* aChild, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aChild->GetMargin(margin);
  AddMargin(aSize, margin);
}

void
nsBox::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  if (aSize.width != NS_INTRINSICSIZE)
    aSize.width += aMargin.left + aMargin.right;

  if (aSize.height != NS_INTRINSICSIZE)
     aSize.height += aMargin.top + aMargin.bottom;
}

void
nsBox::AddInset(nsIBox* aBox, nsSize& aSize)
{
  nsMargin margin(0,0,0,0);
  aBox->GetInset(margin);
  AddMargin(aSize, margin);
}

void
nsBox::BoundsCheck(nsSize& aMinSize, nsSize& aPrefSize, nsSize& aMaxSize)
{
  
   if (aMinSize.width > aMaxSize.width)
       aMinSize.width = aMaxSize.width;

   if (aMinSize.height > aMaxSize.height)
       aMinSize.height = aMaxSize.height;


/*
   if (aMinSize.width > aMaxSize.width)
       aMaxSize.width = aMinSize.width;

   if (aMinSize.height > aMaxSize.height)
       aMaxSize.height = aMinSize.height;
*/

   if (aPrefSize.width > aMaxSize.width)
       aPrefSize.width = aMaxSize.width;

   if (aPrefSize.height > aMaxSize.height)
       aPrefSize.height = aMaxSize.height;

   if (aPrefSize.width < aMinSize.width)
       aPrefSize.width = aMinSize.width;

   if (aPrefSize.height < aMinSize.height)
       aPrefSize.height = aMinSize.height;
}

NS_IMETHODIMP
nsBox::GetDebugBoxAt( const nsPoint& aPoint,
                      nsIBox**     aBox)
{
  nsPoint tmp;

  nsRect rect;
  GetBounds(rect);

  if (!rect.Contains(aPoint))
    return NS_ERROR_FAILURE;

  nsIBox* child = nsnull;
  nsIBox* hit = nsnull;
  GetChildBox(&child);

  *aBox = nsnull;
  tmp.MoveTo(aPoint.x - rect.x, aPoint.y - rect.y);
  while (nsnull != child) {
    nsresult rv = child->GetDebugBoxAt(tmp, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aBox = hit;
    }
    child->GetNextBox(&child);
  }

  // found a child
  if (*aBox) {
    return NS_OK;
  }

 // see if it is in our in our insets
  nsMargin m;
  GetBorderAndPadding(m);
  rect.Deflate(m);
  if (rect.Contains(aPoint)) {
    GetInset(m);
    rect.Deflate(m);
    if (!rect.Contains(aPoint)) {
      *aBox = this;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsBox::GetDebug(PRBool& aDebug)
{
  aDebug = PR_FALSE;
  return NS_OK;
}

// nsISupports
NS_IMETHODIMP_(nsrefcnt) 
nsBox::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsBox::Release(void)
{
    return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsBox)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

