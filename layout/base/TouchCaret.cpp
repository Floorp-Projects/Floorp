/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "TouchCaret.h"

#include <algorithm>

#include "nsBlockFrame.h"
#include "nsCanvasFrame.h"
#include "nsCaret.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsFrameSelection.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMWindow.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsPresContext.h"
#include "nsQueryContentEventResult.h"
#include "nsView.h"
#include "mozilla/dom/SelectionStateChangedEvent.h"
#include "mozilla/dom/CustomEvent.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

static PRLogModuleInfo* gTouchCaretLog;
static const char* kTouchCaretLogModuleName = "TouchCaret";

// To enable all the TOUCHCARET_LOG print statements, set the environment
// variable NSPR_LOG_MODULES=TouchCaret:5
#define TOUCHCARET_LOG(message, ...)                                           \
  MOZ_LOG(gTouchCaretLog, LogLevel::Debug,                                         \
         ("TouchCaret (%p): %s:%d : " message "\n", this, __FUNCTION__,        \
          __LINE__, ##__VA_ARGS__));

#define TOUCHCARET_LOG_STATIC(message, ...)                                    \
  MOZ_LOG(gTouchCaretLog, LogLevel::Debug,                                         \
         ("TouchCaret: %s:%d : " message "\n", __FUNCTION__, __LINE__,         \
          ##__VA_ARGS__));

// Click on the boundary of input/textarea will place the caret at the
// front/end of the content. To advoid this, we need to deflate the content
// boundary by 61 app units (1 pixel + 1 app unit).
static const int32_t kBoundaryAppUnits = 61;

NS_IMPL_ISUPPORTS(TouchCaret,
                  nsISelectionListener,
                  nsIScrollObserver,
                  nsISupportsWeakReference)

/*static*/ int32_t TouchCaret::sTouchCaretInflateSize = 0;
/*static*/ int32_t TouchCaret::sTouchCaretExpirationTime = 0;
/*static*/ bool TouchCaret::sCaretManagesAndroidActionbar = false;
/*static*/ bool TouchCaret::sTouchcaretExtendedvisibility = false;

/*static*/ uint32_t TouchCaret::sActionBarViewCount = 0;

TouchCaret::TouchCaret(nsIPresShell* aPresShell)
  : mState(TOUCHCARET_NONE),
    mActiveTouchId(-1),
    mCaretCenterToDownPointOffsetY(0),
    mInAsyncPanZoomGesture(false),
    mVisible(false),
    mIsValidTap(false),
    mActionBarViewID(0)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gTouchCaretLog) {
    gTouchCaretLog = PR_NewLogModule(kTouchCaretLogModuleName);
  }

  TOUCHCARET_LOG("Constructor, PresShell=%p", aPresShell);

  static bool addedTouchCaretPref = false;
  if (!addedTouchCaretPref) {
    Preferences::AddIntVarCache(&sTouchCaretInflateSize,
                                "touchcaret.inflatesize.threshold");
    Preferences::AddIntVarCache(&sTouchCaretExpirationTime,
                                "touchcaret.expiration.time");
    Preferences::AddBoolVarCache(&sCaretManagesAndroidActionbar,
                                 "caret.manages-android-actionbar");
    Preferences::AddBoolVarCache(&sTouchcaretExtendedvisibility,
                                 "touchcaret.extendedvisibility");
    addedTouchCaretPref = true;
  }

  // The presshell owns us, so no addref.
  mPresShell = do_GetWeakReference(aPresShell);
  MOZ_ASSERT(mPresShell, "Hey, pres shell should support weak refs");
}

void
TouchCaret::Init()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

  docShell->AddWeakScrollObserver(this);
  mDocShell = static_cast<nsDocShell*>(docShell);
}

void
TouchCaret::Terminate()
{
  nsRefPtr<nsDocShell> docShell(mDocShell.get());
  if (docShell) {
    docShell->RemoveWeakScrollObserver(this);
  }

  if (mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer->Cancel();
    mScrollEndDetectorTimer = nullptr;
  }

  mDocShell = WeakPtr<nsDocShell>();
  mPresShell = nullptr;
}

TouchCaret::~TouchCaret()
{
  TOUCHCARET_LOG("Destructor");
  MOZ_ASSERT(NS_IsMainThread());

  if (mTouchCaretExpirationTimer) {
    mTouchCaretExpirationTimer->Cancel();
    mTouchCaretExpirationTimer = nullptr;
  }
}

nsIFrame*
TouchCaret::GetCaretFocusFrame(nsRect* aOutRect)
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return nullptr;
  }

  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return nullptr;
  }

  nsRect rect;
  nsIFrame* frame = caret->GetGeometry(&rect);

  if (aOutRect) {
    *aOutRect = rect;
  }

  return frame;
}

