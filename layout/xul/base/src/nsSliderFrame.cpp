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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsSliderFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMEventReceiver.h"
#include "nsIViewManager.h"
#include "nsIDOMMouseEvent.h"
#include "nsDocument.h"
#include "nsTitledButtonFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIScrollbarListener.h"
#include "nsISupportsArray.h"
#include "nsIXMLContent.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollableView.h"
#include "nsRepeatService.h"

#define DEBUG_SLIDER PR_FALSE


nsresult
NS_NewSliderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSliderFrame* it = new (aPresShell) nsSliderFrame();
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSliderFrame

nsSliderFrame::nsSliderFrame()
: mCurPos(0), mScrollbarListener(nsnull),mChange(0)
{
}

// stop timer
nsSliderFrame::~nsSliderFrame()
{
   mRedrawImmediate = PR_FALSE;
}

NS_IMETHODIMP
nsSliderFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view;
  GetView(aPresContext, &view);
  view->SetContentTransparency(PR_TRUE);
  // XXX Hack
  mPresContext = aPresContext;
  return rv;
}

PRInt32
nsSliderFrame::GetCurrentPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsXULAtoms::curpos, 0);
}

PRInt32
nsSliderFrame::GetMaxPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsXULAtoms::maxpos, 100);
}

PRInt32
nsSliderFrame::GetIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsXULAtoms::increment, 1);
}


PRInt32
nsSliderFrame::GetPageIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsXULAtoms::pageincrement, 10);
}

PRInt32
nsSliderFrame::GetIntegerAttribute(nsIContent* content, nsIAtom* atom, PRInt32 defaultValue)
{
    nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, atom, value))
    {
      PRInt32 error;

      // convert it to an integer
      defaultValue = value.ToInteger(&error);
    }

    return defaultValue;
}


NS_IMETHODIMP
nsSliderFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  // if the current position changes
  if (aAttribute == nsXULAtoms::curpos) {
     rv = CurrentPositionChanged(aPresContext);
     NS_ASSERTION(NS_SUCCEEDED(rv), "failed to change position");
     if (NS_FAILED(rv))
        return rv;
  } else if (aAttribute == nsXULAtoms::maxpos) {
      // bounds check it.

      nsIContent* scrollbar = GetScrollBar();
      PRInt32 current = GetCurrentPosition(scrollbar);      
      PRInt32 max = GetMaxPosition(scrollbar);
      if (current < 0 || current > max)
      {
          if (current < 0)
              current = 0;
          else if (current > max) 
              current = max;

          char ch[100];
          sprintf(ch,"%d", current);
 
          // set the new position but don't notify anyone. We already know
          scrollbar->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, nsString(ch), PR_FALSE);
      }
  }
  
  if ((aHint != NS_STYLE_HINT_REFLOW) && 
             (aAttribute == nsXULAtoms::maxpos || 
             aAttribute == nsXULAtoms::pageincrement ||
             aAttribute == nsXULAtoms::increment)) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
 
      /*
      nsCOMPtr<nsIReflowCommand> reflowCmd;
      rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                                   nsIReflowCommand::StyleChanged);
      if (NS_SUCCEEDED(rv)) 
        shell->AppendReflowCommand(reflowCmd);
      */

      mState |= NS_FRAME_IS_DIRTY;
      return mParent->ReflowDirtyChild(shell, this);
  }

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  // if we are too small to have a thumb don't paint it.
  nsIFrame* thumbFrame = mFrames.FirstChild();
  NS_ASSERTION(thumbFrame,"Slider does not have a thumb!!!!");

  nsSize size(0,0);
  thumbFrame->GetSize(size);
  if (mRect.width < size.width || mRect.height < size.height)
  {
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->IsVisibleOrCollapsed()) {
      const nsStyleColor* myColor = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);
      nsRect rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *myColor, *mySpacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, rect, *mySpacing, mStyleContext, 0);
      }
    }
    return NS_OK;
  }
  
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsresult
nsSliderFrame::ReflowThumb(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus,
                     nsIFrame* thumbFrame,
                     nsSize available,
                     nsSize computed)
{
    nsHTMLReflowState thumbReflowState(aPresContext, aReflowState,
                                       thumbFrame, available);

    // always give the thumb as much size as it needs
    thumbReflowState.mComputedWidth = computed.width;
    thumbReflowState.mComputedHeight = computed.height;

    // subtract out the childs margin and border if computed
    const nsStyleSpacing* spacing;
    nsresult rv = thumbFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;


    nsMargin margin(0,0,0,0);
    spacing->GetMargin(margin);
    nsMargin border(0,0,0,0);
    spacing->GetBorderPadding(border);
    nsMargin total = margin + border;

    if (thumbReflowState.mComputedWidth != NS_INTRINSICSIZE)
       thumbReflowState.mComputedWidth -= total.left + total.right;

    if (thumbReflowState.mComputedHeight != NS_INTRINSICSIZE)
       thumbReflowState.mComputedHeight -= total.top + total.bottom;

    ReflowChild(thumbFrame, aPresContext, aDesiredSize, thumbReflowState,
                0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
    thumbFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    
    // add the margin back in
    aDesiredSize.width += margin.left + margin.right;
    aDesiredSize.height += margin.top + margin.bottom;

    return NS_OK;
}

PRBool 
nsSliderFrame::IsHorizontal(nsIContent* scrollbar)
{
  PRBool isHorizontal = PR_TRUE;
  nsString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == scrollbar->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value))
  {
    if (value=="vertical")
      isHorizontal = PR_FALSE;
  }

  return isHorizontal;
}

