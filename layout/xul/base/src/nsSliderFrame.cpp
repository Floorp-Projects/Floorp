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
 *   Dean Tessman <dean_tessman@hotmail.com>
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

#include "nsSliderFrame.h"
#include "nsStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMEventReceiver.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDocument.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIScrollbarListener.h"
#include "nsIScrollbarMediator.h"
#include "nsIScrollbarFrame.h"
#include "nsISupportsArray.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsRepeatService.h"
#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIServiceManager.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"

PRBool nsSliderFrame::gMiddlePref = PR_FALSE;
PRInt32 nsSliderFrame::gSnapMultiplier = 6;

// Turn this on if you want to debug slider frames.
#undef DEBUG_SLIDER

static already_AddRefed<nsIContent>
GetContentOfBox(nsIBox *aBox)
{
  nsIFrame *frame;
  aBox->GetFrame(&frame);
  nsIContent* content = frame->GetContent();
  NS_IF_ADDREF(content);
  return content;
}

nsresult
NS_NewSliderFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSliderFrame* it = new (aPresShell) nsSliderFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;

} // NS_NewSliderFrame

nsSliderFrame::nsSliderFrame(nsIPresShell* aPresShell):nsBoxFrame(aPresShell),
 mCurPos(0), mScrollbarListener(nsnull),mChange(0), mMediator(nsnull)
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
                    nsStyleContext*  aContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  static PRBool gotPrefs = PR_FALSE;
  if (!gotPrefs) {
    gotPrefs = PR_TRUE;

    gMiddlePref = nsContentUtils::GetBoolPref("middlemouse.scrollbarPosition");
    gSnapMultiplier = nsContentUtils::GetIntPref("slider.snapMultiplier");
  }

  CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view = GetView();
  view->GetViewManager()->SetViewContentTransparency(view, PR_TRUE);
  return rv;
}

NS_IMETHODIMP
nsSliderFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  PRInt32 start = GetChildCount();
  if (start == 0)
    RemoveListener();

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  PRInt32 start = GetChildCount();
  nsresult rv = nsBoxFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList);
  if (start == 0)
    AddListener();

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  // if we have no children and on was added then make sure we add the
  // listener
  PRInt32 start = GetChildCount();
  nsresult rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList);
  if (start == 0)
    AddListener();

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
    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, atom, value))
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
                               PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                             aNameSpaceID, aAttribute,
                                             aModType);
  // if the current position changes
  if (aAttribute == nsXULAtoms::curpos) {
     rv = CurrentPositionChanged(aPresContext);
     NS_ASSERTION(NS_SUCCEEDED(rv), "failed to change position");
     if (NS_FAILED(rv))
        return rv;
  } else if (aAttribute == nsXULAtoms::maxpos) {
      // bounds check it.

      nsIBox* scrollbarBox = GetScrollbar();
      nsCOMPtr<nsIContent> scrollbar;
      scrollbar = GetContentOfBox(scrollbarBox);
      PRInt32 current = GetCurrentPosition(scrollbar);
      PRInt32 max = GetMaxPosition(scrollbar);
      if (current < 0 || current > max)
      {
        if (current < 0)
            current = 0;
        else if (current > max)
            current = max;

        // set the new position and notify observers
        nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(scrollbarBox));
        if (scrollbarFrame) {
          nsCOMPtr<nsIScrollbarMediator> mediator;
          scrollbarFrame->GetScrollbarMediator(getter_AddRefs(mediator));
          if (mediator) {
            mediator->PositionChanged(scrollbarFrame, GetCurrentPosition(scrollbar), current);
          }
        }

        nsAutoString currentStr;
        currentStr.AppendInt(current);
        scrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, currentStr, PR_TRUE);
      }
  }

  if (aAttribute == nsXULAtoms::maxpos ||
      aAttribute == nsXULAtoms::pageincrement ||
      aAttribute == nsXULAtoms::increment) {

      nsBoxLayoutState state(aPresContext);
      MarkDirtyChildren(state);
  }

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
{
  // if we are too small to have a thumb don't paint it.
  nsIBox* thumb;
  GetChildBox(&thumb);

  if (thumb) {
    nsRect thumbRect;
    thumb->GetBounds(thumbRect);
    nsMargin m;
    thumb->GetMargin(m);
    thumbRect.Inflate(m);

    nsRect crect;
    GetClientRect(crect);

    if (crect.width < thumbRect.width || crect.height < thumbRect.height)
    {
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
        PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
      }
      return NS_OK;
    }
  }

  return nsBoxFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP
