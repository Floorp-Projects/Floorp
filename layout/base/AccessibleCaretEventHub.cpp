/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleCaretEventHub.h"

#include "AccessibleCaretLogger.h"
#include "AccessibleCaretManager.h"
#include "Layers.h"
#include "gfxPrefs.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/Selection.h"
#include "nsCanvasFrame.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameSelection.h"
#include "nsITimer.h"
#include "nsPresContext.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

#undef AC_LOG
#define AC_LOG(message, ...) \
  AC_LOG_BASE("AccessibleCaretEventHub (%p): " message, this, ##__VA_ARGS__);

#undef AC_LOGV
#define AC_LOGV(message, ...) \
  AC_LOGV_BASE("AccessibleCaretEventHub (%p): " message, this, ##__VA_ARGS__);

NS_IMPL_ISUPPORTS(AccessibleCaretEventHub, nsIReflowObserver, nsIScrollObserver,
                  nsISupportsWeakReference);

// -----------------------------------------------------------------------------
// NoActionState
//
class AccessibleCaretEventHub::NoActionState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "NoActionState"; }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnPress(AccessibleCaretEventHub* aContext,
                        const nsPoint& aPoint, int32_t aTouchId,
                        EventClassID aEventClass) override {
    nsEventStatus rv = nsEventStatus_eIgnore;

    if (NS_SUCCEEDED(aContext->mManager->PressCaret(aPoint, aEventClass))) {
      aContext->SetState(AccessibleCaretEventHub::PressCaretState());
      rv = nsEventStatus_eConsumeNoDefault;
    } else {
      aContext->SetState(AccessibleCaretEventHub::PressNoCaretState());
    }

    aContext->mPressPoint = aPoint;
    aContext->mActiveTouchId = aTouchId;

    return rv;
  }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollStart(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollStart();
    aContext->SetState(AccessibleCaretEventHub::ScrollState());
  }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollPositionChanged(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollPositionChanged();
  }

  MOZ_CAN_RUN_SCRIPT
  void OnSelectionChanged(AccessibleCaretEventHub* aContext, Document* aDoc,
                          dom::Selection* aSel, int16_t aReason) override {
    aContext->mManager->OnSelectionChanged(aDoc, aSel, aReason);
  }

  MOZ_CAN_RUN_SCRIPT
  void OnBlur(AccessibleCaretEventHub* aContext,
              bool aIsLeavingDocument) override {
    aContext->mManager->OnBlur();
  }

  MOZ_CAN_RUN_SCRIPT
  void OnReflow(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnReflow();
  }

  void Enter(AccessibleCaretEventHub* aContext) override {
    aContext->mPressPoint = nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
    aContext->mActiveTouchId = kInvalidTouchId;
  }
};

// -----------------------------------------------------------------------------
// PressCaretState: Always consume the event since we've pressed on the caret.
//
class AccessibleCaretEventHub::PressCaretState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "PressCaretState"; }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                       const nsPoint& aPoint) override {
    if (aContext->MoveDistanceIsLarge(aPoint)) {
      if (NS_SUCCEEDED(aContext->mManager->DragCaret(aPoint))) {
        aContext->SetState(AccessibleCaretEventHub::DragCaretState());
      }
    }

    return nsEventStatus_eConsumeNoDefault;
  }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->ReleaseCaret();
    aContext->mManager->TapCaret(aContext->mPressPoint);
    aContext->SetState(AccessibleCaretEventHub::NoActionState());

    return nsEventStatus_eConsumeNoDefault;
  }

  nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                          const nsPoint& aPoint) override {
    return nsEventStatus_eConsumeNoDefault;
  }
};

// -----------------------------------------------------------------------------
// DragCaretState: Always consume the event since we've pressed on the caret.
//
class AccessibleCaretEventHub::DragCaretState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "DragCaretState"; }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                       const nsPoint& aPoint) override {
    aContext->mManager->DragCaret(aPoint);

    return nsEventStatus_eConsumeNoDefault;
  }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->ReleaseCaret();
    aContext->SetState(AccessibleCaretEventHub::NoActionState());

    return nsEventStatus_eConsumeNoDefault;
  }
};