nsCanvasFrame*
TouchCaret::GetCanvasFrame()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetCanvasFrame();
}

nsIFrame*
TouchCaret::GetRootFrame()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetRootFrame();
}

void
TouchCaret::SetVisibility(bool aVisible)
{
  if (mVisible == aVisible) {
    TOUCHCARET_LOG("Set visibility %s, same as the old one",
                   (aVisible ? "shown" : "hidden"));
    return;
  }

  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return;
  }

  mozilla::dom::Element* touchCaretElement = presShell->GetTouchCaretElement();
  if (!touchCaretElement) {
    return;
  }

  mVisible = aVisible;

  // Set touch caret visibility.
  ErrorResult err;
  touchCaretElement->ClassList()->Toggle(NS_LITERAL_STRING("hidden"),
                                         dom::Optional<bool>(!mVisible),
                                         err);
  TOUCHCARET_LOG("Set visibility %s", (mVisible ? "shown" : "hidden"));

  // Set touch caret expiration time.
  mVisible ? LaunchExpirationTimer() : CancelExpirationTimer();

  // If after a TouchCaret visibility change we become hidden, ensure
  // the Android ActionBar handler is notified to close the current view.
  if (!mVisible && sCaretManagesAndroidActionbar) {
    UpdateAndroidActionBarVisibility(false, mActionBarViewID);
  }
}

/**
 * Open or close the Android TextSelection ActionBar, based on visibility.
 * Each time we're called to open the actionbar, we increment / assign a
 * unique view ID and return it to the caller. The ID is returned on calls
 * to close the actionbar to ensure we don't close the shared view if it
 * was already force closed by a subsequent callers open request.
 */
/* static */void
TouchCaret::UpdateAndroidActionBarVisibility(bool aVisibility, uint32_t& aViewID)
{
  // Are we openning a new view?
  if (aVisibility) {
    // Assign a new view ID.
    aViewID = ++sActionBarViewCount;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    nsString topic = (aVisibility) ?
      NS_LITERAL_STRING("ActionBar:OpenNew") : NS_LITERAL_STRING("ActionBar:Close");
    nsAutoString viewCount;
    viewCount.AppendInt(aViewID);
    os->NotifyObservers(nullptr, NS_ConvertUTF16toUTF8(topic).get(), viewCount.get());
  }
}

nsRect
TouchCaret::GetTouchFrameRect()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return nsRect();
  }

  dom::Element* touchCaretElement = presShell->GetTouchCaretElement();
  nsIFrame* canvasFrame = GetCanvasFrame();
  return nsLayoutUtils::GetRectRelativeToFrame(touchCaretElement, canvasFrame);
}

nsRect
TouchCaret::GetContentBoundary()
{
  nsIFrame* focusFrame = GetCaretFocusFrame();
  nsIFrame* canvasFrame = GetCanvasFrame();
  if (!focusFrame || !canvasFrame) {
    return nsRect();
  }

  // Get the editing host to determine the touch caret dragable boundary.
  dom::Element* editingHost = focusFrame->GetContent()->GetEditingHost();
  if (!editingHost) {
    return nsRect();
  }

  nsRect resultRect;
  for (nsIFrame* frame = editingHost->GetPrimaryFrame(); frame;
       frame = frame->GetNextContinuation()) {
    nsRect rect = frame->GetContentRectRelativeToSelf();
    nsLayoutUtils::TransformRect(frame, canvasFrame, rect);
    resultRect = resultRect.Union(rect);

    mozilla::layout::FrameChildListIterator lists(frame);
    for (; !lists.IsDone(); lists.Next()) {
      // Loop over all children to take the overflow rect in to consideration.
      nsFrameList::Enumerator childFrames(lists.CurrentList());
      for (; !childFrames.AtEnd(); childFrames.Next()) {
        nsIFrame* kid = childFrames.get();
        nsRect overflowRect = kid->GetScrollableOverflowRect();
        nsLayoutUtils::TransformRect(kid, canvasFrame, overflowRect);
        resultRect = resultRect.Union(overflowRect);
      }
    }
  }
  // Shrink rect to make sure we never hit the boundary.
  resultRect.Deflate(kBoundaryAppUnits);

  return resultRect;
}

