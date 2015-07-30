/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccessibleCaretEventHub_h
#define AccessibleCaretEventHub_h

#include "mozilla/EventForwards.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIReflowObserver.h"
#include "nsIScrollObserver.h"
#include "nsISelectionListener.h"
#include "nsPoint.h"
#include "mozilla/nsRefPtr.h"
#include "nsWeakReference.h"

class nsDocShell;
class nsIPresShell;
class nsITimer;

namespace mozilla {
class AccessibleCaretManager;
class WidgetKeyboardEvent;
class WidgetMouseEvent;
class WidgetTouchEvent;
class WidgetWheelEvent;

// -----------------------------------------------------------------------------
// Each PresShell holds a shared pointer to an AccessibleCaretEventHub; each
// AccessibleCaretEventHub holds a unique pointer to an AccessibleCaretManager.
// Thus, there's one AccessibleCaretManager per PresShell.
//
// AccessibleCaretEventHub implements a state pattern. It receives events from
// PresShell and callbacks by observers and listeners, and then relays them to
// the current concrete state which calls necessary event-handling methods in
// AccessibleCaretManager.
//
// We separate AccessibleCaretEventHub from AccessibleCaretManager to make the
// state transitions in AccessibleCaretEventHub testable. We put (nearly) all
// the operations involving PresShell, Selection, and AccessibleCaret
// manipulation in AccessibleCaretManager so that we can mock methods in
// AccessibleCaretManager in gtest. We test the correctness of the state
// transitions by giving events, callbacks, and the return values by mocked
// methods of AccessibleCaretEventHub. See TestAccessibleCaretEventHub.cpp.
//
// Besides dealing with real events, AccessibleCaretEventHub also synthesizes
// fake events such as scroll-end or long-tap providing APZ is not in use.
//
// State transition diagram:
// http://hg.mozilla.org/mozilla-central/file/default/layout/base/doc/AccessibleCaretEventHubStates.png
// Source code of the diagram:
// http://hg.mozilla.org/mozilla-central/file/default/layout/base/doc/AccessibleCaretEventHubStates.dot
//
class AccessibleCaretEventHub : public nsIReflowObserver,
                                public nsIScrollObserver,
                                public nsISelectionListener,
                                public nsSupportsWeakReference
{
public:
  explicit AccessibleCaretEventHub();
  virtual void Init(nsIPresShell* aPresShell);
  virtual void Terminate();

  nsEventStatus HandleEvent(WidgetEvent* aEvent);

  // Call this function to notify the blur event happened.
  void NotifyBlur(bool aIsLeavingDocument);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFLOWOBSERVER
  NS_DECL_NSISELECTIONLISTENER

  // Override nsIScrollObserver methods.
  virtual void ScrollPositionChanged() override;
  virtual void AsyncPanZoomStarted() override;
  virtual void AsyncPanZoomStopped() override;

  // Base state
  class State;
  State* GetState() const;

protected:
  virtual ~AccessibleCaretEventHub();

#define MOZ_DECL_STATE_CLASS_GETTER(aClassName)                                \
  class aClassName;                                                            \
  static State* aClassName();

#define MOZ_IMPL_STATE_CLASS_GETTER(aClassName)                                \
  AccessibleCaretEventHub::State* AccessibleCaretEventHub::aClassName()        \
  {                                                                            \
    return AccessibleCaretEventHub::aClassName::Singleton();                   \
  }

  // Concrete state getters
  MOZ_DECL_STATE_CLASS_GETTER(NoActionState)
  MOZ_DECL_STATE_CLASS_GETTER(PressCaretState)
  MOZ_DECL_STATE_CLASS_GETTER(DragCaretState)
  MOZ_DECL_STATE_CLASS_GETTER(PressNoCaretState)
  MOZ_DECL_STATE_CLASS_GETTER(ScrollState)
  MOZ_DECL_STATE_CLASS_GETTER(PostScrollState)
  MOZ_DECL_STATE_CLASS_GETTER(LongTapState)

  void SetState(State* aState);

  nsEventStatus HandleMouseEvent(WidgetMouseEvent* aEvent);
  nsEventStatus HandleWheelEvent(WidgetWheelEvent* aEvent);
  nsEventStatus HandleTouchEvent(WidgetTouchEvent* aEvent);
  nsEventStatus HandleKeyboardEvent(WidgetKeyboardEvent* aEvent);

  virtual nsPoint GetTouchEventPosition(WidgetTouchEvent* aEvent,
                                        int32_t aIdentifier) const;
  virtual nsPoint GetMouseEventPosition(WidgetMouseEvent* aEvent) const;

  bool MoveDistanceIsLarge(const nsPoint& aPoint) const;

  void LaunchLongTapInjector();
  void CancelLongTapInjector();
  static void FireLongTap(nsITimer* aTimer, void* aAccessibleCaretEventHub);

  void LaunchScrollEndInjector();
  void CancelScrollEndInjector();
  static void FireScrollEnd(nsITimer* aTimer, void* aAccessibleCaretEventHub);

  // Member variables
  bool mInitialized = false;

  // True if async-pan-zoom should be used.
  bool mUseAsyncPanZoom = false;

  State* mState = NoActionState();

  // Will be set to nullptr in Terminate().
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell = nullptr;

  UniquePtr<AccessibleCaretManager> mManager;

  WeakPtr<nsDocShell> mDocShell;

  // Use this timer for injecting a long tap event when APZ is disabled. If APZ
  // is enabled, it will send long tap event to us.
  nsCOMPtr<nsITimer> mLongTapInjectorTimer;

  // Use this timer for injecting a simulated scroll end.
  nsCOMPtr<nsITimer> mScrollEndInjectorTimer;

  // Last mouse button down event or touch start event point.
  nsPoint mPressPoint{ NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE };

  // For filter multitouch event
  int32_t mActiveTouchId = kInvalidTouchId;

  static const int32_t kScrollEndTimerDelay = 300;
  static const int32_t kMoveStartToleranceInPixel = 5;
  static const int32_t kInvalidTouchId = -1;
  static const int32_t kDefaultTouchId = 0; // For mouse event
};

// -----------------------------------------------------------------------------
// The base class for concrete states. A concrete state should inherit from this
// class, and override the methods to handle the events or callbacks. A concrete
// state is also responsible for transforming itself to the next concrete state.
//
class AccessibleCaretEventHub::State
{
public:
#define NS_IMPL_STATE_UTILITIES(aClassName)                                    \
  virtual const char* Name() const override { return #aClassName; }            \
  static aClassName* Singleton()                                               \
  {                                                                            \
    static aClassName singleton;                                               \
    return &singleton;                                                         \
  }

  virtual const char* Name() const { return ""; }

  virtual nsEventStatus OnPress(AccessibleCaretEventHub* aContext,
                                const nsPoint& aPoint, int32_t aTouchId)
  {
    return nsEventStatus_eIgnore;
  }

  virtual nsEventStatus OnMove(AccessibleCaretEventHub* aContext,
                               const nsPoint& aPoint)
  {
    return nsEventStatus_eIgnore;
  }

  virtual nsEventStatus OnRelease(AccessibleCaretEventHub* aContext)
  {
    return nsEventStatus_eIgnore;
  }

  virtual nsEventStatus OnLongTap(AccessibleCaretEventHub* aContext,
                                  const nsPoint& aPoint)
  {
    return nsEventStatus_eIgnore;
  }

  virtual void OnScrollStart(AccessibleCaretEventHub* aContext) {}
  virtual void OnScrollEnd(AccessibleCaretEventHub* aContext) {}
  virtual void OnScrolling(AccessibleCaretEventHub* aContext) {}
  virtual void OnScrollPositionChanged(AccessibleCaretEventHub* aContext) {}
  virtual void OnBlur(AccessibleCaretEventHub* aContext,
                      bool aIsLeavingDocument) {}
  virtual void OnSelectionChanged(AccessibleCaretEventHub* aContext,
                                  nsIDOMDocument* aDoc, nsISelection* aSel,
                                  int16_t aReason) {}
  virtual void OnReflow(AccessibleCaretEventHub* aContext) {}
  virtual void Enter(AccessibleCaretEventHub* aContext) {}
  virtual void Leave(AccessibleCaretEventHub* aContext) {}

protected:
  explicit State() = default;
  virtual ~State() = default;
  State(const State&) = delete;
  void operator=(const State&) = delete;
};

} // namespace mozilla

#endif // AccessibleCaretEventHub_h
