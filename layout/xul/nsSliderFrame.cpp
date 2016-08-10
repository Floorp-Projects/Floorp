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

#include "gfxPrefs.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIDOMEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsScrollbarButtonFrame.h"
#include "nsISliderListener.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsISupportsImpl.h"
#include "nsScrollbarFrame.h"
#include "nsRepeatService.h"
#include "nsBoxLayoutState.h"
#include "nsSprocketLayout.h"
#include "nsIServiceManager.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/ScrollInputMethods.h"
#include <algorithm>

using namespace mozilla;
using mozilla::layers::APZCCallbackHelper;
using mozilla::layers::AsyncDragMetrics;
using mozilla::layers::InputAPZContext;
using mozilla::layers::ScrollInputMethod;

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
  return new (aPresShell) nsSliderFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSliderFrame)

NS_QUERYFRAME_HEAD(nsSliderFrame)
  NS_QUERYFRAME_ENTRY(nsSliderFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

nsSliderFrame::nsSliderFrame(nsStyleContext* aContext):
  nsBoxFrame(aContext),
  mCurPos(0),
  mChange(0),
  mDragFinished(true),
  mUserChanged(false),
  mScrollingWithAPZ(false),
  mSuppressionActive(false)
{
}

// stop timer
nsSliderFrame::~nsSliderFrame()
{
  if (mSuppressionActive) {
    APZCCallbackHelper::SuppressDisplayport(false, PresContext() ?
                                                   PresContext()->PresShell() :
                                                   nullptr);
  }
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

class nsValueChangedRunnable : public Runnable
{
public:
  nsValueChangedRunnable(nsISliderListener* aListener,
                         nsIAtom* aWhich,
                         int32_t aValue,
                         bool aUserChanged)
  : mListener(aListener), mWhich(aWhich),
    mValue(aValue), mUserChanged(aUserChanged)
  {}

  NS_IMETHOD Run() override
  {
    return mListener->ValueChanged(nsDependentAtomString(mWhich),
                                   mValue, mUserChanged);
  }

  nsCOMPtr<nsISliderListener> mListener;
  nsCOMPtr<nsIAtom> mWhich;
  int32_t mValue;
  bool mUserChanged;
};

class nsDragStateChangedRunnable : public Runnable
{
public:
  nsDragStateChangedRunnable(nsISliderListener* aListener,
                             bool aDragBeginning)
  : mListener(aListener),
    mDragBeginning(aDragBeginning)
  {}

  NS_IMETHOD Run() override
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
        int32_t direction = 0;
        if (current < min || max < min) {
          current = min;
          direction = -1;
        } else if (current > max) {
          current = max;
          direction = 1;
        }

        // set the new position and notify observers
        nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
        if (scrollbarFrame) {
          nsIScrollbarMediator* mediator = scrollbarFrame->GetScrollbarMediator();
          scrollbarFrame->SetIncrementToWhole(direction);
          if (mediator) {
            mediator->ScrollByWhole(scrollbarFrame, direction,
                                    nsIScrollbarMediator::ENABLE_SNAP);
          }
        }
        // 'this' might be destroyed here

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
  nsIFrame* thumb = nsBox::GetChildXULBox(this);

  if (thumb) {
    nsRect thumbRect(thumb->GetRect());
    nsMargin m;
    thumb->GetXULMargin(m);
    thumbRect.Inflate(m);

    nsRect crect;
    GetXULClientRect(crect);

    if (crect.width < thumbRect.width || crect.height < thumbRect.height)
      return;

    // If this scrollbar is the scrollbar of an actively scrolled scroll frame,
    // layerize the scrollbar thumb, wrap it in its own ContainerLayer and
    // attach scrolling information to it.
    // We do this here and not in the thumb's nsBoxFrame::BuildDisplayList so
    // that the event region that gets created for the thumb is included in
    // the nsDisplayOwnLayer contents.

    uint32_t flags = 0;
    mozilla::layers::FrameMetrics::ViewID scrollTargetId =
      mozilla::layers::FrameMetrics::NULL_SCROLL_ID;
    aBuilder->GetScrollbarInfo(&scrollTargetId, &flags);
    bool thumbGetsLayer = (scrollTargetId != layers::FrameMetrics::NULL_SCROLL_ID);
    nsLayoutUtils::SetScrollbarThumbLayerization(thumb, thumbGetsLayer);

    if (thumbGetsLayer) {
      nsDisplayListCollection tempLists;
      nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, tempLists);

      // This is a bit of a hack. Collect up all descendant display items
      // and merge them into a single Content() list.
      nsDisplayList masterList;
      masterList.AppendToTop(tempLists.BorderBackground());
      masterList.AppendToTop(tempLists.BlockBorderBackgrounds());
      masterList.AppendToTop(tempLists.Floats());
      masterList.AppendToTop(tempLists.Content());
      masterList.AppendToTop(tempLists.PositionedDescendants());
      masterList.AppendToTop(tempLists.Outlines());

      // Wrap the list to make it its own layer.
      aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayOwnLayer(aBuilder, this, &masterList, flags, scrollTargetId,
                          GetThumbRatio()));

      return;
    }
  }
  
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsSliderFrame::DoXULLayout(nsBoxLayoutState& aState)
{
  // get the thumb should be our only child
  nsIFrame* thumbBox = nsBox::GetChildXULBox(this);

  if (!thumbBox) {
    SyncLayout(aState);
    return NS_OK;
  }

  EnsureOrient();

#ifdef DEBUG_LAYOUT
  if (mState & NS_STATE_DEBUG_WAS_SET) {
      if (mState & NS_STATE_SET_TO_DEBUG)
          SetXULDebug(aState, true);
      else
          SetXULDebug(aState, false);
  }
#endif

  // get the content area inside our borders
  nsRect clientRect;
  GetXULClientRect(clientRect);

  // get the scrollbar
  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar;
  scrollbar = GetContentOfBox(scrollbarBox);

  // get the thumb's pref size
  nsSize thumbSize = thumbBox->GetXULPrefSize(aState);

  if (IsXULHorizontal())
    thumbSize.height = clientRect.height;
  else
    thumbSize.width = clientRect.width;

  int32_t curPos = GetCurrentPosition(scrollbar);
  int32_t minPos = GetMinPosition(scrollbar);
  int32_t maxPos = GetMaxPosition(scrollbar);
  int32_t pageIncrement = GetPageIncrement(scrollbar);

  maxPos = std::max(minPos, maxPos);
  curPos = clamped(curPos, minPos, maxPos);

  nscoord& availableLength = IsXULHorizontal() ? clientRect.width : clientRect.height;
  nscoord& thumbLength = IsXULHorizontal() ? thumbSize.width : thumbSize.height;

  if ((pageIncrement + maxPos - minPos) > 0 && thumbBox->GetXULFlex() > 0) {
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
  int32_t& thumbPos = (IsXULHorizontal() ? thumbRect.x : thumbRect.y);
  thumbPos += NSToCoordRound(pos * mRatio);

  nsRect oldThumbRect(thumbBox->GetRect());
  LayoutChildAt(aState, thumbBox, thumbRect);

  SyncLayout(aState);

  // Redraw only if thumb changed size.
  if (!oldThumbRect.IsEqualInterior(thumbRect))
    XULRedraw(aState);

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
  bool isHorizontal = IsXULHorizontal();

  if (isDraggingThumb())
  {
    switch (aEvent->mMessage) {
    case eTouchMove:
    case eMouseMove: {
      if (mScrollingWithAPZ) {
        break;
      }
      nsPoint eventPoint;
      if (!GetEventPoint(aEvent, eventPoint)) {
        break;
      }
      if (mChange) {
        // On Linux the destination point is determined by the initial click
        // on the scrollbar track and doesn't change until the mouse button
        // is released.
#ifndef MOZ_WIDGET_GTK
        // On the other platforms we need to update the destination point now.
        mDestinationPoint = eventPoint;
        StopRepeat();
        StartRepeat();
#endif
        break;
      }

      nscoord pos = isHorizontal ? eventPoint.x : eventPoint.y;

      nsIFrame* thumbFrame = mFrames.FirstChild();
      if (!thumbFrame) {
        return NS_OK;
      }

      mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
          (uint32_t) ScrollInputMethod::MainThreadScrollbarDrag);

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

    case eTouchEnd:
    case eMouseUp:
      if (ShouldScrollForEvent(aEvent)) {
        StopDrag();
        //we MUST call nsFrame HandleEvent for mouse ups to maintain the selection state and capture state.
        return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
      }
      break;

    default:
      break;
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

    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
        (uint32_t) ScrollInputMethod::MainThreadScrollbarTrackClick);

    // set it
    nsWeakFrame weakFrame(this);
    // should aMaySnap be true here?
    SetCurrentThumbPosition(scrollbar, pos - thumbLength/2, false, false);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

    DragThumb(true);

#ifdef MOZ_WIDGET_GTK
    nsCOMPtr<nsIContent> thumb = thumbFrame->GetContent();
    thumb->SetAttr(kNameSpaceID_None, nsGkAtoms::active, NS_LITERAL_STRING("true"), true);
#endif

    if (aEvent->mClass == eTouchEventClass) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
    }

    if (isHorizontal)
      mThumbStart = thumbFrame->GetPosition().x;
    else
      mThumbStart = thumbFrame->GetPosition().y;

    mDragStart = pos - mThumbStart;
  }
#ifdef MOZ_WIDGET_GTK
  else if (ShouldScrollForEvent(aEvent) &&
           aEvent->mClass == eMouseEventClass &&
           aEvent->AsMouseEvent()->button == WidgetMouseEvent::eRightButton) {
    // HandlePress and HandleRelease are usually called via
    // nsFrame::HandleEvent, but only for the left mouse button.
    if (aEvent->mMessage == eMouseDown) {
      HandlePress(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->mMessage == eMouseUp) {
      HandleRelease(aPresContext, aEvent, aEventStatus);
    }

    return NS_OK;
  }
#endif

  // XXX hack until handle release is actually called in nsframe.
  //  if (aEvent->mMessage == eMouseOut ||
  //      aEvent->mMessage == NS_MOUSE_RIGHT_BUTTON_UP ||
  //      aEvent->mMessage == NS_MOUSE_LEFT_BUTTON_UP) {
  //    HandleRelease(aPresContext, aEvent, aEventStatus);
  //  }

  if (aEvent->mMessage == eMouseOut && mChange)
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

   return scrollbar->IsXULBoxFrame() ? scrollbar : this;
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
  GetXULClientRect(clientRect);

  // figure out the new rect
  nsRect newThumbRect(thumbRect);

  bool reverse = mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                                         nsGkAtoms::reverse, eCaseMatters);
  nscoord pos = reverse ? (maxPos - curPos) : (curPos - minPos);

  if (IsXULHorizontal())
     newThumbRect.x = clientRect.x + NSToCoordRound(pos * mRatio);
  else
     newThumbRect.y = clientRect.y + NSToCoordRound(pos * mRatio);

  // avoid putting the scroll thumb at subpixel positions which cause needless invalidations
  nscoord appUnitsPerPixel = PresContext()->AppUnitsPerDevPixel();
  nsPoint snappedThumbLocation = ToAppUnits(
      newThumbRect.TopLeft().ToNearestPixels(appUnitsPerPixel),
      appUnitsPerPixel);
  if (IsXULHorizontal()) {
    newThumbRect.x = snappedThumbLocation.x;
  } else {
    newThumbRect.y = snappedThumbLocation.y;
  }

  // set the rect
  thumbFrame->SetRect(newThumbRect);

  // Request a repaint of the scrollbar
  nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
  nsIScrollbarMediator* mediator = scrollbarFrame
      ? scrollbarFrame->GetScrollbarMediator() : nullptr;
  if (!mediator || !mediator->ShouldSuppressScrollbarRepaints()) {
    SchedulePaint();
  }

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
  GetXULClientRect(crect);
  nscoord offset = IsXULHorizontal() ? crect.x : crect.y;
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
  nsWeakFrame weakFrame(this);

  mUserChanged = true;

  nsScrollbarFrame* scrollbarFrame = do_QueryFrame(scrollbarBox);
  if (scrollbarFrame) {
    // See if we have a mediator.
    nsIScrollbarMediator* mediator = scrollbarFrame->GetScrollbarMediator();
    if (mediator) {
      nsCOMPtr<nsIContent> content = GetContent();
      nscoord oldPos = nsPresContext::CSSPixelsToAppUnits(GetCurrentPosition(scrollbar));
      nscoord newPos = nsPresContext::CSSPixelsToAppUnits(aNewPos);
      mediator->ThumbMoved(scrollbarFrame, oldPos, newPos);
      if (!weakFrame.IsAlive()) {
        return;
      }
      CurrentPositionChanged();
      mUserChanged = false;
      return;
    }
  }

  UpdateAttribute(scrollbar, aNewPos, true, aIsSmooth);
  if (!weakFrame.IsAlive()) {
    return;
  }
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
  if (aListID == kPrincipalList) {
    AddListener();
  }
}

nsresult
nsSliderMediator::HandleEvent(nsIDOMEvent* aEvent)
{
  // Only process the event if the thumb is not being dragged.
  if (mSlider && !mSlider->isDraggingThumb())
    return mSlider->StartDrag(aEvent);

  return NS_OK;
}

bool
nsSliderFrame::StartAPZDrag()
{
  if (!gfxPlatform::GetPlatform()->SupportsApzDragInput()) {
    return false;
  }

  nsContainerFrame* cf = GetScrollbar()->GetParent();
  if (!cf) {
    return false;
  }

  nsIContent* scrollableContent = cf->GetContent();
  if (!scrollableContent) {
    return false;
  }

  mozilla::layers::FrameMetrics::ViewID scrollTargetId;
  bool hasID = nsLayoutUtils::FindIDFor(scrollableContent, &scrollTargetId);
  bool hasAPZView = hasID && (scrollTargetId != layers::FrameMetrics::NULL_SCROLL_ID);

  if (!hasAPZView) {
    return false;
  }

  nsIFrame* scrollbarBox = GetScrollbar();
  nsCOMPtr<nsIContent> scrollbar = GetContentOfBox(scrollbarBox);

  // This rect is the range in which the scroll thumb can slide in.
  nsRect sliderTrack = GetRect() - scrollbarBox->GetPosition();
  CSSIntRect sliderTrackCSS = CSSIntRect::FromAppUnitsRounded(sliderTrack);

  uint64_t inputblockId = InputAPZContext::GetInputBlockId();
  uint32_t presShellId = PresContext()->PresShell()->GetPresShellId();
  AsyncDragMetrics dragMetrics(scrollTargetId, presShellId, inputblockId,
                               NSAppUnitsToIntPixels(mDragStart,
                                 float(AppUnitsPerCSSPixel())),
                               sliderTrackCSS,
                               IsXULHorizontal() ? AsyncDragMetrics::HORIZONTAL :
                                                   AsyncDragMetrics::VERTICAL);

  if (!nsLayoutUtils::HasDisplayPort(scrollableContent)) {
    return false;
  }

  // When we start an APZ drag, we wont get mouse events for the drag.
  // APZ will consume them all and only notify us of the new scroll position.
  this->GetNearestWidget()->StartAsyncScrollbarDrag(dragMetrics);
  return true;
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

  WidgetGUIEvent* event = aEvent->WidgetEventPtr()->AsGUIEvent();

  if (!ShouldScrollForEvent(event)) {
    return NS_OK;
  }

  nsPoint pt;
  if (!GetEventPoint(event, pt)) {
    return NS_OK;
  }
  bool isHorizontal = IsXULHorizontal();
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

#ifdef MOZ_WIDGET_GTK
  nsCOMPtr<nsIContent> thumb = thumbFrame->GetContent();
  thumb->SetAttr(kNameSpaceID_None, nsGkAtoms::active, NS_LITERAL_STRING("true"), true);
#endif

  if (isHorizontal)
    mThumbStart = thumbFrame->GetPosition().x;
  else
    mThumbStart = thumbFrame->GetPosition().y;

  mDragStart = pos - mThumbStart;

  mScrollingWithAPZ = StartAPZDrag();

#ifdef DEBUG_SLIDER
  printf("Pressed mDragStart=%d\n",mDragStart);
#endif

  if (!mScrollingWithAPZ && !mSuppressionActive) {
    MOZ_ASSERT(PresContext()->PresShell());
    APZCCallbackHelper::SuppressDisplayport(true, PresContext()->PresShell());
    mSuppressionActive = true;
  }

  return NS_OK;
}

nsresult
nsSliderFrame::StopDrag()
{
  AddListener();
  DragThumb(false);

  mScrollingWithAPZ = false;

  if (mSuppressionActive) {
    MOZ_ASSERT(PresContext()->PresShell());
    APZCCallbackHelper::SuppressDisplayport(false, PresContext()->PresShell());
    mSuppressionActive = false;
  }

#ifdef MOZ_WIDGET_GTK
  nsIFrame* thumbFrame = mFrames.FirstChild();
  if (thumbFrame) {
    nsCOMPtr<nsIContent> thumb = thumbFrame->GetContent();
    thumb->UnsetAttr(kNameSpaceID_None, nsGkAtoms::active, true);
  }
#endif

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
  switch (aEvent->mMessage) {
    case eTouchStart:
    case eTouchEnd:
      return true;
    case eMouseDown:
    case eMouseUp: {
      uint16_t button = aEvent->AsMouseEvent()->button;
#ifdef MOZ_WIDGET_GTK
      return (button == WidgetMouseEvent::eLeftButton) ||
             (button == WidgetMouseEvent::eRightButton && GetScrollToClick()) ||
             (button == WidgetMouseEvent::eMiddleButton && gMiddlePref && !GetScrollToClick());
#else
      return (button == WidgetMouseEvent::eLeftButton) ||
             (button == WidgetMouseEvent::eMiddleButton && gMiddlePref);
#endif
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

  if (aEvent->mMessage == eTouchStart) {
    return GetScrollToClick();
  }

  if (aEvent->mMessage != eMouseDown) {
    return false;
  }

#if defined(XP_MACOSX) || defined(MOZ_WIDGET_GTK)
  // On Mac and Linux, clicking the scrollbar thumb should never scroll to click.
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

#ifdef MOZ_WIDGET_GTK
  if (mouseEvent->button == WidgetMouseEvent::eRightButton) {
    return !GetScrollToClick();
  }
#endif

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

  nsRect thumbRect = thumbFrame->GetRect();
#if defined(MOZ_WIDGET_GTK)
  /* Scrollbar track can have padding, so it's better to check that eventPoint
   * is inside of actual thumb, not just its one axis. The part of the scrollbar
   * track adjacent to thumb can actually receive events in GTK3 */
  return eventPoint.x >= thumbRect.x && eventPoint.x < thumbRect.XMost() &&
         eventPoint.y >= thumbRect.y && eventPoint.y < thumbRect.YMost();
#else
  bool isHorizontal = IsXULHorizontal();
  nscoord eventPos = isHorizontal ? eventPoint.x : eventPoint.y;
  nscoord thumbStart = isHorizontal ? thumbRect.x : thumbRect.y;
  nscoord thumbEnd = isHorizontal ? thumbRect.XMost() : thumbRect.YMost();

  return eventPos >= thumbStart && eventPos < thumbEnd;
#endif
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
  if (IsXULHorizontal() ? eventPoint.x < thumbRect.x 
                        : eventPoint.y < thumbRect.y)
    change = -1;

  mChange = change;
  DragThumb(true);
  // On Linux we want to keep scrolling in the direction indicated by |change|
  // until the mouse is released. On the other platforms we want to stop
  // scrolling as soon as the scrollbar thumb has reached the current mouse
  // position.
#ifdef MOZ_WIDGET_GTK
  nsRect clientRect;
  GetXULClientRect(clientRect);

  // Set the destination point to the very end of the scrollbar so that
  // scrolling doesn't stop halfway through.
  if (change > 0) {
    mDestinationPoint = nsPoint(clientRect.width, clientRect.height);
  }
  else {
    mDestinationPoint = nsPoint(0, 0);
  }
#else
  mDestinationPoint = eventPoint;
#endif
  StartRepeat();
  PageScroll(change);

  return NS_OK;
}

NS_IMETHODIMP
nsSliderFrame::HandleRelease(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  StopRepeat();

  nsIFrame* scrollbar = GetScrollbar();
  nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
  if (sb) {
    nsIScrollbarMediator* m = sb->GetScrollbarMediator();
    if (m) {
      m->ScrollbarReleased(sb);
    }
  }
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
nsSliderFrame::GetXULPrefSize(nsBoxLayoutState& aState)
{
  EnsureOrient();
  return nsBoxFrame::GetXULPrefSize(aState);
}

nsSize
nsSliderFrame::GetXULMinSize(nsBoxLayoutState& aState)
{
  EnsureOrient();

  // our min size is just our borders and padding
  return nsBox::GetXULMinSize(aState);
}

nsSize
nsSliderFrame::GetXULMaxSize(nsBoxLayoutState& aState)
{
  EnsureOrient();
  return nsBoxFrame::GetXULMaxSize(aState);
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


void
nsSliderFrame::Notify(void)
{
    bool stop = false;

    nsIFrame* thumbFrame = mFrames.FirstChild();
    if (!thumbFrame) {
      StopRepeat();
      return;
    }
    nsRect thumbRect = thumbFrame->GetRect();

    bool isHorizontal = IsXULHorizontal();

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
      PageScroll(mChange);
    }
}

void
nsSliderFrame::PageScroll(nscoord aChange)
{
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                            nsGkAtoms::reverse, eCaseMatters)) {
    aChange = -aChange;
  }
  nsIFrame* scrollbar = GetScrollbar();
  nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
  if (sb) {
    nsIScrollbarMediator* m = sb->GetScrollbarMediator();
    sb->SetIncrementToPage(aChange);
    if (m) {
      m->ScrollByPage(sb, aChange, nsIScrollbarMediator::ENABLE_SNAP);
      return;
    }
  }
  PageUpDown(aChange);
}

float
nsSliderFrame::GetThumbRatio() const
{
  // mRatio is in thumb app units per scrolled css pixels. Convert it to a
  // ratio of the thumb's CSS pixels per scrolled CSS pixels. (Note the thumb
  // is in the scrollframe's parent's space whereas the scrolled CSS pixels
  // are in the scrollframe's space).
  return mRatio / mozilla::AppUnitsPerCSSPixel();
}

NS_IMPL_ISUPPORTS(nsSliderMediator,
                  nsIDOMEventListener)