NS_IMETHODIMP
nsSliderFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  nsIContent* scrollbar = GetScrollBar();

  PRBool isHorizontal = IsHorizontal(scrollbar);

  // flow our thumb with our computed width and its intrinsic height
  nsIFrame* thumbFrame = mFrames.FirstChild();
  NS_ASSERTION(thumbFrame,"Slider does not have a thumb!!!!");

  nsHTMLReflowMetrics thumbSize(nsnull);

  nsSize availableSize(isHorizontal ? NS_INTRINSICSIZE: aReflowState.mComputedWidth, isHorizontal ? aReflowState.mComputedHeight : NS_INTRINSICSIZE);
  nsSize computedSize(isHorizontal ? NS_INTRINSICSIZE: aReflowState.mComputedWidth, isHorizontal ? aReflowState.mComputedHeight : NS_INTRINSICSIZE);
     
  ReflowThumb(aPresContext, thumbSize, aReflowState, aStatus, thumbFrame, availableSize, computedSize);
 
  // get our current position and max position from our content node
  PRInt32 curpospx = GetCurrentPosition(scrollbar);
  PRInt32 maxpospx = GetMaxPosition(scrollbar);

  if (curpospx < 0)
     curpospx = 0;
  else if (curpospx > maxpospx)
     curpospx = maxpospx;

  // if the computed height we are given is intrinsic then set it to some default height
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) 
    aDesiredSize.height = isHorizontal ? thumbSize.height : 200*onePixel;
  else {
    aDesiredSize.height = aReflowState.mComputedHeight;
   // if (aDesiredSize.height < thumbSize.height)
   //   aDesiredSize.height = thumbSize.height;
  }

  // set the width to the computed or if intrinsic then the width of the thumb.
  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE) 
    aDesiredSize.width = isHorizontal ? 200*onePixel : thumbSize.width;
  else {
    aDesiredSize.width = aReflowState.mComputedWidth;
   // if (aDesiredSize.width < thumbSize.width)
   //   aDesiredSize.width = thumbSize.width;
  }

  // get max pos in twips
  nscoord maxpos = maxpospx*onePixel;

  // get our maxpos in twips. This is the space we have left over in the scrollbar
  // after the height of the thumb has been removed
  nscoord& desiredcoord = isHorizontal ? aDesiredSize.width : aDesiredSize.height;
  nscoord& thumbcoord = isHorizontal ? thumbSize.width : thumbSize.height;

  nscoord ourmaxpos = desiredcoord; 

  mRatio = float(ourmaxpos)/float(maxpos + ourmaxpos);

  nscoord thumbsize = nscoord(ourmaxpos * mRatio);
  // if there is more room than the thumb need stretch the
  // thumb
  if (thumbsize > thumbcoord) {
    // if the thumb is flexible make the thumb bigger.
    nsCOMPtr<nsIContent> content;
    thumbFrame->GetContent(getter_AddRefs(content));

    PRInt32 error;
    nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
        value.Trim("%");
        // convert to a percent.
        if (value.ToFloat(&error) > 0.0) {
           if (isHorizontal)
              computedSize.width = thumbsize;
           else
              computedSize.height = thumbsize;

           ReflowThumb(aPresContext, thumbSize, aReflowState, aStatus, thumbFrame, availableSize, computedSize);
        }
    }
  } else {
    ourmaxpos -= thumbcoord;
    mRatio = float(ourmaxpos)/float(maxpos);
  }

  // get our border
  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;

  nscoord curpos = curpospx*onePixel;

  // set the thumbs y coord to be the current pos * the ratio.
  nscoord pos = nscoord(float(curpos)*mRatio);
  nsRect thumbRect(borderPadding.left, borderPadding.top, thumbSize.width, thumbSize.height);
  
  if (isHorizontal)
    thumbRect.x += pos;
  else
    thumbRect.y += pos;

  nsIView*  view;
  thumbFrame->SetRect(aPresContext, thumbRect);
  thumbFrame->GetView(aPresContext, &view);
  if (view) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, thumbFrame,
                                               view, nsnull);
  }

  // add in our border
  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (DEBUG_SLIDER) {
     PRInt32 c = GetCurrentPosition(scrollbar);
     PRInt32 m = GetMaxPosition(scrollbar);
     printf("Current=%d, max=%d\n",c,m);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsSliderFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  nsIContent* scrollbar = GetScrollBar();

  PRBool isHorizontal = IsHorizontal(scrollbar);

  if (isDraggingThumb(aPresContext))
  {
      // we want to draw immediately if the user doing it directly with the
      // mouse that makes redrawing much faster.
      mRedrawImmediate = PR_TRUE;

    switch (aEvent->message) {
    case NS_MOUSE_MOVE: {
       // convert coord to pixels
      nscoord pos = isHorizontal ? aEvent->point.x : aEvent->point.y;

       // mDragStartPx is in pixels and is in our client areas coordinate system. 
       // so we need to first convert it so twips and then get it into our coordinate system.

       // convert start to twips
       nscoord startpx = mDragStartPx;
              
       float p2t;
       aPresContext->GetScaledPixelsToTwips(&p2t);
       nscoord onePixel = NSIntPixelsToTwips(1, p2t);
       nscoord start = startpx*onePixel;

       nsIFrame* thumbFrame = mFrames.FirstChild();


       // get it into our coordintate system by subtracting our parents offsets.
       nsIFrame* parent = this;
       while(parent != nsnull)
       {
          // if we hit a scrollable view make sure we take into account
          // how much we are scrolled.
          nsIScrollableView* scrollingView;
          nsIView*           view;
          parent->GetView(aPresContext, &view);
          if (view) {
            nsresult result = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
            if (NS_SUCCEEDED(result)) {
                nscoord xoff = 0;
                nscoord yoff = 0;
                scrollingView->GetScrollPosition(xoff, yoff);
                isHorizontal ? start += xoff : start += yoff;         
            }
          }
       
         nsRect r;
         parent->GetRect(r);
         isHorizontal ? start -= r.x : start -= r.y;
         parent->GetParent(&parent);
       }

      //printf("Translated to start=%d\n",start);

       start -= mThumbStart;

       // take our current position and substract the start location
       pos -= start;

       // convert to pixels
       nscoord pospx = pos/onePixel;

       // convert to our internal coordinate system
       pospx = nscoord(pospx/mRatio);

       // set it
       SetCurrentPosition(scrollbar, thumbFrame, pospx);

    } 
    break;

    case NS_MOUSE_RIGHT_BUTTON_UP:
    case NS_MOUSE_LEFT_BUTTON_UP:
       // stop capturing
      //printf("stop capturing\n");
      AddListener();
      DragThumb(aPresContext, PR_FALSE);
    }

    // we want to draw immediately if the user doing it directly with the
    // mouse that makes redrawing much faster. Switch it back now.
    mRedrawImmediate = PR_FALSE;

    //return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    return NS_OK;
  }

  // XXX hack until handle release is actually called in nsframe.
  if (aEvent->message == NS_MOUSE_EXIT|| aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP || aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
     HandleRelease(aPresContext, aEvent, aEventStatus);

  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}



nsIContent*
nsSliderFrame::GetScrollBar()
{
  // if we are in a scrollbar then return the scrollbar's content node
  // if we are not then return ours.
   nsIFrame* scrollbar;
   nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::scrollbar, this, scrollbar);

   if (scrollbar == nsnull)
       scrollbar = this;

   nsCOMPtr<nsIContent> content;
   scrollbar->GetContent(getter_AddRefs(content));

   return content;
}