// -----------------------------------------------------------------------------
// PressNoCaretState
//
class AccessibleCaretEventHub::PressNoCaretState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "PressNoCaretState"; }

  nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                       const nsPoint& aPoint) override {
    if (aContext->MoveDistanceIsLarge(aPoint)) {
      aContext->SetState(AccessibleCaretEventHub::NoActionState());
    }

    return nsEventStatus_eIgnore;
  }

  nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override {
    aContext->SetState(AccessibleCaretEventHub::NoActionState());

    return nsEventStatus_eIgnore;
  }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                          const nsPoint& aPoint) override {
    aContext->SetState(AccessibleCaretEventHub::LongTapState());

    return aContext->GetState()->OnLongTap(aContext, aPoint);
  }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollStart(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollStart();
    aContext->SetState(AccessibleCaretEventHub::ScrollState());
  }

  MOZ_CAN_RUN_SCRIPT
  void OnBlur(AccessibleCaretEventHub* aContext,
              bool aIsLeavingDocument) override {
    aContext->mManager->OnBlur();
    if (aIsLeavingDocument) {
      aContext->SetState(AccessibleCaretEventHub::NoActionState());
    }
  }

  MOZ_CAN_RUN_SCRIPT
  void OnSelectionChanged(AccessibleCaretEventHub* aContext, Document* aDoc,
                          dom::Selection* aSel, int16_t aReason) override {
    aContext->mManager->OnSelectionChanged(aDoc, aSel, aReason);
  }

  MOZ_CAN_RUN_SCRIPT
  void OnReflow(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnReflow();
  }

  void Enter(AccessibleCaretEventHub* aContext) override {
    aContext->LaunchLongTapInjector();
  }

  void Leave(AccessibleCaretEventHub* aContext) override {
    aContext->CancelLongTapInjector();
  }
};

// -----------------------------------------------------------------------------
// ScrollState
//
class AccessibleCaretEventHub::ScrollState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "ScrollState"; }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollEnd(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollEnd();
    aContext->SetState(AccessibleCaretEventHub::NoActionState());
  }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollPositionChanged(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollPositionChanged();
  }

  MOZ_CAN_RUN_SCRIPT
  void OnBlur(AccessibleCaretEventHub* aContext,
              bool aIsLeavingDocument) override {
    aContext->mManager->OnBlur();
    if (aIsLeavingDocument) {
      aContext->SetState(AccessibleCaretEventHub::NoActionState());
    }
  }
};

// -----------------------------------------------------------------------------
// LongTapState
//
class AccessibleCaretEventHub::LongTapState
    : public AccessibleCaretEventHub::State {
 public:
  const char* Name() const override { return "LongTapState"; }

  MOZ_CAN_RUN_SCRIPT
  nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                          const nsPoint& aPoint) override {
    // In general text selection is lower-priority than the context menu. If
    // we consume this long-press event, then it prevents the context menu from
    // showing up on desktop Firefox (because that happens on long-tap-up, if
    // the long-tap was not cancelled). So we return eIgnore instead.
    aContext->mManager->SelectWordOrShortcut(aPoint);
    return nsEventStatus_eIgnore;
  }

  nsEventStatus OnRelease(AccessibleCaretEventHub* aContext) override {
    aContext->SetState(AccessibleCaretEventHub::NoActionState());

    // Do not consume the release since the press is not consumed in
    // PressNoCaretState either.
    return nsEventStatus_eIgnore;
  }

  MOZ_CAN_RUN_SCRIPT
  void OnScrollStart(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnScrollStart();
    aContext->SetState(AccessibleCaretEventHub::ScrollState());
  }

  MOZ_CAN_RUN_SCRIPT
  void OnReflow(AccessibleCaretEventHub* aContext) override {
    aContext->mManager->OnReflow();
  }
};

// -----------------------------------------------------------------------------
// Implementation of AccessibleCaretEventHub methods
//
AccessibleCaretEventHub::State* AccessibleCaretEventHub::GetState() const {
  return mState;
}

void AccessibleCaretEventHub::SetState(State* aState) {
  MOZ_ASSERT(aState);

  AC_LOG("%s -> %s", mState->Name(), aState->Name());

  mState->Leave(this);
  mState = aState;
  mState->Enter(this);
}