nscoord
TouchCaret::GetCaretYCenterPosition()
{
  nsRect caretRect;
  nsIFrame* focusFrame = GetCaretFocusFrame(&caretRect);
  nsIFrame* canvasFrame = GetCanvasFrame();

  nsLayoutUtils::TransformRect(focusFrame, canvasFrame, caretRect);

  return (caretRect.y + caretRect.height / 2);
}

void
TouchCaret::SetTouchFramePos(const nsRect& aCaretRect)
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return;
  }

  mozilla::dom::Element* touchCaretElement = presShell->GetTouchCaretElement();
  if (!touchCaretElement) {
    return;
  }

  // Convert aOrigin to CSS pixels.
  nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();
  int32_t x = presContext->AppUnitsToIntCSSPixels(aCaretRect.Center().x);
  int32_t y = presContext->AppUnitsToIntCSSPixels(aCaretRect.y);
  int32_t padding = presContext->AppUnitsToIntCSSPixels(aCaretRect.height);

  nsAutoString styleStr;
  styleStr.AppendLiteral("left: ");
  styleStr.AppendInt(x);
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendInt(y);
  styleStr.AppendLiteral("px; padding-top: ");
  styleStr.AppendInt(padding);
  styleStr.AppendLiteral("px;");

  TOUCHCARET_LOG("Set style: %s", NS_ConvertUTF16toUTF8(styleStr).get());

  touchCaretElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style,
                             styleStr, true);
}

void
TouchCaret::MoveCaret(const nsPoint& movePoint)
{
  nsIFrame* focusFrame = GetCaretFocusFrame();
  nsIFrame* canvasFrame = GetCanvasFrame();
  if (!focusFrame && !canvasFrame) {
    return;
  }
  nsIFrame* scrollable =
    nsLayoutUtils::GetClosestFrameOfType(focusFrame, nsGkAtoms::scrollFrame);

  // Convert touch/mouse position to frame coordinates.
  nsPoint offsetToCanvasFrame = nsPoint(0,0);
  nsLayoutUtils::TransformPoint(scrollable, canvasFrame, offsetToCanvasFrame);
  nsPoint pt = movePoint - offsetToCanvasFrame;

  // Evaluate offsets.
  nsIFrame::ContentOffsets offsets =
    scrollable->GetContentOffsetsFromPoint(pt, nsIFrame::SKIP_HIDDEN);

  // Move caret position.
  nsWeakFrame weakScrollable = scrollable;
  nsRefPtr<nsFrameSelection> fs = scrollable->GetFrameSelection();
  fs->HandleClick(offsets.content, offsets.StartOffset(),
                  offsets.EndOffset(),
                  false,
                  false,
                  offsets.associate);

  if (!weakScrollable.IsAlive()) {
    return;
  }

  // Scroll scrolled frame.
  nsIScrollableFrame* saf = do_QueryFrame(scrollable);
  nsIFrame* capturingFrame = saf->GetScrolledFrame();
  offsetToCanvasFrame = nsPoint(0,0);
  nsLayoutUtils::TransformPoint(capturingFrame, canvasFrame, offsetToCanvasFrame);
  pt = movePoint - offsetToCanvasFrame;
  fs->StartAutoScrollTimer(capturingFrame, pt, sAutoScrollTimerDelay);
}

bool
TouchCaret::IsOnTouchCaret(const nsPoint& aPoint)
{
  return mVisible && nsLayoutUtils::ContainsPoint(GetTouchFrameRect(), aPoint,
                                                  TouchCaretInflateSize());
}