void
nsSliderFrame::PageUpDown(nsIFrame* aThumbFrame, nscoord change)
{ 
  // on a page up or down get our page increment. We get this by getting the scrollbar we are in and
  // asking it for the current position and the page increment. If we are not in a scrollbar we will
  // get the values from our own node.
  nsIContent* scrollbar = GetScrollBar();
  
  if (mScrollbarListener)
    mScrollbarListener->PagedUpDown(); // Let the listener decide our increment.

  nscoord pageIncrement = GetPageIncrement(scrollbar);
  PRInt32 curpos = GetCurrentPosition(scrollbar);
  SetCurrentPosition(scrollbar, aThumbFrame, curpos + change*pageIncrement);
}

// called when the current position changed and we need to update the thumb's location
nsresult
nsSliderFrame::CurrentPositionChanged(nsIPresContext* aPresContext)
{
    nsIContent* scrollbar = GetScrollBar();

    PRBool isHorizontal = IsHorizontal(scrollbar);

    // get the current position
    PRInt32 curpos = GetCurrentPosition(scrollbar);

    // do nothing if the position did not change
    if (mCurPos == curpos)
        return NS_OK;

    // get our current position and max position from our content node
    PRInt32 maxpos = GetMaxPosition(scrollbar);

    if (curpos < 0)
      curpos = 0;
         else if (curpos > maxpos)
      curpos = maxpos;

    // convert to pixels
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nscoord curpospx = curpos*onePixel;

    // get the thumb's rect
    nsIFrame* thumbFrame = mFrames.FirstChild();
    nsRect thumbRect;
    thumbFrame->GetRect(thumbRect);

    // get our border and padding
    const nsStyleSpacing* spacing;
    nsresult rv = GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;

    nsMargin borderPadding(0,0,0,0);
    spacing->GetBorderPadding(borderPadding);
    
    // figure out the new rect
    nsRect newThumbRect(thumbRect);

    if (isHorizontal)
       newThumbRect.x = borderPadding.left + nscoord(float(curpospx)*mRatio);
    else
       newThumbRect.y = borderPadding.top + nscoord(float(curpospx)*mRatio);

    // set the rect
    thumbFrame->SetRect(aPresContext, newThumbRect);
    
    // figure out the union of the rect so we know what to redraw
    nsRect changeRect;
    changeRect.UnionRect(thumbRect, newThumbRect);

    // redraw just the change
    Invalidate(aPresContext, changeRect, mRedrawImmediate);

    if (mScrollbarListener)
      mScrollbarListener->PositionChanged(aPresContext, mCurPos, curpos);
    
    mCurPos = curpos;

    return NS_OK;
}

