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

#include "nsSliderFrame.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIDOMMouseEvent.h"
#include "nsScrollbarButtonFrame.h"
#include "nsISliderListener.h"
#include "nsIScrollbarMediator.h"
#include "nsScrollbarFrame.h"
#include "nsRepeatService.h"
#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIServiceManager.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include <algorithm>

using namespace mozilla;

bool nsSliderFrame::gMiddlePref = false;
int32_t nsSliderFrame::gSnapMultiplier;

// Turn this on if you want to debug slider frames.
#undef DEBUG_SLIDER

static already_AddRefed<nsIContent>
GetContentOfBox(nsIFrame *aBox)
{
  nsCOMPtr<nsIContent> content = aBox->GetContent();
  return content.forget();
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
  mDragFinished(true),
  mUserChanged(false)
{
}

// stop timer
nsSliderFrame::~nsSliderFrame()
{
}

void
nsSliderFrame::Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow)
{
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  static bool gotPrefs = false;
  if (!gotPrefs) {
    gotPrefs = true;

    gMiddlePref = Preferences::GetBool("middlemouse.scrollbarPosition");
    gSnapMultiplier = Preferences::GetInt("slider.snapMultiplier");
  }

  mCurPos = GetCurrentPosition(aContent);
}

void
nsSliderFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  nsBoxFrame::RemoveFrame(aListID, aOldFrame);
  if (mFrames.IsEmpty())
    RemoveListener();
}

void
nsSliderFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  bool wasEmpty = mFrames.IsEmpty();
  nsBoxFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
  if (wasEmpty)
    AddListener();
}

void
nsSliderFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  // if we have no children and on was added then make sure we add the
  // listener
  bool wasEmpty = mFrames.IsEmpty();
  nsBoxFrame::AppendFrames(aListID, aFrameList);
  if (wasEmpty)
    AddListener();
}

int32_t
nsSliderFrame::GetCurrentPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::curpos, 0);
}

int32_t
nsSliderFrame::GetMinPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::minpos, 0);
}

int32_t
nsSliderFrame::GetMaxPosition(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::maxpos, 100);
}

int32_t
nsSliderFrame::GetIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::increment, 1);
}


int32_t
nsSliderFrame::GetPageIncrement(nsIContent* content)
{
  return GetIntegerAttribute(content, nsGkAtoms::pageincrement, 10);
}

int32_t
nsSliderFrame::GetIntegerAttribute(nsIContent* content, nsIAtom* atom, int32_t defaultValue)
{
    nsAutoString value;
    content->GetAttr(kNameSpaceID_None, atom, value);
    if (!value.IsEmpty()) {
      nsresult error;

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
                         int32_t aValue,
                         bool aUserChanged)
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
  int32_t mValue;
  bool mUserChanged;
};

class nsDragStateChangedRunnable : public nsRunnable
{
public:
  nsDragStateChangedRunnable(nsISliderListener* aListener,
                             bool aDragBeginning)
  : mListener(aListener),
    mDragBeginning(aDragBeginning)
  {}

  NS_IMETHODIMP Run()
  {
    return mListener->DragStateChanged(mDragBeginning);
  }

  nsCOMPtr<nsISliderListener> mListener;
  bool mDragBeginning;
};