nsSliderFrame::DoLayout(nsBoxLayoutState& aState)
{
  // get the thumb should be our only child
  nsIBox* thumbBox = nsnull;
  GetChildBox(&thumbBox);

  if (!thumbBox) {
    SyncLayout(aState);
    return NS_OK;
  }

  EnsureOrient();

  if (mState & NS_STATE_DEBUG_WAS_SET) {
      if (mState & NS_STATE_SET_TO_DEBUG)
          SetDebug(aState, PR_TRUE);
      else
          SetDebug(aState, PR_FALSE);
  }

  // get the content area inside our borders
  nsRect clientRect(0,0,0,0);
  GetClientRect(clientRect);

  // get the scrollbar
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);
  PRBool isHorizontal = IsHorizontal();

  // get the thumb's pref size
  nsSize thumbSize(0,0);
  thumbBox->GetPrefSize(aState, thumbSize);

  if (isHorizontal)
    thumbSize.height = clientRect.height;
  else
    thumbSize.width = clientRect.width;

  // get our current position and max position from our content node
  PRInt32 curpospx = GetCurrentPosition(scrollbar);
  PRInt32 maxpospx = GetMaxPosition(scrollbar);
  PRInt32 pageIncrement = GetPageIncrement(scrollbar);

  if (curpospx < 0)
     curpospx = 0;
  else if (curpospx > maxpospx)
     curpospx = maxpospx;

  float p2t;
  aState.GetPresContext()->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  // get max pos in twips
  nscoord maxpos = maxpospx*onePixel;

  // get our maxpos in twips. This is the space we have left over in the scrollbar
  // after the height of the thumb has been removed
  nscoord& desiredcoord = isHorizontal ? clientRect.width : clientRect.height;
  nscoord& thumbcoord = isHorizontal ? thumbSize.width : thumbSize.height;

  nscoord ourmaxpos = desiredcoord;

  mRatio = 1;

  if ((pageIncrement + maxpospx) != 0)
  {
    // if the thumb is flexible make the thumb bigger.
    nscoord flex = 0;
    thumbBox->GetFlex(aState, flex);

    if (flex > 0)
    {
      mRatio = float(pageIncrement) / float(maxpospx + pageIncrement);
      nscoord thumbsize = NSToCoordRound(ourmaxpos * mRatio);

      // if there is more room than the thumb needs stretch the thumb
      if (thumbsize > thumbcoord)
        thumbcoord = thumbsize;
    }
  }

  ourmaxpos -= thumbcoord;
  if (float(maxpos) != 0)
    mRatio = float(ourmaxpos)/float(maxpos);

  nscoord curpos = curpospx*onePixel;

  // set the thumbs y coord to be the current pos * the ratio.
  nscoord pos = nscoord(float(curpos)*mRatio);
  nsRect thumbRect(clientRect.x, clientRect.y, thumbSize.width, thumbSize.height);

  if (isHorizontal)
    thumbRect.x += pos;
  else
    thumbRect.y += pos;

  nsRect oldThumbRect;
  thumbBox->GetBounds(oldThumbRect);
  LayoutChildAt(aState, thumbBox, thumbRect);


  SyncLayout(aState);

#ifdef DEBUG_SLIDER
  PRInt32 c = GetCurrentPosition(scrollbar);
  PRInt32 m = GetMaxPosition(scrollbar);
  printf("Current=%d, max=%d\n", c, m);
#endif

  // redraw only if thumb changed size.
  if (oldThumbRect != thumbRect)
    Redraw(aState);

  return NS_OK;
}


NS_IMETHODIMP
nsSliderFrame::HandleEvent(nsIPresContext* aPresContext,
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);
  PRBool isHorizontal = IsHorizontal();

  if (isDraggingThumb())
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