nsresult
TouchCaret::NotifySelectionChanged(nsIDOMDocument* aDoc, nsISelection* aSel,
                                   int16_t aReason)
{
  TOUCHCARET_LOG("aSel (%p), Reason=%d", aSel, aReason);

  // Hide touch caret while no caret exists.
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return NS_OK;
  }

  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    SetVisibility(false);
    return NS_OK;
  }

  // The same touch caret is shared amongst the document and any text widgets it
  // may contain. This means that the touch caret could get notifications from
  // multiple selections.
  // If this notification is for a selection that is not the one the
  // the caret is currently interested in , then there is nothing to do!
  if (aSel != caret->GetSelection()) {
    TOUCHCARET_LOG("Return for selection mismatch!");
    return NS_OK;
  }

  // Update touch caret position and visibility.
  // Hide touch caret while key event causes selection change.
  // Also hide touch caret when gecko or javascript collapse the selection.
  if (aReason & nsISelectionListener::KEYPRESS_REASON ||
      aReason & nsISelectionListener::COLLAPSETOSTART_REASON ||
      aReason & nsISelectionListener::COLLAPSETOEND_REASON) {
    TOUCHCARET_LOG("KEYPRESS_REASON");
    SetVisibility(false);
  } else {
    SyncVisibilityWithCaret();

    // Is the TouchCaret visible and we're showing/hiding the actionbar?
    if (mVisible && sCaretManagesAndroidActionbar) {
      // A selection change due to touch tap opens the actionbar.
      if (aReason & nsISelectionListener::MOUSEUP_REASON) {
        UpdateAndroidActionBarVisibility(true, mActionBarViewID);
      } else {
        // Update the ActionBar state for caret-specific selection changes.
        // Ignore transient selection composition changes that occur while
        // the TouchCaret is also visible.
        bool isCollapsed;
        if (NS_SUCCEEDED(aSel->GetIsCollapsed(&isCollapsed)) && isCollapsed) {
          nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
          if (os) {
            os->NotifyObservers(nullptr, "ActionBar:UpdateState", nullptr);
          }
        }
      }
    }
  }

  return NS_OK;
}

/**
 * Used to update caret position after PanZoom stops for
 * extended caret visibility. Never needed by MOZ_WIDGET_GONK.
 */
void
TouchCaret::AsyncPanZoomStarted()
{
  if (mVisible) {
    if (sTouchcaretExtendedvisibility) {
      mInAsyncPanZoomGesture = true;
    }
  }
}

void
TouchCaret::AsyncPanZoomStopped()
{
  if (mInAsyncPanZoomGesture) {
    mInAsyncPanZoomGesture = false;
    UpdatePosition();
  }
}

/**
 * Used to update caret position after Scroll stops for
 * extended caret visibility. Never needed by MOZ_WIDGET_GONK.
 */
void
TouchCaret::ScrollPositionChanged()
{
  if (mVisible) {
    if (sTouchcaretExtendedvisibility) {
      // Launch scroll end detector.
      LaunchScrollEndDetector();
    }
  }
}

void
TouchCaret::LaunchScrollEndDetector()
{
  if (!mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
  MOZ_ASSERT(mScrollEndDetectorTimer);

  mScrollEndDetectorTimer->InitWithFuncCallback(FireScrollEnd,
                                                this,
                                                sScrollEndTimerDelay,
                                                nsITimer::TYPE_ONE_SHOT);
}

void
TouchCaret::CancelScrollEndDetector()
{
  if (mScrollEndDetectorTimer) {
    mScrollEndDetectorTimer->Cancel();
  }
}


/* static */void
TouchCaret::FireScrollEnd(nsITimer* aTimer, void* aTouchCaret)
{
  nsRefPtr<TouchCaret> self = static_cast<TouchCaret*>(aTouchCaret);
  NS_PRECONDITION(aTimer == self->mScrollEndDetectorTimer,
                  "Unexpected timer");
  self->UpdatePosition();
}

void
TouchCaret::SyncVisibilityWithCaret()
{
  TOUCHCARET_LOG("SyncVisibilityWithCaret");

  if (!IsDisplayable()) {
    SetVisibility(false);
    return;
  }

  SetVisibility(true);
  if (mVisible) {
    UpdatePosition();
  }
}

void
TouchCaret::UpdatePositionIfNeeded()
{
  TOUCHCARET_LOG("UpdatePositionIfNeeded");

  if (!IsDisplayable()) {
    SetVisibility(false);
    return;
  }

  if (mVisible) {
    UpdatePosition();
  }
}

bool
TouchCaret::IsDisplayable()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    TOUCHCARET_LOG("PresShell is nullptr!");
    return false;
  }

  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    TOUCHCARET_LOG("Caret is nullptr!");
    return false;
  }

  nsIFrame* canvasFrame = GetCanvasFrame();
  if (!canvasFrame) {
    TOUCHCARET_LOG("No canvas frame!");
    return false;
  }

  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    TOUCHCARET_LOG("No root frame!");
    return false;
  }

  dom::Element* touchCaretElement = presShell->GetTouchCaretElement();
  if (!touchCaretElement) {
    TOUCHCARET_LOG("No touch caret frame element!");
    return false;
  }

  if (presShell->IsPaintingSuppressed()) {
    TOUCHCARET_LOG("PresShell is suppressing painting!");
    return false;
  }

  if (!caret->IsVisible()) {
    TOUCHCARET_LOG("Caret is not visible!");
    return false;
  }

  nsRect focusRect;
  nsIFrame* focusFrame = caret->GetGeometry(&focusRect);
  if (!focusFrame) {
    TOUCHCARET_LOG("Focus frame is not valid!");
    return false;
  }

  dom::Element* editingHost = focusFrame->GetContent()->GetEditingHost();
  if (!editingHost) {
    TOUCHCARET_LOG("Cannot get editing host!");
    return false;
  }

  // No further checks required if extended TouchCaret visibility.
  if (sTouchcaretExtendedvisibility) {
    return true;
  }

  if (focusRect.IsEmpty()) {
    TOUCHCARET_LOG("Focus rect is empty!");
    return false;
  }

  if (!nsContentUtils::HasNonEmptyTextContent(
         editingHost, nsContentUtils::eRecurseIntoChildren)) {
    TOUCHCARET_LOG("The content is empty!");
    return false;
  }

  if (mState != TOUCHCARET_TOUCHDRAG_ACTIVE &&
        !nsLayoutUtils::IsRectVisibleInScrollFrames(focusFrame, focusRect)) {
    TOUCHCARET_LOG("Caret does not show in the scrollable frame!");
    return false;
  }

  TOUCHCARET_LOG("Touch caret is displayable!");
  return true;
}