nsresult
nsSliderFrame::AttributeChanged(int32_t aNameSpaceID,
                                nsIAtom* aAttribute,
                                int32_t aModType)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  // if the current position changes
  if (aAttribute == nsGkAtoms::curpos) {
     CurrentPositionChanged();
  } else if (aAttribute == nsGkAtoms::minpos ||
             aAttribute == nsGkAtoms::maxpos) {
      // bounds check it.

      nsIFrame* scrollbarBox = GetScrollbar();
      nsCOMPtr<nsIContent> scrollbar;
      scrollbar = GetContentOfBox(scrollbarBox);
      int32_t current = GetCurrentPosition(scrollbar);
      int32_t min = GetMinPosition(scrollbar);
      int32_t max = GetMaxPosition(scrollbar);

      // inform the parent <scale> that the minimum or maximum changed
      nsIFrame* parent = GetParent();
      if (parent) {
        nsCOMPtr<nsISliderListener> sliderListener = do_QueryInterface(parent->GetContent());
        if (sliderListener) {
          nsContentUtils::AddScriptRunner(
            new nsValueChangedRunnable(sliderListener, aAttribute,
                                       aAttribute == nsGkAtoms::minpos ? min : max, false));
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

void
nsSliderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (aBuilder->IsForEventDelivery() && isDraggingThumb()) {
    // This is EVIL, we shouldn't be messing with event delivery just to get
    // thumb mouse drag events to arrive at the slider!
    aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayEventReceiver(aBuilder, this));
    return;
  }
  
  nsBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

void
nsSliderFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  // if we are too small to have a thumb don't paint it.
  nsIFrame* thumb = nsBox::GetChildBox(this);

  if (thumb) {
    nsRect thumbRect(thumb->GetRect());
    nsMargin m;
    thumb->GetMargin(m);
    thumbRect.Inflate(m);

    nsRect crect;
    GetClientRect(crect);

    if (crect.width < thumbRect.width || crect.height < thumbRect.height)
      return;
  }
  
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsSliderFrame::DoLayout(nsBoxLayoutState& aState)
{
  // get the thumb should be our only child
  nsIFrame* thumbBox = nsBox::GetChildBox(this);

  if (!thumbBox) {
    SyncLayout(aState);
    return NS_OK;
  }

  EnsureOrient();

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_DEBUG_WAS_SET) {
      if (mState & NS_STATE_SET_TO_DEBUG)
          SetDebug(aState, true);
      else
          SetDebug(aState, false);
  }
#endif

  // get the content area inside our borders
  nsRect clientRect;
  GetClientRect(clientRect);

  // get the scrollbar
  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  // get the thumb's pref size
  nsSize thumbSize = thumbBox->GetPrefSize(aState);

  if (IsHorizontal())
    thumbSize.height = clientRect.height;
  else
    thumbSize.width = clientRect.width;

  int32_t curPos = GetCurrentPosition(scrollbar);
  int32_t minPos = GetMinPosition(scrollbar);
  int32_t maxPos = GetMaxPosition(scrollbar);
  int32_t pageIncrement = GetPageIncrement(scrollbar);

  maxPos = std::max(minPos, maxPos);
  curPos = clamped(curPos, minPos, maxPos);

  nscoord& availableLength = IsHorizontal() ? clientRect.width : clientRect.height;
  nscoord& thumbLength = IsHorizontal() ? thumbSize.width : thumbSize.height;

  if ((pageIncrement + maxPos - minPos) > 0 && thumbBox->GetFlex(aState) > 0) {
    float ratio = float(pageIncrement) / float(maxPos - minPos + pageIncrement);
    thumbLength = std::max(thumbLength, NSToCoordRound(availableLength * ratio));
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
  bool reverse = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                         nsGkAtoms::reverse, eCaseMatters);
  nscoord pos = reverse ? (maxPos - curPos) : (curPos - minPos);

  // set the thumb's coord to be the current pos * the ratio.
  nsRect thumbRect(clientRect.x, clientRect.y, thumbSize.width, thumbSize.height);
  int32_t& thumbPos = (IsHorizontal() ? thumbRect.x : thumbRect.y);
  thumbPos += NSToCoordRound(pos * mRatio);

  nsRect oldThumbRect(thumbBox->GetRect());
  LayoutChildAt(aState, thumbBox, thumbRect);

  SyncLayout(aState);

  // Redraw only if thumb changed size.
  if (!oldThumbRect.IsEqualInterior(thumbRect))
    Redraw(aState);

  return NS_OK;
}


nsresult
nsSliderFrame::HandleEvent(nsPresContext* aPresContext,
                           WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // If a web page calls event.preventDefault() we still want to
  // scroll when scroll arrow is clicked. See bug 511075.
  if (!mContent->IsInNativeAnonymousSubtree() &&
      nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  if (!mDragFinished && !isDraggingThumb()) {
    StopDrag();
    return NS_OK;
  }

  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);
  bool isHorizontal = IsHorizontal();

  if (isDraggingThumb())
  {
    switch (aEvent->message) {
    case NS_TOUCH_MOVE:
    case NS_MOUSE_MOVE: {
      nsPoint eventPoint;
      if (!GetEventPoint(aEvent, eventPoint)) {
        break;
      }
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
      bool isMouseOutsideThumb = false;
      if (gSnapMultiplier) {
        nsSize thumbSize = thumbFrame->GetSize();
        if (isHorizontal) {
          // horizontal scrollbar - check if mouse is above or below thumb
          // XXXbz what about looking at the .y of the thumb's rect?  Is that
          // always zero here?
          if (eventPoint.y < -gSnapMultiplier * thumbSize.height ||
              eventPoint.y > thumbSize.height +
                               gSnapMultiplier * thumbSize.height)
            isMouseOutsideThumb = true;
        }
        else {
          // vertical scrollbar - check if mouse is left or right of thumb
          if (eventPoint.x < -gSnapMultiplier * thumbSize.width ||
              eventPoint.x > thumbSize.width +
                               gSnapMultiplier * thumbSize.width)
            isMouseOutsideThumb = true;
        }
      }
      if (aEvent->mClass == eTouchEventClass) {
        *aEventStatus = nsEventStatus_eConsumeNoDefault;
      }
      if (isMouseOutsideThumb)
      {
        SetCurrentThumbPosition(scrollbar, mThumbStart, false, false);
        return NS_OK;
      }

      // set it
      SetCurrentThumbPosition(scrollbar, pos, false, true); // with snapping
    }
    break;

    case NS_TOUCH_END:
    case NS_MOUSE_BUTTON_UP:
      if (ShouldScrollForEvent(aEvent)) {
        StopDrag();
        //we MUST call nsFrame HandleEvent for mouse ups to maintain the selection state and capture state.
        return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
      }
    }

    //return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    return NS_OK;
  } else if (ShouldScrollToClickForEvent(aEvent)) {
    nsPoint eventPoint;
    if (!GetEventPoint(aEvent, eventPoint)) {
      return NS_OK;
    }
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
    // should aMaySnap be true here?
    SetCurrentThumbPosition(scrollbar, pos - thumbLength/2, false, false);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

    DragThumb(true);
    if (aEvent->mClass == eTouchEventClass) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
    }

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
bool
nsSliderFrame::GetScrollToClick()
{
  if (GetScrollbar() != this) {
    return LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollToClick, false);
  }

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::movetoclick,
                            nsGkAtoms::_true, eCaseMatters)) {
    return true;
  }
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::movetoclick,
                            nsGkAtoms::_false, eCaseMatters)) {
    return false;
  }