#ifdef OLD_SLIDER_FRAME_OFFSET_CALCULATION

       // get it into our coordintate system by subtracting our parents offsets.
       nsIFrame* parent = this;
       while(parent != nsnull)
       {
         // if we hit a scrollable view make sure we take into account
         // how much we are scrolled.
         nsIView* view = parent->GetView();
         if (view) {
           nsIScrollableView* scrollingView;
           nsresult result = CallQueryInterface(view, &scrollingView);
           if (NS_SUCCEEDED(result)) {
             nscoord xoff = 0;
             nscoord yoff = 0;
             scrollingView->GetScrollPosition(xoff, yoff);
             isHorizontal ? start += xoff : start += yoff;
           }
         }

         nsRect r = parent->GetRect();
         isHorizontal ? start -= r.x : start -= r.y;

         if (view) {
           if (view->HasWidget()) {
             nsWindowType windowType;
             view->GetWidget()->GetWindowType(windowType);
             if (windowType == eWindowType_popup)
               break;
           }
         }

         parent = parent->GetParent();
       }

#else // Kin's new offset calculation code.

       // start is relative to the upper left corner of the rootview's widget,
       // unless the slider frame is in a popup, in which case it's relative
       // to the upper left corner of the popup's widget. We need start to be
       // in the coordinate system for the frame's contained view so that it
       // matches aEvent->point's coordinate system.

       nsIView *view = GetClosestView();

       nsCOMPtr<nsIWidget> rootWidget;

       if (view) {
         nsIViewManager* vm = view->GetViewManager();
         if (vm)
           vm->GetWidget(getter_AddRefs(rootWidget));
       }

       if (view && rootWidget) {
         // Now walk view's parent hierarchy, subtracting off the
         // positions of each parent view, until we hit the view
         // that contains the root or popup widget.
         //
         // When we're done, start should be in our contained
         // view's coordinate system.

         while (view) {
           nsIWidget* widget = view->GetWidget();

           if (widget) {
             nsWindowType windowType;
             widget->GetWindowType(windowType);

             if (widget == rootWidget || windowType == eWindowType_popup)
               break;
           }

           nsPoint p = view->GetPosition();
           start -= (isHorizontal ? p.x : p.y);

           view = view->GetParent();
         }
       }