void
TouchCaret::UpdatePosition()
{
  MOZ_ASSERT(mVisible);

  nsRect rect = GetTouchCaretRect();
  rect = ClampRectToScrollFrame(rect);
  SetTouchFramePos(rect);
}

nsRect
TouchCaret::GetTouchCaretRect()
{
  nsRect focusRect;
  nsIFrame* focusFrame = GetCaretFocusFrame(&focusRect);
  nsIFrame* rootFrame = GetRootFrame();
  // Transform the position to make it relative to root frame.
  nsLayoutUtils::TransformRect(focusFrame, rootFrame, focusRect);

  return focusRect;
}

nsRect
TouchCaret::ClampRectToScrollFrame(const nsRect& aRect)
{
  nsRect rect = aRect;
  nsIFrame* focusFrame = GetCaretFocusFrame();
  nsIFrame* rootFrame = GetRootFrame();

  // Clamp the touch caret position to the scrollframe boundary.
  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(focusFrame, nsGkAtoms::scrollFrame);

  while (closestScrollFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(closestScrollFrame);
    nsRect visualRect = sf->GetScrollPortRect();

    // Clamp the touch caret in the scroll port.
    nsLayoutUtils::TransformRect(closestScrollFrame, rootFrame, visualRect);
    rect = rect.Intersect(visualRect);

    // Get next ancestor scroll frame.
    closestScrollFrame =
      nsLayoutUtils::GetClosestFrameOfType(closestScrollFrame->GetParent(),
                                           nsGkAtoms::scrollFrame);
  }

  return rect;
}

/* static */void
TouchCaret::DisableTouchCaretCallback(nsITimer* aTimer, void* aTouchCaret)
{
  nsRefPtr<TouchCaret> self = static_cast<TouchCaret*>(aTouchCaret);
  NS_PRECONDITION(aTimer == self->mTouchCaretExpirationTimer,
                  "Unexpected timer");

  self->SetVisibility(false);
}

void
TouchCaret::LaunchExpirationTimer()
{
  if (TouchCaretExpirationTime() > 0) {
    if (!mTouchCaretExpirationTimer) {
      mTouchCaretExpirationTimer = do_CreateInstance("@mozilla.org/timer;1");
    }

    if (mTouchCaretExpirationTimer) {
      mTouchCaretExpirationTimer->Cancel();
      mTouchCaretExpirationTimer->InitWithFuncCallback(DisableTouchCaretCallback,
                                                       this,
                                                       TouchCaretExpirationTime(),
                                                       nsITimer::TYPE_ONE_SHOT);
    }
  }
}

void
TouchCaret::CancelExpirationTimer()
{
  if (mTouchCaretExpirationTimer) {
    mTouchCaretExpirationTimer->Cancel();
  }
}

void
TouchCaret::SetSelectionDragState(bool aState)
{
  nsIFrame* caretFocusFrame = GetCaretFocusFrame();
  if (!caretFocusFrame) {
    return;
  }

  nsRefPtr<nsFrameSelection> fs = caretFocusFrame->GetFrameSelection();
  fs->SetDragState(aState);
}

