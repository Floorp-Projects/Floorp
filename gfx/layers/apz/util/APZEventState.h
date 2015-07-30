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
#include "mozilla/layers/GeckoContentController.h"  // for APZStateChange
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWeakReferenceUtils.h"  // for nsWeakPtr
#include "mozilla/nsRefPtr.h"

template <class> class nsCOMPtr;
class nsIDocument;
class nsIPresShell;
class nsIWidget;

namespace mozilla {
namespace layers {

class ActiveElementManager;

struct ContentReceivedInputBlockCallback {
public:
  NS_INLINE_DECL_REFCOUNTING(ContentReceivedInputBlockCallback);
  virtual void Run(const ScrollableLayerGuid& aGuid,
                   uint64_t aInputBlockId,
                   bool aPreventDefault) const = 0;
protected:
  virtual ~ContentReceivedInputBlockCallback() {}
};

/**
 * A content-side component that keeps track of state for handling APZ
 * gestures and sending APZ notifications.
 */
class APZEventState {
  typedef GeckoContentController::APZStateChange APZStateChange;
  typedef FrameMetrics::ViewID ViewID;
public:
  APZEventState(nsIWidget* aWidget,
                const nsRefPtr<ContentReceivedInputBlockCallback>& aCallback);

  NS_INLINE_DECL_REFCOUNTING(APZEventState);

  void ProcessSingleTap(const CSSPoint& aPoint,
                        Modifiers aModifiers,
                        const ScrollableLayerGuid& aGuid);
  void ProcessLongTap(const nsCOMPtr<nsIPresShell>& aUtils,
                      const CSSPoint& aPoint,
                      Modifiers aModifiers,
                      const ScrollableLayerGuid& aGuid,
                      uint64_t aInputBlockId);
  void ProcessTouchEvent(const WidgetTouchEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId,
                         nsEventStatus aApzResponse);
  void ProcessWheelEvent(const WidgetWheelEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId);
  void ProcessAPZStateChange(const nsCOMPtr<nsIDocument>& aDocument,
                             ViewID aViewId,
                             APZStateChange aChange,
                             int aArg);
private:
  ~APZEventState();
  bool SendPendingTouchPreventedResponse(bool aPreventDefault);
  already_AddRefed<nsIWidget> GetWidget() const;
private:
  nsWeakPtr mWidget;
  nsRefPtr<ActiveElementManager> mActiveElementManager;
  nsRefPtr<ContentReceivedInputBlockCallback> mContentReceivedInputBlockCallback;
  bool mPendingTouchPreventedResponse;
  ScrollableLayerGuid mPendingTouchPreventedGuid;
  uint64_t mPendingTouchPreventedBlockId;
  bool mEndTouchIsClick;
  bool mTouchEndCancelled;
  int mActiveAPZTransforms;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_APZEventState_h */