#endif // Kin's new offset calculation code.

      //printf("Translated to start=%d\n",start);

       start -= mThumbStart;

       // take our current position and substract the start location
       pos -= start;
       PRBool isMouseOutsideThumb = PR_FALSE;
       if (gSnapMultiplier) {
         nsSize thumbSize = thumbFrame->GetSize();
         if (isHorizontal) {
           // horizontal scrollbar - check if mouse is above or below thumb
           if (aEvent->point.y < (-gSnapMultiplier * thumbSize.height) || 
               aEvent->point.y > thumbSize.height + (gSnapMultiplier * thumbSize.height))
             isMouseOutsideThumb = PR_TRUE;
         }
         else {
           // vertical scrollbar - check if mouse is left or right of thumb
           if (aEvent->point.x < (-gSnapMultiplier * thumbSize.width) || 
               aEvent->point.x > thumbSize.width + (gSnapMultiplier * thumbSize.width))
             isMouseOutsideThumb = PR_TRUE;
         }
       }
       if (isMouseOutsideThumb)
       {
         // XXX see bug 81586
         SetCurrentPosition(scrollbar, thumbFrame, (int) (mThumbStart / onePixel / mRatio), PR_FALSE);
         return NS_OK;
       }


       // convert to pixels
       nscoord pospx = pos/onePixel;

       // convert to our internal coordinate system
       pospx = nscoord(pospx/mRatio);

       // set it
       SetCurrentPosition(scrollbar, thumbFrame, pospx, PR_FALSE);

    }
    break;

    case NS_MOUSE_MIDDLE_BUTTON_UP:
    if(!gMiddlePref)
        break;

    case NS_MOUSE_LEFT_BUTTON_UP:
       // stop capturing
      //printf("stop capturing\n");
      AddListener();
      DragThumb(PR_FALSE);
      mRedrawImmediate = PR_FALSE;//we MUST call nsFrame HandleEvent for mouse ups to maintain the selection state and capture state.
      return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    }

    // we want to draw immediately if the user doing it directly with the
    // mouse that makes redrawing much faster. Switch it back now.
    mRedrawImmediate = PR_FALSE;

    //return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    return NS_OK;
  }
  else if ((aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && ((nsMouseEvent *)aEvent)->isShift)
      || (gMiddlePref && aEvent->message == NS_MOUSE_MIDDLE_BUTTON_DOWN)) {
    // convert coord from twips to pixels
    nscoord pos = isHorizontal ? aEvent->point.x : aEvent->point.y;
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);
    nscoord pospx = pos/onePixel;

   // adjust so that the middle of the thumb is placed under the click
    nsIFrame* thumbFrame = mFrames.FirstChild();
    nsSize thumbSize = thumbFrame->GetSize();
    nscoord thumbLength = isHorizontal ? thumbSize.width : thumbSize.height;
    thumbLength /= onePixel;
    pospx -= (thumbLength/2);


    // convert to our internal coordinate system
    pospx = nscoord(pospx/mRatio);

    // set it
    SetCurrentPosition(scrollbar, thumbFrame, pospx, PR_FALSE);

    // hack to start dragging

    nsIFrame* parent = this;
    while(parent != nsnull)
    {
      // if we hit a scrollable view make sure we take into account
      // how much we are scrolled.
      nsIView* view = parent->GetView();
      if (view) {
        nsIScrollableView* scrollingView;
        nsresult result = CallQueryInterface(view, &scrollingView);
        if (NS_SUCCEEDED(result)) {
          nscoord xoff = 0;
          nscoord yoff = 0;
          scrollingView->GetScrollPosition(xoff, yoff);
          isHorizontal ? pos += xoff : pos += yoff;
        }
      }

      nsPoint p = parent->GetPosition();
      pos += isHorizontal ? p.x : p.y;
      parent = parent->GetParent();
    }

    DragThumb(PR_TRUE);

    if (isHorizontal)
      mThumbStart = thumbFrame->GetPosition().x;
    else
      mThumbStart = thumbFrame->GetPosition().y;

    mDragStartPx =pos/onePixel;
  }
  else if (mChange != 0 && aEvent->message == NS_MOUSE_MOVE) {
    // We're in the process of moving the thumb to the mouse,
    // but the mouse just moved.  Make sure to update our
    // destination point.
    mDestinationPoint = aEvent->point;
  }

  // XXX hack until handle release is actually called in nsframe.
//  if (aEvent->message == NS_MOUSE_EXIT_SYNTH || aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP || aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
  //   HandleRelease(aPresContext, aEvent, aEventStatus);

  if (aEvent->message == NS_MOUSE_EXIT_SYNTH || aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
     HandleRelease(aPresContext, aEvent, aEventStatus);

  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}



nsIBox*
nsSliderFrame::GetScrollbar()
{
  // if we are in a scrollbar then return the scrollbar's content node
  // if we are not then return ours.
   nsIFrame* scrollbar;
   nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::scrollbar, this, scrollbar);

   if (scrollbar == nsnull)
       return this;

   nsIBox* ibox = nsnull;
   scrollbar->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox);

   if (ibox == nsnull)
       return this;

   return ibox;
}

void
nsSliderFrame::PageUpDown(nsIFrame* aThumbFrame, nscoord change)
{
  // on a page up or down get our page increment. We get this by getting the scrollbar we are in and
  // asking it for the current position and the page increment. If we are not in a scrollbar we will
  // get the values from our own node.
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  if (mScrollbarListener)
    mScrollbarListener->PagedUpDown(); // Let the listener decide our increment.

  nscoord pageIncrement = GetPageIncrement(scrollbar);
  PRInt32 curpos = GetCurrentPosition(scrollbar);
  SetCurrentPosition(scrollbar, aThumbFrame, curpos + change*pageIncrement, PR_TRUE);
}