nsEventStatus
TouchCaret::HandleEvent(WidgetEvent* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!IsDisplayable()) {
    return nsEventStatus_eIgnore;
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (aEvent->mMessage) {
    case NS_TOUCH_START:
      status = HandleTouchDownEvent(aEvent->AsTouchEvent());
      break;
    case eMouseDown:
      status = HandleMouseDownEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_END:
      status = HandleTouchUpEvent(aEvent->AsTouchEvent());
      break;
    case eMouseUp:
      status = HandleMouseUpEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_MOVE:
      status = HandleTouchMoveEvent(aEvent->AsTouchEvent());
      break;
    case eMouseMove:
      status = HandleMouseMoveEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_CANCEL:
      mTouchesId.Clear();
      SetState(TOUCHCARET_NONE);
      LaunchExpirationTimer();
      break;
    case eKeyUp:
    case eKeyDown:
    case eKeyPress:
    case NS_WHEEL_WHEEL:
    case NS_WHEEL_START:
    case NS_WHEEL_STOP:
      // Disable touch caret while key/wheel event is received.
      TOUCHCARET_LOG("Receive key/wheel event %d", aEvent->mMessage);
      SetVisibility(false);
      break;
    case eMouseLongTap:
      if (mState == TOUCHCARET_TOUCHDRAG_ACTIVE) {
        // Disable long tap event from APZ while dragging the touch caret.
        status = nsEventStatus_eConsumeNoDefault;
      }
      break;
    default:
      break;
  }

  return status;
}

nsPoint
TouchCaret::GetEventPosition(WidgetTouchEvent* aEvent, int32_t aIdentifier)
{
  for (size_t i = 0; i < aEvent->touches.Length(); i++) {
    if (aEvent->touches[i]->mIdentifier == aIdentifier) {
      // Get event coordinate relative to canvas frame.
      nsIFrame* canvasFrame = GetCanvasFrame();
      LayoutDeviceIntPoint touchIntPoint = aEvent->touches[i]->mRefPoint;
      return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                          touchIntPoint,
                                                          canvasFrame);
    }
  }
  return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsPoint
TouchCaret::GetEventPosition(WidgetMouseEvent* aEvent)
{
  // Get event coordinate relative to canvas frame.
  nsIFrame* canvasFrame = GetCanvasFrame();
  LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->refPoint;
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                      mouseIntPoint,
                                                      canvasFrame);
}

nsEventStatus
TouchCaret::HandleMouseMoveEvent(WidgetMouseEvent* aEvent)
{
  TOUCHCARET_LOG("Got a mouse-move in state %d", mState);
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
      {
        nsPoint movePoint = GetEventPosition(aEvent);
        movePoint.y += mCaretCenterToDownPointOffsetY;
        nsRect contentBoundary = GetContentBoundary();
        movePoint = contentBoundary.ClampPoint(movePoint);

        MoveCaret(movePoint);
        mIsValidTap = false;
        status = nsEventStatus_eConsumeNoDefault;
      }
      break;

    case TOUCHCARET_TOUCHDRAG_ACTIVE:
    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      // Consume mouse event in touch sequence.
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  return status;
}

nsEventStatus
TouchCaret::HandleTouchMoveEvent(WidgetTouchEvent* aEvent)
{
  TOUCHCARET_LOG("Got a touch-move in state %d", mState);
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
      // Consume touch event in mouse sequence.
      status = nsEventStatus_eConsumeNoDefault;
      break;

    case TOUCHCARET_TOUCHDRAG_ACTIVE:
      {
        nsPoint movePoint = GetEventPosition(aEvent, mActiveTouchId);
        movePoint.y += mCaretCenterToDownPointOffsetY;
        nsRect contentBoundary = GetContentBoundary();
        movePoint = contentBoundary.ClampPoint(movePoint);

        MoveCaret(movePoint);
        mIsValidTap = false;
        status = nsEventStatus_eConsumeNoDefault;
      }
      break;

    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      // Consume NS_TOUCH_MOVE event in TOUCHCARET_TOUCHDRAG_INACTIVE state.
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  return status;
}

nsEventStatus
TouchCaret::HandleMouseUpEvent(WidgetMouseEvent* aEvent)
{
  TOUCHCARET_LOG("Got a mouse-up in state %d", mState);
  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        SetSelectionDragState(false);
        LaunchExpirationTimer();
        SetState(TOUCHCARET_NONE);
        status = nsEventStatus_eConsumeNoDefault;
      }
      break;

    case TOUCHCARET_TOUCHDRAG_ACTIVE:
    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      // Consume mouse event in touch sequence.
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  return status;
}