#ifdef XP_MACOSX
  return true;
#else
  return false;
#endif
}

nsIFrame*
nsSliderFrame::GetScrollbar()
{
  // if we are in a scrollbar then return the scrollbar's content node
  // if we are not then return ours.
   nsIFrame* scrollbar;
   nsScrollbarButtonFrame::GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);

   if (scrollbar == nullptr)
       return this;

   return scrollbar->IsBoxFrame() ? scrollbar : this;
}

void
nsSliderFrame::PageUpDown(nscoord change)
{
  // on a page up or down get our page increment. We get this by getting the scrollbar we are in and
  // asking it for the current position and the page increment. If we are not in a scrollbar we will
  // get the values from our own node.
  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                            nsGkAtoms::reverse, eCaseMatters))
    change = -change;

  nscoord pageIncrement = GetPageIncrement(scrollbar);
  int32_t curpos = GetCurrentPosition(scrollbar);
  int32_t minpos = GetMinPosition(scrollbar);
  int32_t maxpos = GetMaxPosition(scrollbar);

  // get the new position and make sure it is in bounds
  int32_t newpos = curpos + change * pageIncrement;
  if (newpos < minpos || maxpos < minpos)
    newpos = minpos;
  else if (newpos > maxpos)
    newpos = maxpos;

  SetCurrentPositionInternal(scrollbar, newpos, true);
}

