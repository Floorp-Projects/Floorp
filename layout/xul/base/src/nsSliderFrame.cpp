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
 *   Stan Shebs <stanshebs@earthlink.net>
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
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDocument.h"
#include "nsScrollbarButtonFrame.h"
#include "nsISliderListener.h"
#include "nsIScrollbarMediator.h"
#include "nsScrollbarFrame.h"
#include "nsILookAndFeel.h"
#include "nsRepeatService.h"
#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIServiceManager.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

PRBool nsSliderFrame::gMiddlePref = PR_FALSE;
PRInt32 nsSliderFrame::gSnapMultiplier;

// Turn this on if you want to debug slider frames.
#undef DEBUG_SLIDER

static already_AddRefed<nsIContent>
GetContentOfBox(nsIBox *aBox)
{
  nsIContent* content = aBox->GetContent();
  NS_IF_ADDREF(content);
  return content;
}

nsIFrame*
NS_NewSliderFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSliderFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSliderFrame)

nsSliderFrame::nsSliderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext):
  nsBoxFrame(aPresShell, aContext),
  mCurPos(0),
  mChange(0),
  mUserChanged(PR_FALSE)
{
}

// stop timer
nsSliderFrame::~nsSliderFrame()
{
}

NS_IMETHODIMP
nsSliderFrame::Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  static PRBool gotPrefs = PR_FALSE;
  if (!gotPrefs) {
    gotPrefs = PR_TRUE;

    gMiddlePref = Preferences::GetBool("middlemouse.scrollbarPosition");
    gSnapMultiplier = Preferences::GetInt("slider.snapMultiplier");
  }

  mCurPos = GetCurrentPosition(aContent);

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  if (mFrames.IsEmpty())
    RemoveListener();

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  PRBool wasEmpty = mFrames.IsEmpty();
  nsresult rv = nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  if (wasEmpty)
    AddListener();

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  // if we have no children and on was added then make sure we add the
  // listener
  PRBool wasEmpty = mFrames.IsEmpty();
  nsresult rv = nsBoxFrame::AppendFrames(aListID, aFrameList);
  if (wasEmpty)
    AddListener();

  return rv;
}

PRInt32
nsSliderFrame::GetCurrentPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::curpos, 0);
}

PRInt32
nsSliderFrame::GetMinPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::minpos, 0);
}

PRInt32
nsSliderFrame::GetMaxPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::maxpos, 100);
}

PRInt32
nsSliderFrame::GetIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::increment, 1);
}


PRInt32
nsSliderFrame::GetPageIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::pageincrement, 10);
}

PRInt32
nsSliderFrame::GetIntegerAttribute(nsIContent* content, nsIAtom* atom, PRInt32 defaultValue)
{
    nsAutoString value;
    content->GetAttr(kNameSpaceID_None, atom, value);
    if (!value.IsEmpty()) {
      PRInt32 error;

      // convert it to an integer
      defaultValue = value.ToInteger(&error);
    }

    return defaultValue;
}

class nsValueChangedRunnable : public nsRunnable
{
public:
  nsValueChangedRunnable(nsISliderListener* aListener,
                         nsIAtom* aWhich,
                         PRInt32 aValue,
                         PRBool aUserChanged)
  : mListener(aListener), mWhich(aWhich),
    mValue(aValue), mUserChanged(aUserChanged)
  {}

  NS_IMETHODIMP Run()
  {
    return mListener->ValueChanged(nsDependentAtomString(mWhich),
                                   mValue, mUserChanged);
  }

  nsCOMPtr<nsISliderListener> mListener;
  nsCOMPtr<nsIAtom> mWhich;
  PRInt32 mValue;
  PRBool mUserChanged;
};

class nsDragStateChangedRunnable : public nsRunnable
{
public:
  nsDragStateChangedRunnable(nsISliderListener* aListener,
                             PRBool aDragBeginning)
  : mListener(aListener),
    mDragBeginning(aDragBeginning)
  {}