void
nsSliderFrame::SetCurrentPosition(nsIContent* scrollbar, nsIFrame* aThumbFrame, nscoord newpos)
{
  
   // get our current position and max position from our content node
  PRInt32 maxpos = GetMaxPosition(scrollbar);

  // get the new position and make sure it is in bounds
  if (newpos > maxpos)
      newpos = maxpos;
  else if (newpos < 0) 
      newpos = 0;

  char ch[100];
  sprintf(ch,"%d", newpos);

  // set the new position
  scrollbar->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, nsString(ch), PR_TRUE);

  if (DEBUG_SLIDER)
     printf("Current Pos=%s\n",ch);
  
}

NS_IMETHODIMP  nsSliderFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint, 
                                             nsIFrame**     aFrame)
{   
  if (isDraggingThumb(aPresContext))
  {
    *aFrame = this;
    return NS_OK;
  }

  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsRect thumbRect;
  thumbFrame->GetRect(thumbRect);

  if (thumbRect.Contains(aPoint)) {
     *aFrame = thumbFrame;
  } else {
  // always return us
     *aFrame = this;
  }

  return NS_OK;

  //return nsHTMLContainerFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);
}



NS_IMETHODIMP
nsSliderFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  AddListener();

  return r;
}


NS_IMETHODIMP
nsSliderFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
      // remove the child frame
      nsresult rv = nsHTMLContainerFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
      mFrames.DestroyFrame(aPresContext, aOldFrame);
      return rv;
}

NS_IMETHODIMP
nsSliderFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return nsHTMLContainerFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList); 
}

NS_IMETHODIMP
nsSliderFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
   mFrames.AppendFrames(nsnull, aFrameList); 
   return nsHTMLContainerFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
}


nsresult
nsSliderFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  //printf("Begin dragging\n");
  
  nsIContent* scrollbar = GetScrollBar();
  PRBool isHorizontal = IsHorizontal(scrollbar);

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));

  RemoveListener();
  DragThumb(mPresContext, PR_TRUE);
  PRInt32 c = 0;
  if (isHorizontal)
     mouseEvent->GetClientX(&c);
  else
     mouseEvent->GetClientY(&c);

  mDragStartPx = c;
  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsRect thumbRect;
  thumbFrame->GetRect(thumbRect);

  if (isHorizontal)
     mThumbStart = thumbRect.x;
  else
     mThumbStart = thumbRect.y;
     
  //printf("Pressed mDragStartPx=%d\n",mDragStartPx);
  
  return NS_OK;
}

nsresult
nsSliderFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
 // printf("Finish dragging\n");

  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame :: DragThumb(nsIPresContext* aPresContext, PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  GetView(aPresContext, &view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
  }

  return NS_OK;
}