// called when the current position changed and we need to update the thumb's location
void
nsSliderFrame::CurrentPositionChanged()
{
  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  // get the current position
  int32_t curPos = GetCurrentPosition(scrollbar);

  // do nothing if the position did not change
  if (mCurPos == curPos)
    return;

  // get our current min and max position from our content node
  int32_t minPos = GetMinPosition(scrollbar);
  int32_t maxPos = GetMaxPosition(scrollbar);

  maxPos = std::max(minPos, maxPos);
  curPos = clamped(curPos, minPos, maxPos);

  // get the thumb's rect
  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame)
    return; // The thumb may stream in asynchronously via XBL.

  nsRect thumbRect = thumbFrame->GetRect();

  nsRect clientRect;
  GetClientRect(clientRect);

  // figure out the new rect
  nsRect newThumbRect(thumbRect);

  bool reverse = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                         nsGkAtoms::reverse, eCaseMatters);
  nscoord pos = reverse ? (maxPos - curPos) : (curPos - minPos);

  if (IsHorizontal())
     newThumbRect.x = clientRect.x + NSToCoordRound(pos * mRatio);
  else
     newThumbRect.y = clientRect.y + NSToCoordRound(pos * mRatio);

  // avoid putting the scroll thumb at subpixel positions which cause needless invalidations
  nscoord appUnitsPerPixel = PresContext()->AppUnitsPerDevPixel();
  newThumbRect = newThumbRect.ToNearestPixels(appUnitsPerPixel).ToAppUnits(appUnitsPerPixel);

  // set the rect
  thumbFrame->SetRect(newThumbRect);

  // Request a repaint of the scrollbar
  SchedulePaint();

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
}

static void UpdateAttribute(nsIContent* aScrollbar, nscoord aNewPos, bool aNotify, bool aIsSmooth) {
  nsAutoString str;
  str.AppendInt(aNewPos);
  
  if (aIsSmooth) {
    aScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth, NS_LITERAL_STRING("true"), false);
  }
  aScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, str, aNotify);
  if (aIsSmooth) {
    aScrollbar->UnsetAttr(kNameSpaceID_None, nsGkAtoms::smooth, false);
  }
}

// Use this function when you want to set the scroll position via the position
// of the scrollbar thumb, e.g. when dragging the slider. This function scrolls
// the content in such a way that thumbRect.x/.y becomes aNewThumbPos.
void
nsSliderFrame::SetCurrentThumbPosition(nsIContent* aScrollbar, nscoord aNewThumbPos,
                                       bool aIsSmooth, bool aMaySnap)
{
  nsRect crect;
  GetClientRect(crect);
  nscoord offset = IsHorizontal() ? crect.x : crect.y;
  int32_t newPos = NSToIntRound((aNewThumbPos - offset) / mRatio);
  
  if (aMaySnap && mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::snap,
                                        nsGkAtoms::_true, eCaseMatters)) {
    // If snap="true", then the slider may only be set to min + (increment * x).
    // Otherwise, the slider may be set to any positive integer.
    int32_t increment = GetIncrement(aScrollbar);
    newPos = NSToIntRound(newPos / float(increment)) * increment;
  }
  
  SetCurrentPosition(aScrollbar, newPos, aIsSmooth);
}

// Use this function when you know the target scroll position of the scrolled content.
// aNewPos should be passed to this function as a position as if the minpos is 0.
// That is, the minpos will be added to the position by this function. In a reverse
// direction slider, the newpos should be the distance from the end.
void
nsSliderFrame::SetCurrentPosition(nsIContent* aScrollbar, int32_t aNewPos,
                                  bool aIsSmooth)
{
   // get min and max position from our content node
  int32_t minpos = GetMinPosition(aScrollbar);
  int32_t maxpos = GetMaxPosition(aScrollbar);

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

  SetCurrentPositionInternal(aScrollbar, aNewPos, aIsSmooth);
}

void
nsSliderFrame::SetCurrentPositionInternal(nsIContent* aScrollbar, int32_t aNewPos,
                                          bool aIsSmooth)
{
  nsCOMPtr<nsIContent> scrollbar = aScrollbar;
  nsIFrame* scrollbarBox = GetScrollbar();

  mUserChanged = true;

  nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
  if (scrollbarFrame) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbarFrame->GetScrollbarMediator();
    if (mediator) {
      nsRefPtr<nsPresContext> context = PresContext();
      nsCOMPtr<nsIContent> content = GetContent();
      mediator->PositionChanged(scrollbarFrame, GetCurrentPosition(scrollbar), aNewPos);
      // 'mediator' might be dangling now...
      UpdateAttribute(scrollbar, aNewPos, false, aIsSmooth);
      nsIFrame* frame = content->GetPrimaryFrame();
      if (frame && frame->GetType() == nsGkAtoms::sliderFrame) {
        static_cast<nsSliderFrame*>(frame)->CurrentPositionChanged();
      }
      mUserChanged = false;
      return;
    }
  }

  UpdateAttribute(scrollbar, aNewPos, true, aIsSmooth);
  mUserChanged = false;

#ifdef DEBUG_SLIDER
  printf("Current Pos=%d\n",aNewPos);
#endif

}

nsIAtom*
nsSliderFrame::GetType() const
{
  return nsGkAtoms::sliderFrame;
}

void
nsSliderFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  nsBoxFrame::SetInitialChildList(aListID, aChildList);
  AddListener();
}

nsresult
nsSliderMediator::HandleEvent(nsIDOMEvent* aEvent)
{
  // Only process the event if the thumb is not being dragged.
  if (mSlider && !mSlider->isDraggingThumb())
    return mSlider->StartDrag(aEvent);

  return NS_OK;
}

nsresult
nsSliderFrame::StartDrag(nsIDOMEvent* aEvent)
{
#ifdef DEBUG_SLIDER
  printf("Begin dragging\n");
#endif
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return NS_OK;

  WidgetGUIEvent* event = aEvent->GetInternalNSEvent()->AsGUIEvent();

  if (!ShouldScrollForEvent(event)) {
    return NS_OK;
  }

  nsPoint pt;
  if (!GetEventPoint(event, pt)) {
    return NS_OK;
  }
  bool isHorizontal = IsHorizontal();
  nscoord pos = isHorizontal ? pt.x : pt.y;

  // If we should scroll-to-click, first place the middle of the slider thumb
  // under the mouse.
  nsCOMPtr<nsIContent> scrollbar;
  nscoord newpos = pos;
  bool scrollToClick = ShouldScrollToClickForEvent(event);
  if (scrollToClick) {
    // adjust so that the middle of the thumb is placed under the click
    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      return NS_OK;
    }
    nsSize thumbSize = thumbFrame->GetSize();
    nscoord thumbLength = isHorizontal ? thumbSize.width : thumbSize.height;

    newpos -= (thumbLength/2);

    nsIFrame* scrollbarBox = GetScrollbar();
    scrollbar = GetContentOfBox(scrollbarBox);
  }

  DragThumb(true);

  if (scrollToClick) {
    // should aMaySnap be true here?
    SetCurrentThumbPosition(scrollbar, newpos, false, false);
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

nsresult
nsSliderFrame::StopDrag()
{
  AddListener();
  DragThumb(false);
  if (mChange) {
    StopRepeat();
    mChange = 0;
  }
  return NS_OK;
}

void
nsSliderFrame::DragThumb(bool aGrabMouseEvents)
{
  mDragFinished = !aGrabMouseEvents;

  // inform the parent <scale> that a drag is beginning or ending
  nsIFrame* parent = GetParent();
  if (parent) {
    nsCOMPtr<nsISliderListener> sliderListener = do_QueryInterface(parent->GetContent());
    if (sliderListener) {
      nsContentUtils::AddScriptRunner(
        new nsDragStateChangedRunnable(sliderListener, aGrabMouseEvents));
    }
  }

  nsIPresShell::SetCapturingContent(aGrabMouseEvents ? GetContent() : nullptr,
                                    aGrabMouseEvents ? CAPTURE_IGNOREALLOWED : 0);
}

bool
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
  if (!thumbFrame) {
    return;
  }
  thumbFrame->GetContent()->
    AddSystemEventListener(NS_LITERAL_STRING("mousedown"), mMediator,
                           false, false);
  thumbFrame->GetContent()->
    AddSystemEventListener(NS_LITERAL_STRING("touchstart"), mMediator,
                           false, false);
}

void
nsSliderFrame::RemoveListener()
{
  NS_ASSERTION(mMediator, "No listener was ever added!!");

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame)
    return;

  thumbFrame->GetContent()->
    RemoveSystemEventListener(NS_LITERAL_STRING("mousedown"), mMediator, false);
}

bool
nsSliderFrame::ShouldScrollForEvent(WidgetGUIEvent* aEvent)
{
  switch (aEvent->message) {
    case NS_TOUCH_START:
    case NS_TOUCH_END:
      return true;
    case NS_MOUSE_BUTTON_DOWN:
    case NS_MOUSE_BUTTON_UP: {
      uint16_t button = aEvent->AsMouseEvent()->button;
      return (button == WidgetMouseEvent::eLeftButton) ||
             (button == WidgetMouseEvent::eMiddleButton && gMiddlePref);
    }
    default:
      return false;
  }
}

bool
nsSliderFrame::ShouldScrollToClickForEvent(WidgetGUIEvent* aEvent)
{
  if (!ShouldScrollForEvent(aEvent)) {
    return false;
  }

  if (aEvent->message == NS_TOUCH_START) {
    return GetScrollToClick();
  }

  if (aEvent->message != NS_MOUSE_BUTTON_DOWN) {
    return false;
  }

#ifdef XP_MACOSX
  // On Mac, clicking the scrollbar thumb should never scroll to click.
  if (IsEventOverThumb(aEvent)) {
    return false;
  }
#endif

  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (mouseEvent->button == WidgetMouseEvent::eLeftButton) {
#ifdef XP_MACOSX
    bool invertPref = mouseEvent->IsAlt();
#else
    bool invertPref = mouseEvent->IsShift();
#endif
    return GetScrollToClick() != invertPref;
  }

  return true;
}

bool
nsSliderFrame::IsEventOverThumb(WidgetGUIEvent* aEvent)
{
  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame) {
    return false;
  }

  nsPoint eventPoint;
  if (!GetEventPoint(aEvent, eventPoint)) {
    return false;
  }

  bool isHorizontal = IsHorizontal();
  nsRect thumbRect = thumbFrame->GetRect();
  nscoord eventPos = isHorizontal ? eventPoint.x : eventPoint.y;
  nscoord thumbStart = isHorizontal ? thumbRect.x : thumbRect.y;
  nscoord thumbEnd = isHorizontal ? thumbRect.XMost() : thumbRect.YMost();

  return eventPos >= thumbStart && eventPos < thumbEnd;
}

NS_IMETHODIMP
nsSliderFrame::HandlePress(nsPresContext* aPresContext,
                           WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus)
{
  if (!ShouldScrollForEvent(aEvent) || ShouldScrollToClickForEvent(aEvent)) {
    return NS_OK;
  }

  if (IsEventOverThumb(aEvent)) {
    return NS_OK;
  }

  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (!thumbFrame) // display:none?
    return NS_OK;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return NS_OK;
  
  nsRect thumbRect = thumbFrame->GetRect();
  
  nscoord change = 1;
  nsPoint eventPoint;
  if (!GetEventPoint(aEvent, eventPoint)) {
    return NS_OK;
  }
  if (IsHorizontal() ? eventPoint.x < thumbRect.x 
                     : eventPoint.y < thumbRect.y)
    change = -1;

  mChange = change;
  DragThumb(true);
  mDestinationPoint = eventPoint;
  StartRepeat();
  PageUpDown(change);
  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::HandleRelease(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  StopRepeat();

  return NS_OK;
}

void
nsSliderFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // tell our mediator if we have one we are gone.
  if (mMediator) {
    mMediator->SetSlider(nullptr);
    mMediator = nullptr;
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
  nsIFrame* scrollbarBox = GetScrollbar();

  bool isHorizontal = (scrollbarBox->GetStateBits() & NS_STATE_IS_HORIZONTAL) != 0;
  if (isHorizontal)
      mState |= NS_STATE_IS_HORIZONTAL;
  else
      mState &= ~NS_STATE_IS_HORIZONTAL;
}


void nsSliderFrame::Notify(void)
{
    bool stop = false;

    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      StopRepeat();
      return;
    }
    nsRect thumbRect = thumbFrame->GetRect();

    bool isHorizontal = IsHorizontal();

    // See if the thumb has moved past our destination point.
    // if it has we want to stop.
    if (isHorizontal) {
        if (mChange < 0) {
            if (thumbRect.x < mDestinationPoint.x)
                stop = true;
        } else {
            if (thumbRect.x + thumbRect.width > mDestinationPoint.x)
                stop = true;
        }
    } else {
         if (mChange < 0) {
            if (thumbRect.y < mDestinationPoint.y)
                stop = true;
        } else {
            if (thumbRect.y + thumbRect.height > mDestinationPoint.y)
                stop = true;
        }
    }


    if (stop) {
      StopRepeat();
    } else {
      PageUpDown(mChange);
    }
}

NS_IMPL_ISUPPORTS(nsSliderMediator,
                  nsIDOMEventListener)
