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
#include "nsIDOMUIEvent.h"
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

nscoord            nsSliderFrame::gChange = 0;

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

nsresult
NS_NewSliderFrame ( nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSliderFrame* it = new nsSliderFrame();
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSliderFrame

nsSliderFrame::nsSliderFrame()
:mScrollbarListener(nsnull), mCurPos(0)
{
}

// stop timer
nsSliderFrame::~nsSliderFrame()
{

}

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

/**
 * Anonymous interface
 */
NS_IMETHODIMP
nsSliderFrame::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  // supply anonymous content if there is no content
  PRInt32 count = 0;
  mContent->ChildCount(count); 
  if (count == 0) 
  {
    nsCOMPtr<nsIContent> content;
    NS_CreateAnonymousNode(mContent, nsXULAtoms::thumb, nsXULAtoms::nameSpaceID, content);
    content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, "horizontal", PR_FALSE);

    // if we are not in a scrollbar default our thumbs flex to be
    // flexible.
    nsIContent* scrollbar = GetScrollBar();

    if (scrollbar) { 
      content->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, "100%", PR_FALSE);
      nsString value;
      if (NS_CONTENT_ATTR_HAS_VALUE == scrollbar->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value))
         mContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value, PR_FALSE);
    }

    aAnonymousChildren.AppendElement(content);
  }

  return NS_OK;
}