// called when the current position changed and we need to update the thumb's location
nsresult
nsSliderFrame::CurrentPositionChanged(nsIPresContext* aPresContext)
{
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  PRBool isHorizontal = IsHorizontal();

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
    if (!thumbFrame)
      return NS_OK; // The thumb may stream in asynchronously via XBL.

    nsRect thumbRect = thumbFrame->GetRect();

    nsRect clientRect;
    GetClientRect(clientRect);

    // figure out the new rect
    nsRect newThumbRect(thumbRect);

    if (isHorizontal)
       newThumbRect.x = clientRect.x + nscoord(float(curpospx)*mRatio);
    else
       newThumbRect.y = clientRect.y + nscoord(float(curpospx)*mRatio);

    // set the rect
    thumbFrame->SetRect(newThumbRect);

    // figure out the union of the rect so we know what to redraw
    nsRect changeRect;
    changeRect.UnionRect(thumbRect, newThumbRect);

    // redraw just the change
    Invalidate(changeRect, mRedrawImmediate);
    
    if (mScrollbarListener)
      mScrollbarListener->PositionChanged(aPresContext, mCurPos, curpos);

    mCurPos = curpos;

    return NS_OK;
}

static void UpdateAttribute(nsIContent* aScrollbar, nscoord aNewPos, PRBool aNotify, PRBool aIsSmooth) {
  nsAutoString str;
  str.AppendInt(aNewPos);
  
  if (aIsSmooth) {
    aScrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::smooth, NS_LITERAL_STRING("true"), PR_FALSE);
  }
  aScrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, str, aNotify);
  if (aIsSmooth) {
    aScrollbar->UnsetAttr(kNameSpaceID_None, nsXULAtoms::smooth, PR_FALSE);
  }
}

void
nsSliderFrame::SetCurrentPosition(nsIContent* scrollbar, nsIFrame* aThumbFrame, nscoord newpos, PRBool aIsSmooth)
{

   // get our current position and max position from our content node
  PRInt32 maxpos = GetMaxPosition(scrollbar);

  // get the new position and make sure it is in bounds
  if (newpos > maxpos)
      newpos = maxpos;
  else if (newpos < 0)
      newpos = 0;

  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(scrollbarBox));


  if (scrollbarFrame) {
    // See if we have a mediator.
    nsCOMPtr<nsIScrollbarMediator> mediator;
    scrollbarFrame->GetScrollbarMediator(getter_AddRefs(mediator));
    if (mediator) {
      mediator->PositionChanged(scrollbarFrame, GetCurrentPosition(scrollbar), newpos);
      UpdateAttribute(scrollbar, newpos, PR_FALSE, aIsSmooth);
      CurrentPositionChanged(GetPresContext());
      return;
    }
  }

  UpdateAttribute(scrollbar, newpos, PR_TRUE, aIsSmooth);

#ifdef DEBUG_SLIDER
  printf("Current Pos=%d\n",newpos);
#endif

}

NS_IMETHODIMP  nsSliderFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint,
                                             nsFramePaintLayer aWhichLayer,
                                             nsIFrame**     aFrame)
{
  // This is EVIL, we shouldn't be messing with GetFrameForPoint just to get
  // thumb mouse drag events to arrive at the slider!
  if (isDraggingThumb())
  {
    // XXX I assume it's better not to test for visibility here.
    *aFrame = this;
    return NS_OK;
  }

  if (NS_SUCCEEDED(nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame)))
    return NS_OK;

  // always return us (if visible)
  if (mRect.Contains(aPoint) && GetStyleVisibility()->IsVisible()) {
    *aFrame = this;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsSliderFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  nsresult r = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  AddListener();

  return r;
}

nsresult
nsSliderMediator::MouseDown(nsIDOMEvent* aMouseEvent)
{
  // Only process the event if the thumb is not being dragged.
  if (mSlider && !mSlider->isDraggingThumb())
    return mSlider->MouseDown(aMouseEvent);

  return NS_OK;
}

nsresult
nsSliderMediator::MouseUp(nsIDOMEvent* aMouseEvent)
{
  // Only process the event if the thumb is not being dragged.
  if (mSlider && !mSlider->isDraggingThumb())
    return mSlider->MouseUp(aMouseEvent);

  return NS_OK;
}

