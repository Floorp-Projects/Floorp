/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TouchCaret.h"

#include <algorithm>

#include "nsCOMPtr.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsCanvasFrame.h"
#include "nsRenderingContext.h"
#include "nsPresContext.h"
#include "nsBlockFrame.h"
#include "nsISelectionController.h"
#include "mozilla/Preferences.h"
#include "mozilla/BasicEvents.h"
#include "nsIDOMWindow.h"
#include "nsQueryContentEventResult.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsView.h"
#include "nsDOMTokenList.h"
#include "nsCaret.h"
#include "mozilla/dom/CustomEvent.h"

using namespace mozilla;

// To enable all the TOUCHCARET_LOG print statements, change the 0 to 1 in the
// following #define.
#define ENABLE_TOUCHCARET_LOG 0

#if ENABLE_TOUCHCARET_LOG
  #define TOUCHCARET_LOG(message, ...) \
    printf_stderr("TouchCaret (%p): %s:%d : " message "\n", this, __func__, __LINE__, ##__VA_ARGS__);
#else
  #define TOUCHCARET_LOG(message, ...)
#endif

// Click on the boundary of input/textarea will place the caret at the
// front/end of the content. To advoid this, we need to deflate the content
// boundary by 61 app units (1 pixel + 1 app unit).
static const int32_t kBoundaryAppUnits = 61;

NS_IMPL_ISUPPORTS(TouchCaret, nsISelectionListener)

/*static*/ int32_t TouchCaret::sTouchCaretInflateSize = 0;
/*static*/ int32_t TouchCaret::sTouchCaretExpirationTime = 0;

TouchCaret::TouchCaret(nsIPresShell* aPresShell)
  : mState(TOUCHCARET_NONE),
    mActiveTouchId(-1),
    mCaretCenterToDownPointOffsetY(0),
    mVisible(false),
    mIsValidTap(false)
{
  TOUCHCARET_LOG("Constructor, PresShell=%p", aPresShell);
  MOZ_ASSERT(NS_IsMainThread());

  static bool addedTouchCaretPref = false;
  if (!addedTouchCaretPref) {
    Preferences::AddIntVarCache(&sTouchCaretInflateSize,
                                "touchcaret.inflatesize.threshold");
    Preferences::AddIntVarCache(&sTouchCaretExpirationTime,
                                "touchcaret.expiration.time");
    addedTouchCaretPref = true;
  }

  // The presshell owns us, so no addref.
  mPresShell = do_GetWeakReference(aPresShell);
  MOZ_ASSERT(mPresShell, "Hey, pres shell should support weak refs");
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

  // We must call SetMayHaveTouchCaret() in order to get APZC to wait until the
  // event has been round-tripped and check whether it has been handled,
  // otherwise B2G will end up panning the document when the user tries to drag
  // touch caret.
  presShell->SetMayHaveTouchCaret(mVisible);
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
TouchCaret::SetTouchFramePos(const nsPoint& aOrigin)
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
  int32_t x = presContext->AppUnitsToIntCSSPixels(aOrigin.x);
  int32_t y = presContext->AppUnitsToIntCSSPixels(aOrigin.y);

  nsAutoString styleStr;
  styleStr.AppendLiteral("left: ");
  styleStr.AppendInt(x);
  styleStr.AppendLiteral("px; top: ");
  styleStr.AppendInt(y);
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
  TOUCHCARET_LOG("Reason=%d", aReason);

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
  if (aReason & nsISelectionListener::KEYPRESS_REASON) {
    TOUCHCARET_LOG("KEYPRESS_REASON");
    SetVisibility(false);
  } else {
    SyncVisibilityWithCaret();
  }

  return NS_OK;
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
  if (focusRect.IsEmpty()) {
    TOUCHCARET_LOG("Focus rect is empty!");
    return false;
  }

  if (!IsCaretShowingInScrollFrame()) {
    TOUCHCARET_LOG("Caret does not show in the scrollable frame!");
    return false;
  }

  return true;
}

void
TouchCaret::UpdatePosition()
{
  MOZ_ASSERT(mVisible);

  nsPoint pos = GetTouchCaretPosition();
  pos = ClampPositionToScrollFrame(pos);
  SetTouchFramePos(pos);
}

nsPoint
TouchCaret::GetTouchCaretPosition()
{
  nsRect focusRect;
  nsIFrame* focusFrame = GetCaretFocusFrame(&focusRect);
  nsIFrame* canvasFrame = GetCanvasFrame();

  // Position of the touch caret relative to focusFrame.
  nsPoint pos = nsPoint(focusRect.x + (focusRect.width / 2),
                        focusRect.y + focusRect.height);

  // Transform the position to make it relative to canvas frame.
  nsLayoutUtils::TransformPoint(focusFrame, canvasFrame, pos);

  return pos;
}

bool
TouchCaret::IsCaretShowingInScrollFrame()
{
  nsRect caretRect;
  nsIFrame* caretFrame = GetCaretFocusFrame(&caretRect);

  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(caretFrame, nsGkAtoms::scrollFrame);

  while (closestScrollFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(closestScrollFrame);
    nsRect scrollPortRect = sf->GetScrollPortRect();

    nsRect caretRectRelativeToScrollFrame = caretRect;
    nsLayoutUtils::TransformRect(caretFrame, closestScrollFrame,
                                 caretRectRelativeToScrollFrame);

    // Check whether nsCaret appears in the scroll frame or not.
    if (!scrollPortRect.Intersects(caretRectRelativeToScrollFrame)) {
      return false;
    }

    // Get next ancestor scroll frame.
    closestScrollFrame =
      nsLayoutUtils::GetClosestFrameOfType(closestScrollFrame->GetParent(),
                                           nsGkAtoms::scrollFrame);
  }

  return true;
}

nsPoint
TouchCaret::ClampPositionToScrollFrame(const nsPoint& aPosition)
{
  nsPoint pos = aPosition;
  nsIFrame* focusFrame = GetCaretFocusFrame();
  nsIFrame* canvasFrame = GetCanvasFrame();

  // Clamp the touch caret position to the scrollframe boundary.
  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(focusFrame, nsGkAtoms::scrollFrame);

  while (closestScrollFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(closestScrollFrame);
    nsRect visualRect = sf->GetScrollPortRect();

    // Clamp the touch caret in the scroll port.
    nsLayoutUtils::TransformRect(closestScrollFrame, canvasFrame, visualRect);
    pos = visualRect.ClampPoint(pos);

    // Get next ancestor scroll frame.
    closestScrollFrame =
      nsLayoutUtils::GetClosestFrameOfType(closestScrollFrame->GetParent(),
                                           nsGkAtoms::scrollFrame);
  }

  return pos;
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

  switch (aEvent->message) {
    case NS_TOUCH_START:
      status = HandleTouchDownEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_BUTTON_DOWN:
      status = HandleMouseDownEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_END:
      status = HandleTouchUpEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_BUTTON_UP:
      status = HandleMouseUpEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_MOVE:
      status = HandleTouchMoveEvent(aEvent->AsTouchEvent());
      break;
    case NS_MOUSE_MOVE:
      status = HandleMouseMoveEvent(aEvent->AsMouseEvent());
      break;
    case NS_TOUCH_CANCEL:
      mTouchesId.Clear();
      SetState(TOUCHCARET_NONE);
      LaunchExpirationTimer();
      break;
    case NS_KEY_UP:
    case NS_KEY_DOWN:
    case NS_KEY_PRESS:
    case NS_WHEEL_WHEEL:
    case NS_WHEEL_START:
    case NS_WHEEL_STOP:
      // Disable touch caret while key/wheel event is received.
      TOUCHCARET_LOG("Receive key/wheel event %d", aEvent->message);
      SetVisibility(false);
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
      nsIntPoint touchIntPoint = aEvent->touches[i]->mRefPoint;
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
  nsIntPoint mouseIntPoint =
    LayoutDeviceIntPoint::ToUntyped(aEvent->AsGUIEvent()->refPoint);
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
          SetVisibility(false);
          status = nsEventStatus_eIgnore;
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

  nsCOMPtr<nsIDocument> doc = presShell->GetDocument();
  if (!doc) {
    return;
  }

  ErrorResult res;
  nsRefPtr<dom::Event> domEvent =
    doc->CreateEvent(NS_LITERAL_STRING("CustomEvent"), res);
  if (res.Failed()) {
    return;
  }

  dom::CustomEvent* customEvent = static_cast<dom::CustomEvent*>(domEvent.get());
  customEvent->InitCustomEvent(NS_LITERAL_STRING("touchcarettap"),
                               true, false, nullptr);
  customEvent->SetTrusted(true);
  customEvent->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  bool ret;
  doc->DispatchEvent(domEvent, &ret);
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