nsEventStatus
TouchCaret::HandleTouchUpEvent(WidgetTouchEvent* aEvent)
{
  TOUCHCARET_LOG("Got a touch-end in state %d", mState);
  // Remove touches from cache if the stroke is gone in TOUCHDRAG states.
  if (mState == TOUCHCARET_TOUCHDRAG_ACTIVE ||
      mState == TOUCHCARET_TOUCHDRAG_INACTIVE) {
    for (size_t i = 0; i < aEvent->touches.Length(); i++) {
      nsTArray<int32_t>::index_type index =
        mTouchesId.IndexOf(aEvent->touches[i]->mIdentifier);
      MOZ_ASSERT(index != nsTArray<int32_t>::NoIndex);
      mTouchesId.RemoveElementAt(index);
    }
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
      // Consume touch event in mouse sequence.
      status = nsEventStatus_eConsumeNoDefault;
      break;

    case TOUCHCARET_TOUCHDRAG_ACTIVE:
      if (mTouchesId.Length() == 0) {
        SetSelectionDragState(false);
        // No more finger on the screen.
        SetState(TOUCHCARET_NONE);
        LaunchExpirationTimer();
      } else {
        // Still has finger touching on the screen.
        if (aEvent->touches[0]->mIdentifier == mActiveTouchId) {
          // Remove finger from the touch caret.
          SetState(TOUCHCARET_TOUCHDRAG_INACTIVE);
          LaunchExpirationTimer();
        } else {
          // If the finger removed is not the finger on touch caret, remain in
          // TOUCHCARET_DRAG_ACTIVE state.
        }
      }
      status = nsEventStatus_eConsumeNoDefault;
      break;

    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      if (mTouchesId.Length() == 0) {
        // No more finger on the screen.
        SetState(TOUCHCARET_NONE);
      }
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  return status;
}

nsEventStatus
TouchCaret::HandleMouseDownEvent(WidgetMouseEvent* aEvent)
{
  TOUCHCARET_LOG("Got a mouse-down in state %d", mState);
  if (!GetVisibility()) {
    // If touch caret is invisible, bypass event.
    return nsEventStatus_eIgnore;
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      if (aEvent->button == WidgetMouseEvent::eLeftButton) {
        nsPoint point = GetEventPosition(aEvent);
        if (IsOnTouchCaret(point)) {
          SetSelectionDragState(true);
          // Cache distence of the event point to the center of touch caret.
          mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - point.y;
          // Enter TOUCHCARET_MOUSEDRAG_ACTIVE state and cancel the timer.
          SetState(TOUCHCARET_MOUSEDRAG_ACTIVE);
          CancelExpirationTimer();
          status = nsEventStatus_eConsumeNoDefault;
        } else {
          // Mousedown events that miss HitTest can be caused by soft-keyboard
          // auto-suggestions. If extended visibility, update the caret position.
          if (sTouchcaretExtendedvisibility) {
            UpdatePositionIfNeeded();
            break;
          }
          // Set touch caret invisible if HisTest fails. Bypass event.
          SetVisibility(false);
          status = nsEventStatus_eIgnore;
        }
      } else {
        // Set touch caret invisible if not left button down event.
        SetVisibility(false);
        status = nsEventStatus_eIgnore;
      }
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
      SetVisibility(false);
      SetState(TOUCHCARET_NONE);
      break;

    case TOUCHCARET_TOUCHDRAG_ACTIVE:
    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      // Consume mouse event in touch sequence.
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  return status;
}

nsEventStatus
TouchCaret::HandleTouchDownEvent(WidgetTouchEvent* aEvent)
{
  TOUCHCARET_LOG("Got a touch-start in state %d", mState);

  nsEventStatus status = nsEventStatus_eIgnore;

  switch (mState) {
    case TOUCHCARET_NONE:
      if (!GetVisibility()) {
        // If touch caret is invisible, bypass event.
        status = nsEventStatus_eIgnore;
      } else {
        // Loop over all the touches and see if any of them is on the touch
        // caret.
        for (size_t i = 0; i < aEvent->touches.Length(); ++i) {
          int32_t touchId = aEvent->touches[i]->Identifier();
          nsPoint point = GetEventPosition(aEvent, touchId);
          if (IsOnTouchCaret(point)) {
            SetSelectionDragState(true);
            // Touch start position is contained in touch caret.
            mActiveTouchId = touchId;
            // Cache distance of the event point to the center of touch caret.
            mCaretCenterToDownPointOffsetY = GetCaretYCenterPosition() - point.y;
            // Enter TOUCHCARET_TOUCHDRAG_ACTIVE state and cancel the timer.
            SetState(TOUCHCARET_TOUCHDRAG_ACTIVE);
            CancelExpirationTimer();
            status = nsEventStatus_eConsumeNoDefault;
            break;
          }
        }
        // No touch is on the touch caret. Set touch caret invisible, and bypass
        // the event.
        if (mActiveTouchId == -1) {
          // Check touch caret visibility style.
          if (sTouchcaretExtendedvisibility) {
            // Update position on events associated with scroll and pan-zoom.
            UpdatePositionIfNeeded();
          } else {
            SetVisibility(false);
            status = nsEventStatus_eIgnore;
          }
        }
      }
      break;

    case TOUCHCARET_MOUSEDRAG_ACTIVE:
    case TOUCHCARET_TOUCHDRAG_ACTIVE:
    case TOUCHCARET_TOUCHDRAG_INACTIVE:
      // Consume NS_TOUCH_START event.
      status = nsEventStatus_eConsumeNoDefault;
      break;
  }

  // Cache active touch IDs in TOUCHDRAG states.
  if (mState == TOUCHCARET_TOUCHDRAG_ACTIVE ||
      mState == TOUCHCARET_TOUCHDRAG_INACTIVE) {
    mTouchesId.Clear();
    for (size_t i = 0; i < aEvent->touches.Length(); i++) {
      mTouchesId.AppendElement(aEvent->touches[i]->mIdentifier);
    }
  }

  return status;
}

void
TouchCaret::DispatchTapEvent()
{
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (!presShell) {
    return;
  }

  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  if (!caret) {
    return;
  }

  dom::Selection* sel = static_cast<dom::Selection*>(caret->GetSelection());
  if (!sel) {
    return;
  }

  nsIDocument* doc = presShell->GetDocument();

  MOZ_ASSERT(doc);

  dom::SelectionStateChangedEventInit init;
  init.mBubbles = true;

  // XXX: Do we need to flush layout?
  presShell->FlushPendingNotifications(Flush_Layout);
  nsRect rect = nsContentUtils::GetSelectionBoundingRect(sel);
  nsRefPtr<dom::DOMRect>domRect = new dom::DOMRect(ToSupports(doc));

  domRect->SetLayoutRect(rect);
  init.mBoundingClientRect = domRect;
  init.mVisible = false;

  sel->Stringify(init.mSelectedText);

  dom::Sequence<dom::SelectionState> state;
  state.AppendElement(dom::SelectionState::Taponcaret, fallible);
  init.mStates = state;

  nsRefPtr<dom::SelectionStateChangedEvent> event =
    dom::SelectionStateChangedEvent::Constructor(doc, NS_LITERAL_STRING("mozselectionstatechanged"), init);

  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  bool ret;
  doc->DispatchEvent(event, &ret);
}

void
TouchCaret::SetState(TouchCaretState aState)
{
  TOUCHCARET_LOG("state changed from %d to %d", mState, aState);
  if (mState == TOUCHCARET_NONE) {
    MOZ_ASSERT(aState != TOUCHCARET_TOUCHDRAG_INACTIVE,
               "mState: NONE => TOUCHDRAG_INACTIVE isn't allowed!");
  }

  if (mState == TOUCHCARET_TOUCHDRAG_ACTIVE) {
    MOZ_ASSERT(aState != TOUCHCARET_MOUSEDRAG_ACTIVE,
               "mState: TOUCHDRAG_ACTIVE => MOUSEDRAG_ACTIVE isn't allowed!");
  }

  if (mState == TOUCHCARET_MOUSEDRAG_ACTIVE) {
    MOZ_ASSERT(aState == TOUCHCARET_MOUSEDRAG_ACTIVE ||
               aState == TOUCHCARET_NONE,
               "MOUSEDRAG_ACTIVE allowed next state: NONE!");
  }

  if (mState == TOUCHCARET_TOUCHDRAG_INACTIVE) {
    MOZ_ASSERT(aState == TOUCHCARET_TOUCHDRAG_INACTIVE ||
               aState == TOUCHCARET_NONE,
               "TOUCHDRAG_INACTIVE allowed next state: NONE!");
  }

  mState = aState;

  if (mState == TOUCHCARET_NONE) {
    mActiveTouchId = -1;
    mCaretCenterToDownPointOffsetY = 0;
    if (mIsValidTap) {
      DispatchTapEvent();
      mIsValidTap = false;
    }
  } else if (mState == TOUCHCARET_TOUCHDRAG_ACTIVE ||
             mState == TOUCHCARET_MOUSEDRAG_ACTIVE) {
    mIsValidTap = true;
  }
}