  NS_IMETHODIMP Run()
  {
    return mListener->DragStateChanged(mDragBeginning);
  }

  nsCOMPtr<nsISliderListener> mListener;
  PRBool mDragBeginning;
};

NS_IMETHODIMP
nsSliderFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  // if the current position changes
  if (aAttribute == nsGkAtoms::curpos) {
     rv = CurrentPositionChanged(PresContext(), PR_FALSE);
     NS_ASSERTION(NS_SUCCEEDED(rv), "failed to change position");
     if (NS_FAILED(rv))
        return rv;
  } else if (aAttribute == nsGkAtoms::minpos ||
             aAttribute == nsGkAtoms::maxpos) {
      // bounds check it.

      nsIBox* scrollbarBox = GetScrollbar();
      nsCOMPtr<nsIContent> scrollbar;
      scrollbar = GetContentOfBox(scrollbarBox);
      PRInt32 current = GetCurrentPosition(scrollbar);
      PRInt32 min = GetMinPosition(scrollbar);
      PRInt32 max = GetMaxPosition(scrollbar);

      // inform the parent <scale> that the minimum or maximum changed
      nsIFrame* parent = GetParent();
      if (parent) {
        nsCOMPtr<nsISliderListener> sliderListener = do_QueryInterface(parent->GetContent());
        if (sliderListener) {
          nsContentUtils::AddScriptRunner(
            new nsValueChangedRunnable(sliderListener, aAttribute,
                                       aAttribute == nsGkAtoms::minpos ? min : max, PR_FALSE));
        }
      }

      if (current < min || current > max)
      {
        if (current < min || max < min)
            current = min;
        else if (current > max)
            current = max;

        // set the new position and notify observers
        nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
        if (scrollbarFrame) {
          nsIScrollbarMediator* mediator = scrollbarFrame->GetScrollbarMediator();
          if (mediator) {
            mediator->PositionChanged(scrollbarFrame, GetCurrentPosition(scrollbar), current);
          }
        }

        nsContentUtils::AddScriptRunner(
          new nsSetAttrRunnable(scrollbar, nsGkAtoms::curpos, current));
      }
  }

  if (aAttribute == nsGkAtoms::minpos ||
      aAttribute == nsGkAtoms::maxpos ||
      aAttribute == nsGkAtoms::pageincrement ||
      aAttribute == nsGkAtoms::increment) {

      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }

  return rv;
}

NS_IMETHODIMP
nsSliderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForEventDelivery() && isDraggingThumb()) {
    // This is EVIL, we shouldn't be messing with event delivery just to get
    // thumb mouse drag events to arrive at the slider!
    return aLists.Outlines()->AppendNewToTop(new (aBuilder)
        nsDisplayEventReceiver(aBuilder, this));
  }
  
  return nsBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsSliderFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  // if we are too small to have a thumb don't paint it.
  nsIBox* thumb = GetChildBox();

  if (thumb) {
    nsRect thumbRect(thumb->GetRect());
    nsMargin m;
    thumb->GetMargin(m);
    thumbRect.Inflate(m);

    nsRect crect;
    GetClientRect(crect);

    if (crect.width < thumbRect.width || crect.height < thumbRect.height)
      return NS_OK;
  }
  
  return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsSliderFrame::DoLayout(nsBoxLayoutState& aState)
{
  // get the thumb should be our only child
  nsIBox* thumbBox = GetChildBox();

  if (!thumbBox) {
    SyncLayout(aState);
    return NS_OK;
  }

  EnsureOrient();

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_DEBUG_WAS_SET) {
      if (mState & NS_STATE_SET_TO_DEBUG)
          SetDebug(aState, PR_TRUE);
      else
          SetDebug(aState, PR_FALSE);
  }
#endif

  // get the content area inside our borders
  nsRect clientRect;
  GetClientRect(clientRect);

  // get the scrollbar
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  // get the thumb's pref size
  nsSize thumbSize = thumbBox->GetPrefSize(aState);

  if (IsHorizontal())
    thumbSize.height = clientRect.height;
  else
    thumbSize.width = clientRect.width;

  PRInt32 curPos = GetCurrentPosition(scrollbar);
  PRInt32 minPos = GetMinPosition(scrollbar);
  PRInt32 maxPos = GetMaxPosition(scrollbar);
  PRInt32 pageIncrement = GetPageIncrement(scrollbar);

  maxPos = NS_MAX(minPos, maxPos);
  curPos = NS_MAX(minPos, NS_MIN(curPos, maxPos));

  nscoord& availableLength = IsHorizontal() ? clientRect.width : clientRect.height;
  nscoord& thumbLength = IsHorizontal() ? thumbSize.width : thumbSize.height;

  if ((pageIncrement + maxPos - minPos) > 0 && thumbBox->GetFlex(aState) > 0) {
    float ratio = float(pageIncrement) / float(maxPos - minPos + pageIncrement);
    thumbLength = NS_MAX(thumbLength, NSToCoordRound(availableLength * ratio));
  }

  // Round the thumb's length to device pixels.
  nsPresContext* presContext = PresContext();
  thumbLength = presContext->DevPixelsToAppUnits(
                  presContext->AppUnitsToDevPixels(thumbLength));

  // mRatio translates the thumb position in app units to the value.
  mRatio = (minPos != maxPos) ? float(availableLength - thumbLength) / float(maxPos - minPos) : 1;

  // in reverse mode, curpos is reversed such that lower values are to the
  // right or bottom and increase leftwards or upwards. In this case, use the
  // offset from the end instead of the beginning.
  PRBool reverse = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                         nsGkAtoms::reverse, eCaseMatters);
  nscoord pos = reverse ? (maxPos - curPos) : (curPos - minPos);

  // set the thumb's coord to be the current pos * the ratio.
  nsRect thumbRect(clientRect.x, clientRect.y, thumbSize.width, thumbSize.height);
  PRInt32& thumbPos = (IsHorizontal() ? thumbRect.x : thumbRect.y);
  thumbPos += NSToCoordRound(pos * mRatio);

  nsRect oldThumbRect(thumbBox->GetRect());
  LayoutChildAt(aState, thumbBox, thumbRect);

  SyncLayout(aState);

  // Redraw only if thumb changed size.
  if (!oldThumbRect.IsEqualInterior(thumbRect))
    Redraw(aState);

  return NS_OK;
}


NS_IMETHODIMP
nsSliderFrame::HandleEvent(nsPresContext* aPresContext,
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // If a web page calls event.preventDefault() we still want to
  // scroll when scroll arrow is clicked. See bug 511075.
  if (!mContent->IsInNativeAnonymousSubtree() &&
      nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);
  PRBool isHorizontal = IsHorizontal();

  if (isDraggingThumb())
  {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE: {
      nsPoint eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                                         this);
      if (mChange) {
        // We're in the process of moving the thumb to the mouse,
        // but the mouse just moved.  Make sure to update our
        // destination point.
        mDestinationPoint = eventPoint;
        StopRepeat();
        StartRepeat();
        break;
      }

      nscoord pos = isHorizontal ? eventPoint.x : eventPoint.y;

      nsIFrame* thumbFrame = mFrames.FirstChild();
      if (!thumbFrame) {
        return NS_OK;
      }

      // take our current position and subtract the start location
      pos -= mDragStart;
      PRBool isMouseOutsideThumb = PR_FALSE;
      if (gSnapMultiplier) {
        nsSize thumbSize = thumbFrame->GetSize();
        if (isHorizontal) {
          // horizontal scrollbar - check if mouse is above or below thumb
          // XXXbz what about looking at the .y of the thumb's rect?  Is that
          // always zero here?
          if (eventPoint.y < -gSnapMultiplier * thumbSize.height ||
              eventPoint.y > thumbSize.height +
                               gSnapMultiplier * thumbSize.height)
            isMouseOutsideThumb = PR_TRUE;
        }
        else {
          // vertical scrollbar - check if mouse is left or right of thumb
          if (eventPoint.x < -gSnapMultiplier * thumbSize.width ||
              eventPoint.x > thumbSize.width +
                               gSnapMultiplier * thumbSize.width)
            isMouseOutsideThumb = PR_TRUE;
        }
      }
      if (isMouseOutsideThumb)
      {
        SetCurrentThumbPosition(scrollbar, mThumbStart, PR_FALSE, PR_TRUE, PR_FALSE);
        return NS_OK;
      }

      // set it
      SetCurrentThumbPosition(scrollbar, pos, PR_FALSE, PR_TRUE, PR_TRUE); // with snapping
    }
    break;

    case NS_MOUSE_BUTTON_UP:
      if (static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton ||
          (static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eMiddleButton &&
           gMiddlePref)) {
        // stop capturing
        AddListener();
        DragThumb(PR_FALSE);
        if (mChange) {
          StopRepeat();
          mChange = 0;
        }
        //we MUST call nsFrame HandleEvent for mouse ups to maintain the selection state and capture state.
        return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
      }
    }

    //return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    return NS_OK;
  } else if ((aEvent->message == NS_MOUSE_BUTTON_DOWN &&
              static_cast<nsMouseEvent*>(aEvent)->button ==
                nsMouseEvent::eLeftButton &&
#ifdef XP_MACOSX
              // On Mac the option key inverts the scroll-to-here preference.
              (static_cast<nsMouseEvent*>(aEvent)->isAlt != GetScrollToClick())) ||
#else
              (static_cast<nsMouseEvent*>(aEvent)->isShift != GetScrollToClick())) ||
#endif
             (gMiddlePref && aEvent->message == NS_MOUSE_BUTTON_DOWN &&
              static_cast<nsMouseEvent*>(aEvent)->button ==
                nsMouseEvent::eMiddleButton)) {

    nsPoint eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                                      this);
    nscoord pos = isHorizontal ? eventPoint.x : eventPoint.y;

    // adjust so that the middle of the thumb is placed under the click
    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      return NS_OK;
    }
    nsSize thumbSize = thumbFrame->GetSize();
    nscoord thumbLength = isHorizontal ? thumbSize.width : thumbSize.height;

    // set it
    nsWeakFrame weakFrame(this);
    // should aMaySnap be PR_TRUE here?
    SetCurrentThumbPosition(scrollbar, pos - thumbLength/2, PR_FALSE, PR_FALSE, PR_FALSE);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

    DragThumb(PR_TRUE);

    if (isHorizontal)
      mThumbStart = thumbFrame->GetPosition().x;
    else
      mThumbStart = thumbFrame->GetPosition().y;

    mDragStart = pos - mThumbStart;
  }

  // XXX hack until handle release is actually called in nsframe.
//  if (aEvent->message == NS_MOUSE_EXIT_SYNTH || aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP || aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
  //   HandleRelease(aPresContext, aEvent, aEventStatus);

  if (aEvent->message == NS_MOUSE_EXIT_SYNTH && mChange)
     HandleRelease(aPresContext, aEvent, aEventStatus);

  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

// Helper function to collect the "scroll to click" metric. Beware of
// caching this, users expect to be able to change the system preference
// and see the browser change its behavior immediately.
PRBool
nsSliderFrame::GetScrollToClick()
{
  // if there is no parent scrollbar, check the movetoclick attribute. If set
  // to true, always scroll to the click point. If false, never do this.
  // Otherwise, the default is true on Mac and false on other platforms.
  if (GetScrollbar() == this)
#ifdef XP_MACOSX
    return !mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::movetoclick,
                                   nsGkAtoms::_false, eCaseMatters);
 
  // if there was no scrollbar, always scroll on click
  PRBool scrollToClick = PR_FALSE;
  nsresult rv;
  nsCOMPtr<nsILookAndFeel> lookNFeel =
    do_GetService("@mozilla.org/widget/lookandfeel;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 scrollToClickMetric;
    rv = lookNFeel->GetMetric(nsILookAndFeel::eMetric_ScrollToClick,
                              scrollToClickMetric);
    if (NS_SUCCEEDED(rv) && scrollToClickMetric == 1)
      scrollToClick = PR_TRUE;
  }
  return scrollToClick;

#else
    return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::movetoclick,
                                  nsGkAtoms::_true, eCaseMatters);
  return PR_FALSE;
#endif
}

nsIBox*
nsSliderFrame::GetScrollbar()
{
  // if we are in a scrollbar then return the scrollbar's content node
  // if we are not then return ours.
   nsIFrame* scrollbar;
   nsScrollbarButtonFrame::GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);

   if (scrollbar == nsnull)
       return this;

   return scrollbar->IsBoxFrame() ? scrollbar : this;
}

void
nsSliderFrame::PageUpDown(nscoord change)
{
  // on a page up or down get our page increment. We get this by getting the scrollbar we are in and
  // asking it for the current position and the page increment. If we are not in a scrollbar we will
  // get the values from our own node.
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                            nsGkAtoms::reverse, eCaseMatters))
    change = -change;

  nscoord pageIncrement = GetPageIncrement(scrollbar);
  PRInt32 curpos = GetCurrentPosition(scrollbar);
  PRInt32 minpos = GetMinPosition(scrollbar);
  PRInt32 maxpos = GetMaxPosition(scrollbar);

  // get the new position and make sure it is in bounds
  PRInt32 newpos = curpos + change * pageIncrement;
  if (newpos < minpos || maxpos < minpos)
    newpos = minpos;
  else if (newpos > maxpos)
    newpos = maxpos;

  SetCurrentPositionInternal(scrollbar, newpos, PR_TRUE, PR_FALSE);
}

// called when the current position changed and we need to update the thumb's location
nsresult
nsSliderFrame::CurrentPositionChanged(nsPresContext* aPresContext,
                                      PRBool aImmediateRedraw)
{
  nsIBox* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  // get the current position
  PRInt32 curPos = GetCurrentPosition(scrollbar);

  // do nothing if the position did not change
  if (mCurPos == curPos)
      return NS_OK;

  // get our current min and max position from our content node
  PRInt32 minPos = GetMinPosition(scrollbar);
  PRInt32 maxPos = GetMaxPosition(scrollbar);

  maxPos = NS_MAX(minPos, maxPos);
  curPos = NS_MAX(minPos, NS_MIN(curPos, maxPos));

  // get the thumb's rect
  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame)
    return NS_OK; // The thumb may stream in asynchronously via XBL.

  nsRect thumbRect = thumbFrame->GetRect();

  nsRect clientRect;
  GetClientRect(clientRect);

  // figure out the new rect
  nsRect newThumbRect(thumbRect);

  PRBool reverse = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                         nsGkAtoms::reverse, eCaseMatters);
  nscoord pos = reverse ? (maxPos - curPos) : (curPos - minPos);

  if (IsHorizontal())
     newThumbRect.x = clientRect.x + NSToCoordRound(pos * mRatio);
  else
     newThumbRect.y = clientRect.y + NSToCoordRound(pos * mRatio);

  // set the rect
  thumbFrame->SetRect(newThumbRect);

  // Redraw the scrollbar
  InvalidateWithFlags(clientRect, aImmediateRedraw ? INVALIDATE_IMMEDIATE : 0);

  mCurPos = curPos;

  // inform the parent <scale> if it exists that the value changed
  nsIFrame* parent = GetParent();
  if (parent) {
    nsCOMPtr<nsISliderListener> sliderListener = do_QueryInterface(parent->GetContent());
    if (sliderListener) {
      nsContentUtils::AddScriptRunner(
        new nsValueChangedRunnable(sliderListener, nsGkAtoms::curpos, mCurPos, mUserChanged));
    }
  }

  return NS_OK;
}

static void UpdateAttribute(nsIContent* aScrollbar, nscoord aNewPos, PRBool aNotify, PRBool aIsSmooth) {
  nsAutoString str;
  str.AppendInt(aNewPos);
  
  if (aIsSmooth) {
    aScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth, NS_LITERAL_STRING("true"), PR_FALSE);
  }
  aScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, str, aNotify);
  if (aIsSmooth) {
    aScrollbar->UnsetAttr(kNameSpaceID_None, nsGkAtoms::smooth, PR_FALSE);
  }
}

// Use this function when you want to set the scroll position via the position
// of the scrollbar thumb, e.g. when dragging the slider. This function scrolls
// the content in such a way that thumbRect.x/.y becomes aNewThumbPos.
void
nsSliderFrame::SetCurrentThumbPosition(nsIContent* aScrollbar, nscoord aNewThumbPos,
                                       PRBool aIsSmooth, PRBool aImmediateRedraw, PRBool aMaySnap)
{
  nsRect crect;
  GetClientRect(crect);
  nscoord offset = IsHorizontal() ? crect.x : crect.y;
  PRInt32 newPos = NSToIntRound((aNewThumbPos - offset) / mRatio);
  
  if (aMaySnap && mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::snap,
                                        nsGkAtoms::_true, eCaseMatters)) {
    // If snap="true", then the slider may only be set to min + (increment * x).
    // Otherwise, the slider may be set to any positive integer.
    PRInt32 increment = GetIncrement(aScrollbar);
    newPos = NSToIntRound(newPos / float(increment)) * increment;
  }
  
  SetCurrentPosition(aScrollbar, newPos, aIsSmooth, aImmediateRedraw);
}

// Use this function when you know the target scroll position of the scrolled content.
// aNewPos should be passed to this function as a position as if the minpos is 0.
// That is, the minpos will be added to the position by this function. In a reverse
// direction slider, the newpos should be the distance from the end.
void
nsSliderFrame::SetCurrentPosition(nsIContent* aScrollbar, PRInt32 aNewPos,
                                  PRBool aIsSmooth, PRBool aImmediateRedraw)
{
   // get min and max position from our content node
  PRInt32 minpos = GetMinPosition(aScrollbar);
  PRInt32 maxpos = GetMaxPosition(aScrollbar);

  // in reverse direction sliders, flip the value so that it goes from
  // right to left, or bottom to top.
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                            nsGkAtoms::reverse, eCaseMatters))
    aNewPos = maxpos - aNewPos;
  else
    aNewPos += minpos;

  // get the new position and make sure it is in bounds
  if (aNewPos < minpos || maxpos < minpos)
    aNewPos = minpos;
  else if (aNewPos > maxpos)
    aNewPos = maxpos;

  SetCurrentPositionInternal(aScrollbar, aNewPos, aIsSmooth, aImmediateRedraw);
}

