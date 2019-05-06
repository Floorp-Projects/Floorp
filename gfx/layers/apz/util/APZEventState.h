/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZEventState_h
#define mozilla_layers_APZEventState_h

#include <stdint.h>

#include "Units.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/GeckoContentController.h"  // for APZStateChange
#include "mozilla/layers/ScrollableLayerGuid.h"     // for ScrollableLayerGuid
#include "mozilla/layers/TouchCounter.h"            // for TouchCounter
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"        // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr

#include <functional>

template <class>
class nsCOMPtr;
class nsIContent;
class nsIWidget;

namespace mozilla {

class PresShell;

namespace layers {

class ActiveElementManager;

typedef std::function<void(uint64_t /* input block id */,
                           bool /* prevent default */)>
    ContentReceivedInputBlockCallback;

/**
 * A content-side component that keeps track of state for handling APZ
 * gestures and sending APZ notifications.
 */
class APZEventState final {
  typedef GeckoContentController::APZStateChange APZStateChange;
  typedef ScrollableLayerGuid::ViewID ViewID;

 public:
  APZEventState(nsIWidget* aWidget,
                ContentReceivedInputBlockCallback&& aCallback);

  NS_INLINE_DECL_REFCOUNTING(APZEventState);

  void ProcessSingleTap(const CSSPoint& aPoint,
                        const CSSToLayoutDeviceScale& aScale,
                        Modifiers aModifiers, int32_t aClickCount);
  MOZ_CAN_RUN_SCRIPT
  void ProcessLongTap(PresShell* aPresShell, const CSSPoint& aPoint,
                      const CSSToLayoutDeviceScale& aScale,
                      Modifiers aModifiers, uint64_t aInputBlockId);
  MOZ_CAN_RUN_SCRIPT
  void ProcessLongTapUp(PresShell* aPresShell, const CSSPoint& aPoint,
                        const CSSToLayoutDeviceScale& aScale,
                        Modifiers aModifiers);
  void ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId, nsEventStatus aApzResponse,
                         nsEventStatus aContentResponse);
  void ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                         uint64_t aInputBlockId);
  void ProcessMouseEvent(const WidgetMouseEvent& aEvent,
                         uint64_t aInputBlockId);
  void ProcessAPZStateChange(ViewID aViewId, APZStateChange aChange, int aArg);
  void ProcessClusterHit();

 private:
  ~APZEventState();
  bool SendPendingTouchPreventedResponse(bool aPreventDefault);
  MOZ_CAN_RUN_SCRIPT
  bool FireContextmenuEvents(PresShell* aPresShell, const CSSPoint& aPoint,
                             const CSSToLayoutDeviceScale& aScale,
                             Modifiers aModifiers,
                             const nsCOMPtr<nsIWidget>& aWidget);
  already_AddRefed<nsIWidget> GetWidget() const;
  already_AddRefed<nsIContent> GetTouchRollup() const;

 private:
  nsWeakPtr mWidget;
  RefPtr<ActiveElementManager> mActiveElementManager;
  ContentReceivedInputBlockCallback mContentReceivedInputBlockCallback;
  TouchCounter mTouchCounter;
  bool mPendingTouchPreventedResponse;
  ScrollableLayerGuid mPendingTouchPreventedGuid;
  uint64_t mPendingTouchPreventedBlockId;
  bool mEndTouchIsClick;
  bool mFirstTouchCancelled;
  bool mTouchEndCancelled;
  int32_t mLastTouchIdentifier;

  // Because touch-triggered mouse events (e.g. mouse events from a tap
  // gesture) happen asynchronously from the touch events themselves, we
  // need to stash and replicate some of the state from the touch events
  // to the mouse events. One piece of state is the rollup content, which
  // is the content for which a popup window was recently closed. If we
  // don't replicate this state properly during the mouse events, the
  // synthetic click might reopen a popup window that was just closed by
  // the touch event, which is undesirable. See also documentation in
  // nsAutoRollup.h
  // Note that in cases where we get multiple touch blocks interleaved with
  // their single-tap event notifications, mTouchRollup may hold an incorrect
  // value. This is kind of an edge case, and falls in the same category of
  // problems as bug 1227241. I intend that fixing that bug will also take
  // care of this potential problem.
  nsWeakPtr mTouchRollup;
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_APZEventState_h */