NS_IMETHODIMP
nsSliderFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view;
  GetView(&view);
  view->SetContentTransparency(PR_TRUE);
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
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);
  // if the current position changes
  if (aAttribute == nsXULAtoms::curpos) {
     CurrentPositionChanged(aPresContext);
  } else if (aAttribute == nsXULAtoms::maxpos || 
             aAttribute == nsXULAtoms::pageincrement ||
             aAttribute == nsXULAtoms::increment) {
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
  
      nsCOMPtr<nsIReflowCommand> reflowCmd;
      nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), this,
                                            nsIReflowCommand::StyleChanged);
      if (NS_SUCCEEDED(rv)) 
        shell->AppendReflowCommand(reflowCmd);
  }

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::Paint(nsIPresContext& aPresContext,
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
    if (disp->mVisible) {
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

void
nsSliderFrame::ReflowThumb(nsIPresContext&   aPresContext,
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

    nsMargin margin(0,0,0,0);
    spacing->GetMargin(margin);
    nsMargin border(0,0,0,0);
    spacing->GetBorderPadding(border);
    nsMargin total = margin + border;

    if (thumbReflowState.mComputedWidth != NS_INTRINSICSIZE)
       thumbReflowState.mComputedWidth -= total.left + total.right;

    if (thumbReflowState.mComputedHeight != NS_INTRINSICSIZE)
       thumbReflowState.mComputedHeight -= total.top + total.bottom;

    ReflowChild(thumbFrame, aPresContext, aDesiredSize, thumbReflowState, aStatus);
    
    // add the margin back in
    aDesiredSize.width += margin.left + margin.right;
    aDesiredSize.height += margin.top + margin.bottom;
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
nsSliderFrame::Reflow(nsIPresContext&   aPresContext,
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
  aPresContext.GetScaledPixelsToTwips(&p2t);
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

  thumbFrame->SetRect(thumbRect);

  // add in our border
  aDesiredSize.width += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  
  return NS_OK;
}


NS_IMETHODIMP
nsSliderFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  nsIContent* scrollbar = GetScrollBar();

  PRBool isHorizontal = IsHorizontal(scrollbar);

  if (isDraggingThumb())
  {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE: {
       // convert coord to pixels
      nscoord pos = isHorizontal ? aEvent->point.x : aEvent->point.y;

       // mDragStartPx is in pixels and is in our client areas coordinate system. 
       // so we need to first convert it so twips and then get it into our coordinate system.

       // convert start to twips
       nscoord startpx = mDragStartPx;
              
       float p2t;
       aPresContext.GetScaledPixelsToTwips(&p2t);
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
          parent->GetView(&view);
          if (view) {
            nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
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
      nsIFrame* thumbFrame = mFrames.FirstChild();
      AddListener();
      DragThumb(PR_FALSE);
    }
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
void
nsSliderFrame::CurrentPositionChanged(nsIPresContext* aPresContext)
{
    nsIContent* scrollbar = GetScrollBar();

    PRBool isHorizontal = IsHorizontal(scrollbar);

    // get the current position
    PRInt32 curpos = GetCurrentPosition(scrollbar);

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

    nsMargin borderPadding(0,0,0,0);
    spacing->GetBorderPadding(borderPadding);
    
    // figure out the new rect
    nsRect newThumbRect(thumbRect);

    if (isHorizontal)
       newThumbRect.x = borderPadding.left + nscoord(float(curpospx)*mRatio);
    else
       newThumbRect.y = borderPadding.top + nscoord(float(curpospx)*mRatio);

    // set the rect
    thumbFrame->SetRect(newThumbRect);
    
    // figure out the union of the rect so we know what to redraw
    nsRect changeRect;
    changeRect.UnionRect(thumbRect, newThumbRect);

    // redraw just the change
    Invalidate(changeRect, PR_TRUE);

    if (mScrollbarListener)
      mScrollbarListener->PositionChanged(*aPresContext, mCurPos, curpos);
    
    mCurPos = curpos;

}

void
nsSliderFrame::SetCurrentPosition(nsIContent* scrollbar, nsIFrame* aThumbFrame, nscoord newpos)
{
  
   // get our current position and max position from our content node
  PRInt32 curpos = GetCurrentPosition(scrollbar);
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

 // printf("Current Pos=%s\n",ch);
  
}

NS_IMETHODIMP  nsSliderFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                             nsIFrame**     aFrame)
{   
  if (isDraggingThumb())
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

  //return nsHTMLContainerFrame::GetFrameForPoint(aPoint, aFrame);
}


static NS_DEFINE_IID(kIDOMMouseListenerIID,     NS_IDOMMOUSELISTENER_IID);

NS_IMETHODIMP
nsSliderFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  AddListener();

  return r;
}


NS_IMETHODIMP
nsSliderFrame::RemoveFrame(nsIPresContext& aPresContext,
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
nsSliderFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return nsHTMLContainerFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList); 
}

NS_IMETHODIMP
nsSliderFrame::AppendFrames(nsIPresContext& aPresContext,
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

  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(aMouseEvent));

  RemoveListener();
  DragThumb(PR_TRUE);
  PRInt32 c = 0;
  if (isHorizontal)
     uiEvent->GetClientX(&c);
  else
     uiEvent->GetClientY(&c);

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
nsSliderFrame :: DragThumb(PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = nsnull;
  GetView(&view);
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
nsSliderFrame :: isDraggingThumb()
{
    // get its view
  nsIView* view = nsnull;
  GetView(&view);
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

  reciever->AddEventListenerByIID(this, kIDOMMouseListenerIID);
}

void
nsSliderFrame::RemoveListener()
{
  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsCOMPtr<nsIContent> content;
  thumbFrame->GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->RemoveEventListenerByIID(this,kIDOMMouseListenerIID);
}

static NS_DEFINE_IID(kITimerCallbackIID, NS_ITIMERCALLBACK_IID);


NS_IMETHODIMP nsSliderFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  } else if (aIID.Equals(kIDOMMouseListenerIID)) {                                         
    *aInstancePtr = (void*)(nsIDOMMouseListener*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  } else if (aIID.Equals(kITimerCallbackIID)) {                                         
    *aInstancePtr = (void*)(nsITimerCallback*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                     
}

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
nsSliderFrame::HandlePress(nsIPresContext& aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus&  aEventStatus)
{
  nsIContent* scrollbar = GetScrollBar();
  PRBool isHorizontal = IsHorizontal(scrollbar);

  nsIFrame* thumbFrame = mFrames.FirstChild();
  nsRect thumbRect;
  thumbFrame->GetRect(thumbRect);

    nscoord change = 1;
    
    if ((isHorizontal && aEvent->point.x < thumbRect.x) || (!isHorizontal && aEvent->point.y < thumbRect.y)) 
        change = -1;

    gChange = change;
    nsRepeatService::GetInstance()->Start(this);
    PageUpDown(thumbFrame, change);

  return NS_OK;
}

NS_IMETHODIMP 
nsSliderFrame::HandleRelease(nsIPresContext& aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus&  aEventStatus)
{
  nsRepeatService::GetInstance()->Stop();

  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::Destroy(nsIPresContext& aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  return nsHTMLContainerFrame::Destroy(aPresContext);
}


void 
nsSliderFrame::SetScrollbarListener(nsIScrollbarListener* aListener)
{
  // Don't addref/release this, since it's actually a frame.
  mScrollbarListener = aListener;
}

void nsSliderFrame::Notify(nsITimer *timer)
{
  nsIFrame* thumbFrame = mFrames.FirstChild();
  PageUpDown(thumbFrame, gChange);
}

class nsThumbFrame : public nsTitledButtonFrame
{
public:

  friend nsresult NS_NewThumbFrame(nsIFrame** aNewFrame);

  NS_IMETHOD HandlePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus) { return NS_OK; }

  NS_IMETHOD HandleMultiplePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleDrag(nsIPresContext& aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus&  aEventStatus)  { return NS_OK; }

  NS_IMETHOD HandleRelease(nsIPresContext& aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus&  aEventStatus)  { return NS_OK; }

};


nsresult
NS_NewThumbFrame ( nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsThumbFrame* it = new nsThumbFrame();
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSliderFrame




