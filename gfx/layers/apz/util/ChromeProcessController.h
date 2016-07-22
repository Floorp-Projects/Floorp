/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ChromeProcessController_h
#define mozilla_layers_ChromeProcessController_h

#include "mozilla/layers/GeckoContentController.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

class nsIDOMWindowUtils;
class nsIDocument;
class nsIPresShell;
class nsIWidget;

class MessageLoop;

namespace mozilla {

namespace layers {

class IAPZCTreeManager;
class APZEventState;

// A ChromeProcessController is attached to the root of a compositor's layer
// tree.
class ChromeProcessController : public mozilla::layers::GeckoContentController
{
protected:
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  explicit ChromeProcessController(nsIWidget* aWidget, APZEventState* aAPZEventState, IAPZCTreeManager* aAPZCTreeManager);
  ~ChromeProcessController();
  virtual void Destroy() override;

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) override;
  virtual void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) override;
  virtual void HandleTap(TapType aType,
                         const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId) override;
  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg) override;
  virtual void NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                         const nsString& aEvent) override;
  virtual void NotifyFlushComplete() override;
private:
  nsCOMPtr<nsIWidget> mWidget;
  RefPtr<APZEventState> mAPZEventState;
  RefPtr<IAPZCTreeManager> mAPZCTreeManager;
  MessageLoop* mUILoop;

  void InitializeRoot();
  nsIPresShell* GetPresShell() const;
  nsIDocument* GetRootDocument() const;
  nsIDocument* GetRootContentDocument(const FrameMetrics::ViewID& aScrollId) const;
  void HandleDoubleTap(const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                       const ScrollableLayerGuid& aGuid);
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_ChromeProcessController_h */
