/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZEventState_h
#define mozilla_layers_APZEventState_h

#include <stdint.h>

#include "FrameMetrics.h"     // for ScrollableLayerGuid
#include "Units.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Function.h"
#include "mozilla/layers/GeckoContentController.h"  // for APZStateChange
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr

template <class> class nsCOMPtr;
class nsIDocument;
class nsIPresShell;
class nsIWidget;

namespace mozilla {
namespace layers {

class ActiveElementManager;

typedef function<void(const ScrollableLayerGuid&,
                      uint64_t /* input block id */,
                      bool /* prevent default */)>
        ContentReceivedInputBlockCallback;

/**
 * A content-side component that keeps track of state for handling APZ
 * gestures and sending APZ notifications.
 */
class APZEventState {
  typedef GeckoContentController::APZStateChange APZStateChange;
  typedef FrameMetrics::ViewID ViewID;
public:
  APZEventState(nsIWidget* aWidget,
                ContentReceivedInputBlockCallback&& aCallback);

  NS_INLINE_DECL_REFCOUNTING(APZEventState);

  void ProcessSingleTap(const CSSPoint& aPoint,
                        const CSSToLayoutDeviceScale& aScale,
                        Modifiers aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        int32_t aClickCount);
  void ProcessLongTap(const nsCOMPtr<nsIPresShell>& aUtils,
                      const CSSPoint& aPoint,
                      const CSSToLayoutDeviceScale& aScale,
                      Modifiers aModifiers,
                      const ScrollableLayerGuid& aGuid,
                      uint64_t aInputBlockId);
  void ProcessLongTapUp(const nsCOMPtr<nsIPresShell>& aPresShell,
                        const CSSPoint& aPoint,
                        const CSSToLayoutDeviceScale& aScale,
                        Modifiers aModifiers);
  void ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId,
                         nsEventStatus aApzResponse,
                         nsEventStatus aContentResponse);
  void ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId);
  void ProcessMouseEvent(const WidgetMouseEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId);
  void ProcessAPZStateChange(ViewID aViewId,
                             APZStateChange aChange,
                             int aArg);
  void ProcessClusterHit();
private:
  ~APZEventState();
  bool SendPendingTouchPreventedResponse(bool aPreventDefault);
  bool FireContextmenuEvents(const nsCOMPtr<nsIPresShell>& aPresShell,
                             const CSSPoint& aPoint,
                             const CSSToLayoutDeviceScale& aScale,
                             Modifiers aModifiers,
                             const nsCOMPtr<nsIWidget>& aWidget);
  already_AddRefed<nsIWidget> GetWidget() const;
private:
  nsWeakPtr mWidget;
  RefPtr<ActiveElementManager> mActiveElementManager;
  ContentReceivedInputBlockCallback mContentReceivedInputBlockCallback;
  bool mPendingTouchPreventedResponse;
  ScrollableLayerGuid mPendingTouchPreventedGuid;
  uint64_t mPendingTouchPreventedBlockId;
  bool mEndTouchIsClick;
  bool mTouchEndCancelled;
  int32_t mLastTouchIdentifier;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_APZEventState_h */
