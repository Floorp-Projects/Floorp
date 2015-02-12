/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ChromeProcessController_h
#define mozilla_layers_ChromeProcessController_h

#include "mozilla/layers/GeckoContentController.h"
#include "nsCOMPtr.h"
#include "nsRefPtr.h"

class nsIDOMWindowUtils;
class nsIPresShell;
class nsIWidget;

class MessageLoop;

namespace mozilla {

namespace layers {

class APZEventState;
class CompositorParent;

// A ChromeProcessController is attached to the root of a compositor's layer
// tree.
class ChromeProcessController : public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  explicit ChromeProcessController(nsIWidget* aWidget, APZEventState* aAPZEventState);
  virtual void Destroy() MOZ_OVERRIDE;

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE;
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration) MOZ_OVERRIDE;

  virtual void HandleDoubleTap(const mozilla::CSSPoint& aPoint, int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE {}
  virtual void HandleSingleTap(const mozilla::CSSPoint& aPoint, int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
  virtual void HandleLongTap(const mozilla::CSSPoint& aPoint, int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid,
                               uint64_t aInputBlockId) MOZ_OVERRIDE;
  virtual void HandleLongTapUp(const CSSPoint& aPoint, int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot, const mozilla::CSSRect &aContentRect,
                                       const mozilla::CSSSize &aScrollableSize) MOZ_OVERRIDE {}
  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg) MOZ_OVERRIDE;
private:
  nsCOMPtr<nsIWidget> mWidget;
  nsRefPtr<APZEventState> mAPZEventState;
  MessageLoop* mUILoop;

  void InitializeRoot();
  float GetPresShellResolution() const;
  nsIPresShell* GetPresShell() const;
  already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils() const;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_ChromeProcessController_h */