PRBool
nsSliderFrame :: isDraggingThumb(nsIPresContext* aPresContext)
{
    // get its view
  nsIView* view = nsnull;
  GetView(aPresContext, &view);
  nsCOMPtr<nsIViewManager> viewMan;
  
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
}

void
nsSliderFrame::AddListener()
{
  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsCOMPtr<nsIContent> content;
  thumbFrame->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->AddEventListenerByIID(this,NS_GET_IID(nsIDOMMouseListener));
}

void
nsSliderFrame::RemoveListener()
{
  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsCOMPtr<nsIContent> content;
  thumbFrame->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->RemoveEventListenerByIID(this,NS_GET_IID(nsIDOMMouseListener));
}


NS_INTERFACE_MAP_BEGIN(nsSliderFrame)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsHTMLContainerFrame)


NS_IMETHODIMP_(nsrefcnt) 
nsSliderFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsSliderFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::HandlePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  nsIContent* scrollbar = GetScrollBar();
  PRBool isHorizontal = IsHorizontal(scrollbar);

  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsRect thumbRect;
  thumbFrame->GetRect(thumbRect);

    nscoord change = 1;
    
    if ((isHorizontal && aEvent->point.x < thumbRect.x) || (!isHorizontal && aEvent->point.y < thumbRect.y)) 
        change = -1;

    mChange = change;
    mClickPoint = aEvent->point;
    PageUpDown(thumbFrame, change);
    nsRepeatService::GetInstance()->Start(this);

  return NS_OK;
}

NS_IMETHODIMP 
nsSliderFrame::HandleRelease(nsIPresContext* aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  nsRepeatService::GetInstance()->Stop();

  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::Destroy(nsIPresContext* aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();

  // XXX: HACK!  WORKAROUND FOR BUG 21571
  /*
    the root cause of the crash is that nsSliderFrame implements nsIDOMEventListener and passes 
    itself to nsEventListenerManager::AddEventListener().  
    nsEventListenerManager::AddEventListener() assumes it is passed an 
    object that is governed by ref-counting.  But nsSliderFrame is **not** a 
    ref-counted object, and it's lifetime is implicitly controlled by the lifetime 
    of the frame model.  By passing itself to 
    nsEventListenerManager::AddEventListener(), the slider is passing in a pointer 
    that can be yanked out from underneath the event listener manager.  When the 
    event listener manager is destroyed, it correctly tries to clean up any objects 
    still under it's control, including the already-deleted slider.

    This bug is only evident when a slider is the last focused object before deletion.  
    Calling RemoveListener() removes *this* from nsEventListenerManager,
    removing the worst symptom of the bug.

    The real solution is to create a ref-counted listener object for the 
    slider to hand off to nsEventListenerManager::AddEventListener().
    Part of that fix should be removing nsSliderFrame::AddRef and
    nsSliderFrame::Release, which were masking this problem.  Without those
    methods, we would have gotten assertions as soon as the first slider was passed
    to any interface that tried to refcount it.
  */
  RemoveListener();   // remove this line when 21571 is fixed properly

  // call base class Destroy()
  return nsHTMLContainerFrame::Destroy(aPresContext);
}


void 
nsSliderFrame::SetScrollbarListener(nsIScrollbarListener* aListener)
{
  // Don't addref/release this, since it's actually a frame.
  mScrollbarListener = aListener;
}

NS_IMETHODIMP_(void) nsSliderFrame::Notify(nsITimer *timer)
{ 
    PRBool stop = PR_FALSE;

    nsIFrame* thumbFrame = mFrames.FirstChild();
    nsRect thumbRect;
    thumbFrame->GetRect(thumbRect);

    nsIContent* scrollbar = GetScrollBar();
    PRBool isHorizontal = IsHorizontal(scrollbar);

    // see if the thumb has moved passed our original click point.
    // if it has we want to stop.
    if (isHorizontal) {
        if (mChange < 0) {
            if (thumbRect.x < mClickPoint.x) 
                stop = PR_TRUE;
        } else {
            if (thumbRect.x + thumbRect.width > mClickPoint.x)
                stop = PR_TRUE;
        }
    } else {
         if (mChange < 0) {
            if (thumbRect.y < mClickPoint.y) 
                stop = PR_TRUE;
        } else {
            if (thumbRect.y + thumbRect.height > mClickPoint.y)
                stop = PR_TRUE;
        }
    }


    if (stop) {
       nsRepeatService::GetInstance()->Stop();
    } else {
      PageUpDown(thumbFrame, mChange);
    }
}