nsresult
nsSliderFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
  //printf("Begin dragging\n");

  PRBool isHorizontal = IsHorizontal();

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));

  PRUint16 button = 0;
  PRBool scrollToClick = PR_FALSE;
  mouseEvent->GetShiftKey(&scrollToClick);
  mouseEvent->GetButton(&button);
  if (button != 0) {
    if (button != 1 || !gMiddlePref)
      return NS_OK;
    scrollToClick = PR_TRUE;
  }

  // If shift click or middle button, first
  // place the middle of the slider thumb under the click
  if (scrollToClick) {

    nscoord pos;
    nscoord pospx;

    // mouseEvent has click coordinates in pixels, convert to twips first
    isHorizontal ? mouseEvent->GetClientX(&pospx) : mouseEvent->GetClientY(&pospx);
    float p2t;
    GetPresContext()->GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);
    pos = pospx * onePixel;

    // then get it into our coordinate system by subtracting our parents offsets.
    nsIFrame* parent = this;
    while(parent != nsnull) {
       // if we hit a scrollable view make sure we take into account
       // how much we are scrolled.
       // XXX hack
       nsIView* view = parent->GetView();
       if (view) {
         nsIScrollableView* scrollingView;
         nsresult result = CallQueryInterface(view, &scrollingView);
         if (NS_SUCCEEDED(result)) {
             nscoord xoff = 0;
             nscoord yoff = 0;
             scrollingView->GetScrollPosition(xoff, yoff);
             isHorizontal ? pos += xoff : pos += yoff;
         }
       }

      pos -= (isHorizontal ? parent->GetPosition().x : parent->GetPosition().y);
      parent = parent->GetParent();
    }

    // now convert back into pixels
    pospx = pos/onePixel;

    // adjust so that the middle of the thumb is placed under the click
    nsIFrame* thumbFrame = mFrames.FirstChild();
    nsSize thumbSize = thumbFrame->GetSize();
    nscoord thumbLength = isHorizontal ? thumbSize.width : thumbSize.height;
    thumbLength /= onePixel;
    pospx -= (thumbLength/2);

    // finally, convert to scrollbar's internal coordinate system
    pospx = nscoord(pospx/mRatio);

    nsIBox* scrollbarBox = GetScrollbar();
    nsCOMPtr<nsIContent> scrollbar;
    scrollbar = GetContentOfBox(scrollbarBox);

    // set it
    SetCurrentPosition(scrollbar, thumbFrame, pospx, PR_FALSE);
  }

  DragThumb(PR_TRUE);
  PRInt32 c = 0;
  if (isHorizontal)
     mouseEvent->GetClientX(&c);
  else
     mouseEvent->GetClientY(&c);

  mDragStartPx = c;
  nsIFrame* thumbFrame = mFrames.FirstChild();

  if (isHorizontal)
     mThumbStart = thumbFrame->GetPosition().x;
  else
     mThumbStart = thumbFrame->GetPosition().y;

  //printf("Pressed mDragStartPx=%d\n",mDragStartPx);

  return NS_OK;
}

nsresult
nsSliderFrame::MouseUp(nsIDOMEvent* aMouseEvent)
{
 // printf("Finish dragging\n");

  return NS_OK;
}

void
nsSliderFrame::DragThumb(PRBool aGrabMouseEvents)
{
    // get its view
  nsIView* view = GetView();

  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();

    if (viewMan) {
      PRBool result;

      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
      }
    }
  }
}

PRBool
nsSliderFrame::isDraggingThumb()
{
    // get its view
  nsIView* view = GetView();

  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();

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
  if (!mMediator) {
    mMediator = new nsSliderMediator(this);
    NS_ADDREF(mMediator);
  }

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (thumbFrame) {
    nsCOMPtr<nsIDOMEventReceiver>
      receiver(do_QueryInterface(thumbFrame->GetContent()));

    receiver->AddEventListenerByIID(mMediator, NS_GET_IID(nsIDOMMouseListener));
  }
}

