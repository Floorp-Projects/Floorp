/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ChromeProcessController_h
#define mozilla_layers_ChromeProcessController_h

#include "mozilla/layers/GeckoContentController.h"
#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"

class nsIDOMWindowUtils;
class nsIDocument;
class nsIPresShell;
class nsIWidget;

class MessageLoop;

namespace mozilla {

namespace layers {

class APZEventState;

// A ChromeProcessController is attached to the root of a compositor's layer
// tree.
class ChromeProcessController : public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  explicit ChromeProcessController(nsIWidget* aWidget, APZEventState* aAPZEventState);
  virtual void Destroy() override;

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) override;
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) override;
  virtual void RequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                                const mozilla::CSSPoint& aDestination) override;
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration) override;

  virtual void HandleDoubleTap(const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                               const ScrollableLayerGuid& aGuid) override {}
  virtual void HandleSingleTap(const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                               const ScrollableLayerGuid& aGuid) override;
  virtual void HandleLongTap(const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                               const ScrollableLayerGuid& aGuid,
                               uint64_t aInputBlockId) override;
  virtual void SendAsyncScrollDOMEvent(bool aIsRootContent, const mozilla::CSSRect &aContentRect,
                                       const mozilla::CSSSize &aScrollableSize) override {}
  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg) override;
  virtual void NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                         const nsString& aEvent) override;
  virtual void NotifyFlushComplete() override;
private:
  nsCOMPtr<nsIWidget> mWidget;
  nsRefPtr<APZEventState> mAPZEventState;
  MessageLoop* mUILoop;

  void InitializeRoot();
  nsIPresShell* GetPresShell() const;
  nsIDocument* GetDocument() const;
  already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils() const;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_ChromeProcessController_h */
