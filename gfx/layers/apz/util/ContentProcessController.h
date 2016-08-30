/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ContentProcessController_h
#define mozilla_layers_ContentProcessController_h

#include "mozilla/layers/GeckoContentController.h"

class nsIObserver;

namespace mozilla {

namespace dom {
class TabChild;
} // namespace dom

namespace layers {

class APZChild;

/**
 * ContentProcessController is a GeckoContentController for a TabChild, and is always
 * remoted using PAPZ/APZChild.
 *
 * ContentProcessController is created in ContentChild when a layer tree id has
 * been allocated for a PBrowser that lives in that content process, and is destroyed
 * when the Destroy message is received, or when the tab dies.
 *
 * If ContentProcessController needs to implement a new method on GeckoContentController
 * PAPZ, APZChild, and RemoteContentController must be updated to handle it.
 */
class ContentProcessController final
      : public GeckoContentController
{
public:
  ~ContentProcessController();

  static APZChild* Create(const dom::TabId& aTabId);

  // ContentProcessController

  void SetBrowser(dom::TabChild* aBrowser);

  // GeckoContentController

  void RequestContentRepaint(const FrameMetrics& frame) override;

  void HandleTap(TapType aType,
                 const LayoutDevicePoint& aPoint,
                 Modifiers aModifiers,
                 const ScrollableLayerGuid& aGuid,
                 uint64_t aInputBlockId) override;

  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange,
                            int aArg) override;

  void NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                 const nsString& aEvent) override;

  void NotifyFlushComplete() override;

  void PostDelayedTask(already_AddRefed<Runnable> aRunnable, int aDelayMs) override;

  bool IsRepaintThread() override;

  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;

private:
  ContentProcessController();

  void SetObserver(nsIObserver* aObserver);

  RefPtr<dom::TabChild> mBrowser;
  RefPtr<nsIObserver> mObserver;
};

} // namespace layers

} // namespace mozilla

#endif // mozilla_layers_ContentProcessController_h