MOZ_IMPL_STATE_CLASS_GETTER(NoActionState)
MOZ_IMPL_STATE_CLASS_GETTER(PressCaretState)
MOZ_IMPL_STATE_CLASS_GETTER(DragCaretState)
MOZ_IMPL_STATE_CLASS_GETTER(PressNoCaretState)
MOZ_IMPL_STATE_CLASS_GETTER(ScrollState)
MOZ_IMPL_STATE_CLASS_GETTER(LongTapState)

AccessibleCaretEventHub::AccessibleCaretEventHub(PresShell* aPresShell)
    : mPresShell(aPresShell) {}

void AccessibleCaretEventHub::Init() {
  if (mInitialized && mManager) {
    mManager->OnFrameReconstruction();
  }

  if (mInitialized || !mPresShell || !mPresShell->GetCanvasFrame()) {
    return;
  }

  // Without nsAutoScriptBlocker, the script might be run after constructing
  // mFirstCaret in AccessibleCaretManager's constructor, which might destructs
  // the whole frame tree. Therefore we'll fail to construct mSecondCaret
  // because we cannot get root frame or canvas frame from mPresShell to inject
  // anonymous content. To avoid that, we protect Init() by nsAutoScriptBlocker.
  // To reproduce, run "./mach crashtest layout/base/crashtests/897852.html"
  // without the following scriptBlocker.
  nsAutoScriptBlocker scriptBlocker;

  nsPresContext* presContext = mPresShell->GetPresContext();
  MOZ_ASSERT(presContext, "PresContext should be given in PresShell::Init()");

  nsIDocShell* docShell = presContext->GetDocShell();
  if (!docShell) {
    return;
  }

  nsCOMPtr<nsIDocShell> curDocShell = docShell;
  do {
    curDocShell->AddWeakReflowObserver(this);
    curDocShell->AddWeakScrollObserver(this);

    nsCOMPtr<nsIDocShellTreeItem> tmp;
    curDocShell->GetSameTypeParent(getter_AddRefs(tmp));
    curDocShell = do_QueryInterface(tmp);
  } while (curDocShell);

  mDocShell = static_cast<nsDocShell*>(docShell);

  if (StaticPrefs::layout_accessiblecaret_use_long_tap_injector()) {
    mLongTapInjectorTimer = NS_NewTimer();
  }

  mManager = MakeUnique<AccessibleCaretManager>(mPresShell);

  mInitialized = true;
}

void AccessibleCaretEventHub::Terminate() {
  if (!mInitialized) {
    return;
  }

  nsCOMPtr<nsIDocShell> curDocShell = mDocShell.get();
  while (curDocShell) {
    curDocShell->RemoveWeakReflowObserver(this);
    curDocShell->RemoveWeakScrollObserver(this);

    nsCOMPtr<nsIDocShellTreeItem> tmp;
    curDocShell->GetSameTypeParent(getter_AddRefs(tmp));
    curDocShell = do_QueryInterface(tmp);
  }

  if (mLongTapInjectorTimer) {
    mLongTapInjectorTimer->Cancel();
  }

  mManager->Terminate();
  mPresShell = nullptr;
  mInitialized = false;
}