void
nsSliderFrame::SetCurrentPositionInternal(nsIContent* aScrollbar, PRInt32 aNewPos,
                                          PRBool aIsSmooth,
                                          PRBool aImmediateRedraw)
{
  nsCOMPtr<nsIContent> scrollbar = aScrollbar;
  nsIBox* scrollbarBox = GetScrollbar();

  mUserChanged = PR_TRUE;

  nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
  if (scrollbarFrame) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbarFrame->GetScrollbarMediator();
    if (mediator) {
      nsRefPtr<nsPresContext> context = PresContext();
      nsCOMPtr<nsIContent> content = GetContent();
      mediator->PositionChanged(scrollbarFrame, GetCurrentPosition(scrollbar), aNewPos);
      // 'mediator' might be dangling now...
      UpdateAttribute(scrollbar, aNewPos, PR_FALSE, aIsSmooth);
      nsIFrame* frame = content->GetPrimaryFrame();
      if (frame && frame->GetType() == nsGkAtoms::sliderFrame) {
        static_cast<nsSliderFrame*>(frame)->
          CurrentPositionChanged(frame->PresContext(), aImmediateRedraw);
      }
      mUserChanged = PR_FALSE;
      return;
    }
  }

  UpdateAttribute(scrollbar, aNewPos, PR_TRUE, aIsSmooth);
  mUserChanged = PR_FALSE;

#ifdef DEBUG_SLIDER
  printf("Current Pos=%d\n",aNewPos);
#endif

}

nsIAtom*
nsSliderFrame::GetType() const
{
  return nsGkAtoms::sliderFrame;
}

NS_IMETHODIMP
nsSliderFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  nsresult r = nsBoxFrame::SetInitialChildList(aListID, aChildList);

  AddListener();

  return r;
}

nsresult
nsSliderMediator::HandleEvent(nsIDOMEvent* aEvent)
{
  // Only process the event if the thumb is not being dragged.
  if (mSlider && !mSlider->isDraggingThumb())
    return mSlider->MouseDown(aEvent);

  return NS_OK;
}

nsresult
nsSliderFrame::MouseDown(nsIDOMEvent* aMouseEvent)
{
#ifdef DEBUG_SLIDER
  printf("Begin dragging\n");
#endif

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
  if (!mouseEvent)
    return NS_OK;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return NS_OK;

  PRUint16 button = 0;
  mouseEvent->GetButton(&button);
  if (!(button == 0 || (button == 1 && gMiddlePref)))
    return NS_OK;

  PRBool isHorizontal = IsHorizontal();

  PRBool scrollToClick = PR_FALSE;
#ifndef XP_MACOSX
  // On Mac there's no scroll-to-here when clicking the thumb
  mouseEvent->GetShiftKey(&scrollToClick);
  if (button != 0) {
    scrollToClick = PR_TRUE;
  }
#endif

  nsPoint pt =  nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(mouseEvent,
                                                                this);
  nscoord pos = isHorizontal ? pt.x : pt.y;

  // If shift click or middle button, first
  // place the middle of the slider thumb under the click
  nsCOMPtr<nsIContent> scrollbar;
  nscoord newpos = pos;
  if (scrollToClick) {
    // adjust so that the middle of the thumb is placed under the click
    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      return NS_OK;
    }
    nsSize thumbSize = thumbFrame->GetSize();
    nscoord thumbLength = isHorizontal ? thumbSize.width : thumbSize.height;

    newpos -= (thumbLength/2);

    nsIBox* scrollbarBox = GetScrollbar();
    scrollbar = GetContentOfBox(scrollbarBox);
  }

  DragThumb(PR_TRUE);

  if (scrollToClick) {
    // should aMaySnap be PR_TRUE here?
    SetCurrentThumbPosition(scrollbar, newpos, PR_FALSE, PR_FALSE, PR_FALSE);
  }

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame) {
    return NS_OK;
  }

  if (isHorizontal)
    mThumbStart = thumbFrame->GetPosition().x;
  else
    mThumbStart = thumbFrame->GetPosition().y;

  mDragStart = pos - mThumbStart;

#ifdef DEBUG_SLIDER
  printf("Pressed mDragStart=%d\n",mDragStart);
#endif

  return NS_OK;
}