void
nsSliderFrame::RemoveListener()
{
  NS_ASSERTION(mMediator, "No listener was ever added!!");

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame)
    return;

  nsCOMPtr<nsIDOMEventReceiver>
    receiver(do_QueryInterface(thumbFrame->GetContent()));

  receiver->RemoveEventListenerByIID(mMediator, NS_GET_IID(nsIDOMMouseListener));
}

NS_IMETHODIMP
nsSliderFrame::HandlePress(nsIPresContext* aPresContext,
                           nsGUIEvent*     aEvent,
                           nsEventStatus*  aEventStatus)
{
  if (((nsMouseEvent *)aEvent)->isShift)
    return NS_OK;

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame) // display:none?
    return NS_OK;
  
  nsRect thumbRect = thumbFrame->GetRect();
  
  nscoord change = 1;
  if (IsHorizontal() ? aEvent->point.x < thumbRect.x 
                     : aEvent->point.y < thumbRect.y)
    change = -1;

  mChange = change;
  mDestinationPoint = aEvent->point;
  PageUpDown(thumbFrame, change);
  
  nsRepeatService::GetInstance()->Start(mMediator);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::HandleRelease(nsIPresContext* aPresContext,
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  nsRepeatService::GetInstance()->Stop();

  mChange = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::Destroy(nsIPresContext* aPresContext)
{
  // tell our mediator if we have one we are gone.
  if (mMediator) {
    mMediator->SetSlider(nsnull);
    NS_RELEASE(mMediator);
    mMediator = nsnull;
  }

  // call base class Destroy()
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsSliderFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  EnsureOrient();
  return nsBoxFrame::GetPrefSize(aState, aSize);
}

NS_IMETHODIMP
nsSliderFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  EnsureOrient();

  // our min size is just our borders and padding
  return nsBox::GetMinSize(aState, aSize);
}

NS_IMETHODIMP
nsSliderFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  EnsureOrient();
  return nsBoxFrame::GetMaxSize(aState, aSize);
}

void
nsSliderFrame::EnsureOrient()
{
  nsIBox* scrollbarBox = GetScrollbar();

  nsIFrame* frame = nsnull;
  scrollbarBox->GetFrame(&frame);

  PRBool isHorizontal = frame->GetStateBits() & NS_STATE_IS_HORIZONTAL;
  if (isHorizontal)
      mState |= NS_STATE_IS_HORIZONTAL;
  else
      mState &= ~NS_STATE_IS_HORIZONTAL;
}


void
nsSliderFrame::SetScrollbarListener(nsIScrollbarListener* aListener)
{
  // Don't addref/release this, since it's actually a frame.
  mScrollbarListener = aListener;
}

NS_IMETHODIMP nsSliderMediator::Notify(nsITimer *timer)
{
  if (mSlider)
    mSlider->Notify(timer);
  return NS_OK;
}

NS_IMETHODIMP_(void) nsSliderFrame::Notify(nsITimer *timer)
{
    PRBool stop = PR_FALSE;

    nsIFrame* thumbFrame = mFrames.FirstChild();
    nsRect thumbRect = thumbFrame->GetRect();

    PRBool isHorizontal = IsHorizontal();

    // See if the thumb has moved past our destination point.
    // if it has we want to stop.
    if (isHorizontal) {
        if (mChange < 0) {
            if (thumbRect.x < mDestinationPoint.x)
                stop = PR_TRUE;
        } else {
            if (thumbRect.x + thumbRect.width > mDestinationPoint.x)
                stop = PR_TRUE;
        }
    } else {
         if (mChange < 0) {
            if (thumbRect.y < mDestinationPoint.y)
                stop = PR_TRUE;
        } else {
            if (thumbRect.y + thumbRect.height > mDestinationPoint.y)
                stop = PR_TRUE;
        }
    }


    if (stop) {
       nsRepeatService::GetInstance()->Stop();
    } else {
      PageUpDown(thumbFrame, mChange);
    }
}

NS_INTERFACE_MAP_BEGIN(nsSliderMediator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsSliderMediator)
NS_IMPL_RELEASE(nsSliderMediator)