nsEventStatus AccessibleCaretEventHub::HandleEvent(WidgetEvent* aEvent) {
  nsEventStatus status = nsEventStatus_eIgnore;

  if (!mInitialized) {
    return status;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  switch (aEvent->mClass) {
    case eMouseEventClass:
      status = HandleMouseEvent(aEvent->AsMouseEvent());
      break;

    case eTouchEventClass:
      status = HandleTouchEvent(aEvent->AsTouchEvent());
      break;

    case eKeyboardEventClass:
      status = HandleKeyboardEvent(aEvent->AsKeyboardEvent());
      break;

    default:
      break;
  }

  return status;
}

nsEventStatus AccessibleCaretEventHub::HandleMouseEvent(
    WidgetMouseEvent* aEvent) {
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (aEvent->mButton != MouseButton::eLeft) {
    return rv;
  }

  int32_t id =
      (mActiveTouchId == kInvalidTouchId ? kDefaultTouchId : mActiveTouchId);
  nsPoint point = GetMouseEventPosition(aEvent);

  if (aEvent->mMessage == eMouseDown || aEvent->mMessage == eMouseUp ||
      aEvent->mMessage == eMouseClick ||
      aEvent->mMessage == eMouseDoubleClick ||
      aEvent->mMessage == eMouseLongTap) {
    // Don't reset the source on mouse movement since that can
    // happen anytime, even randomly during a touch sequence.
    mManager->SetLastInputSource(aEvent->mInputSource);
  }

  switch (aEvent->mMessage) {
    case eMouseDown:
      AC_LOGV("Before eMouseDown, state: %s", mState->Name());
      rv = mState->OnPress(this, point, id, eMouseEventClass);
      AC_LOGV("After eMouseDown, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eMouseMove:
      AC_LOGV("Before eMouseMove, state: %s", mState->Name());
      rv = mState->OnMove(this, point);
      AC_LOGV("After eMouseMove, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eMouseUp:
      AC_LOGV("Before eMouseUp, state: %s", mState->Name());
      rv = mState->OnRelease(this);
      AC_LOGV("After eMouseUp, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eMouseLongTap:
      AC_LOGV("Before eMouseLongTap, state: %s", mState->Name());
      rv = mState->OnLongTap(this, point);
      AC_LOGV("After eMouseLongTap, state: %s, consume: %d", mState->Name(),
              rv);
      break;

    default:
      break;
  }

  return rv;
}

nsEventStatus AccessibleCaretEventHub::HandleTouchEvent(
    WidgetTouchEvent* aEvent) {
  if (aEvent->mTouches.IsEmpty()) {
    AC_LOG("%s: Receive a touch event without any touch data!", __FUNCTION__);
    return nsEventStatus_eIgnore;
  }

  nsEventStatus rv = nsEventStatus_eIgnore;

  int32_t id =
      (mActiveTouchId == kInvalidTouchId ? aEvent->mTouches[0]->Identifier()
                                         : mActiveTouchId);
  nsPoint point = GetTouchEventPosition(aEvent, id);

  mManager->SetLastInputSource(MouseEvent_Binding::MOZ_SOURCE_TOUCH);

  switch (aEvent->mMessage) {
    case eTouchStart:
      AC_LOGV("Before eTouchStart, state: %s", mState->Name());
      rv = mState->OnPress(this, point, id, eTouchEventClass);
      AC_LOGV("After eTouchStart, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eTouchMove:
      AC_LOGV("Before eTouchMove, state: %s", mState->Name());
      rv = mState->OnMove(this, point);
      AC_LOGV("After eTouchMove, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eTouchEnd:
      AC_LOGV("Before eTouchEnd, state: %s", mState->Name());
      rv = mState->OnRelease(this);
      AC_LOGV("After eTouchEnd, state: %s, consume: %d", mState->Name(), rv);
      break;

    case eTouchCancel:
      AC_LOGV("Got eTouchCancel, state: %s", mState->Name());
      // Do nothing since we don't really care eTouchCancel anyway.
      break;

    default:
      break;
  }

  return rv;
}

nsEventStatus AccessibleCaretEventHub::HandleKeyboardEvent(
    WidgetKeyboardEvent* aEvent) {
  mManager->SetLastInputSource(MouseEvent_Binding::MOZ_SOURCE_KEYBOARD);

  switch (aEvent->mMessage) {
    case eKeyUp:
      AC_LOGV("eKeyUp, state: %s", mState->Name());
      mManager->OnKeyboardEvent();
      break;

    case eKeyDown:
      AC_LOGV("eKeyDown, state: %s", mState->Name());
      mManager->OnKeyboardEvent();
      break;

    case eKeyPress:
      AC_LOGV("eKeyPress, state: %s", mState->Name());
      mManager->OnKeyboardEvent();
      break;

    default:
      break;
  }

  return nsEventStatus_eIgnore;
}

bool AccessibleCaretEventHub::MoveDistanceIsLarge(const nsPoint& aPoint) const {
  nsPoint delta = aPoint - mPressPoint;
  return NS_hypot(delta.x, delta.y) >
         AppUnitsPerCSSPixel() * kMoveStartToleranceInPixel;
}

void AccessibleCaretEventHub::LaunchLongTapInjector() {
  if (!mLongTapInjectorTimer) {
    return;
  }

  int32_t longTapDelay = gfxPrefs::UiClickHoldContextMenusDelay();
  mLongTapInjectorTimer->InitWithNamedFuncCallback(
      FireLongTap, this, longTapDelay, nsITimer::TYPE_ONE_SHOT,
      "AccessibleCaretEventHub::LaunchLongTapInjector");
}

void AccessibleCaretEventHub::CancelLongTapInjector() {
  if (!mLongTapInjectorTimer) {
    return;
  }

  mLongTapInjectorTimer->Cancel();
}

/* static */
void AccessibleCaretEventHub::FireLongTap(nsITimer* aTimer,
                                          void* aAccessibleCaretEventHub) {
  RefPtr<AccessibleCaretEventHub> self =
      static_cast<AccessibleCaretEventHub*>(aAccessibleCaretEventHub);
  self->mState->OnLongTap(self, self->mPressPoint);
}

NS_IMETHODIMP
AccessibleCaretEventHub::Reflow(DOMHighResTimeStamp aStart,
                                DOMHighResTimeStamp aEnd) {
  if (!mInitialized) {
    return NS_OK;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  if (mIsInReflowCallback) {
    return NS_OK;
  }

  AutoRestore<bool> autoRestoreIsInReflowCallback(mIsInReflowCallback);
  mIsInReflowCallback = true;

  AC_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnReflow(this);
  return NS_OK;
}

NS_IMETHODIMP
AccessibleCaretEventHub::ReflowInterruptible(DOMHighResTimeStamp aStart,
                                             DOMHighResTimeStamp aEnd) {
  // Defer the error checking to Reflow().
  return Reflow(aStart, aEnd);
}

void AccessibleCaretEventHub::AsyncPanZoomStarted() {
  if (!mInitialized) {
    return;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  AC_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollStart(this);
}

void AccessibleCaretEventHub::AsyncPanZoomStopped() {
  if (!mInitialized) {
    return;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  AC_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollEnd(this);
}

void AccessibleCaretEventHub::ScrollPositionChanged() {
  if (!mInitialized) {
    return;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  AC_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnScrollPositionChanged(this);
}

void AccessibleCaretEventHub::OnSelectionChange(Document* aDoc,
                                                dom::Selection* aSel,
                                                int16_t aReason) {
  if (!mInitialized) {
    return;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  AC_LOG("%s, state: %s, reason: %d", __FUNCTION__, mState->Name(), aReason);

  // XXX Here we may be in a hot path.  So, if we could avoid this virtual call,
  //     we should do so.
  mState->OnSelectionChanged(this, aDoc, aSel, aReason);
}

void AccessibleCaretEventHub::NotifyBlur(bool aIsLeavingDocument) {
  if (!mInitialized) {
    return;
  }

  MOZ_ASSERT(mRefCnt.get() > 1, "Expect caller holds us as well!");

  AC_LOG("%s, state: %s", __FUNCTION__, mState->Name());
  mState->OnBlur(this, aIsLeavingDocument);
}

nsPoint AccessibleCaretEventHub::GetTouchEventPosition(
    WidgetTouchEvent* aEvent, int32_t aIdentifier) const {
  for (dom::Touch* touch : aEvent->mTouches) {
    if (touch->Identifier() == aIdentifier) {
      LayoutDeviceIntPoint touchIntPoint = touch->mRefPoint;

      // Get event coordinate relative to root frame.
      nsIFrame* rootFrame = mPresShell->GetRootFrame();
      return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, touchIntPoint,
                                                          rootFrame);
    }
  }
  return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsPoint AccessibleCaretEventHub::GetMouseEventPosition(
    WidgetMouseEvent* aEvent) const {
  LayoutDeviceIntPoint mouseIntPoint = aEvent->AsGUIEvent()->mRefPoint;

  // Get event coordinate relative to root frame.
  nsIFrame* rootFrame = mPresShell->GetRootFrame();
  return nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, mouseIntPoint,
                                                      rootFrame);
}

}  // namespace mozilla