void
nsSliderFrame::DragThumb(PRBool aGrabMouseEvents)
{
  // inform the parent <scale> that a drag is beginning or ending
  nsIFrame* parent = GetParent();
  if (parent) {
    nsCOMPtr<nsISliderListener> sliderListener = do_QueryInterface(parent->GetContent());
    if (sliderListener) {
      nsContentUtils::AddScriptRunner(
        new nsDragStateChangedRunnable(sliderListener, aGrabMouseEvents));
    }
  }

  nsIPresShell::SetCapturingContent(aGrabMouseEvents ? GetContent() : nsnull,
                                    aGrabMouseEvents ? CAPTURE_IGNOREALLOWED : 0);
}

PRBool
nsSliderFrame::isDraggingThumb()
{
  return (nsIPresShell::GetCapturingContent() == GetContent());
}

void
nsSliderFrame::AddListener()
{
  if (!mMediator) {
    mMediator = new nsSliderMediator(this);
  }

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (thumbFrame) {
    thumbFrame->GetContent()->
      AddEventListener(NS_LITERAL_STRING("mousedown"), mMediator, PR_FALSE,
                       PR_FALSE);
  }
}

void
nsSliderFrame::RemoveListener()
{
  NS_ASSERTION(mMediator, "No listener was ever added!!");

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame)
    return;

  thumbFrame->GetContent()->
    RemoveEventListener(NS_LITERAL_STRING("mousedown"), mMediator, PR_FALSE);
}

NS_IMETHODIMP
nsSliderFrame::HandlePress(nsPresContext* aPresContext,
                           nsGUIEvent*     aEvent,
                           nsEventStatus*  aEventStatus)
{
#ifdef XP_MACOSX
  // On Mac the option key inverts the scroll-to-here preference.
  if (((nsMouseEvent *)aEvent)->isAlt != GetScrollToClick())
#else
  if (((nsMouseEvent *)aEvent)->isShift != GetScrollToClick())
#endif
    return NS_OK;

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame) // display:none?
    return NS_OK;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return NS_OK;
  
  nsRect thumbRect = thumbFrame->GetRect();
  
  nscoord change = 1;
  nsPoint eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                                    this);
  if (IsHorizontal() ? eventPoint.x < thumbRect.x 
                     : eventPoint.y < thumbRect.y)
    change = -1;

  mChange = change;
  DragThumb(PR_TRUE);
  mDestinationPoint = eventPoint;
  StartRepeat();
  PageUpDown(change);
  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::HandleRelease(nsPresContext* aPresContext,
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  StopRepeat();

  return NS_OK;
}

void
nsSliderFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // tell our mediator if we have one we are gone.
  if (mMediator) {
    mMediator->SetSlider(nsnull);
    mMediator = nsnull;
  }
  StopRepeat();

  // call base class Destroy()
  nsBoxFrame::DestroyFrom(aDestructRoot);
}

nsSize
nsSliderFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  EnsureOrient();
  return nsBoxFrame::GetPrefSize(aState);
}

nsSize
nsSliderFrame::GetMinSize(nsBoxLayoutState& aState)
{
  EnsureOrient();

  // our min size is just our borders and padding
  return nsBox::GetMinSize(aState);
}

nsSize
nsSliderFrame::GetMaxSize(nsBoxLayoutState& aState)
{
  EnsureOrient();
  return nsBoxFrame::GetMaxSize(aState);
}

void
nsSliderFrame::EnsureOrient()
{
  nsIBox* scrollbarBox = GetScrollbar();

  PRBool isHorizontal = (scrollbarBox->GetStateBits() & NS_STATE_IS_HORIZONTAL) != 0;
  if (isHorizontal)
      mState |= NS_STATE_IS_HORIZONTAL;
  else
      mState &= ~NS_STATE_IS_HORIZONTAL;
}


void nsSliderFrame::Notify(void)
{
    PRBool stop = PR_FALSE;

    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      StopRepeat();
      return;
    }
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
      StopRepeat();
    } else {
      PageUpDown(mChange);
    }
}

NS_IMPL_ISUPPORTS1(nsSliderMediator,
                   nsIDOMEventListener)
