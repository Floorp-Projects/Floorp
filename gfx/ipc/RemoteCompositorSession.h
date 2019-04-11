/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_mozilla_gfx_ipc_RemoteCompositorSession_h
#define include_mozilla_gfx_ipc_RemoteCompositorSession_h

#include "CompositorSession.h"
#include "mozilla/gfx/Point.h"
#include "Units.h"

namespace mozilla {
namespace layers {

class RemoteCompositorSession final : public CompositorSession {
 public:
  RemoteCompositorSession(nsBaseWidget* aWidget, CompositorBridgeChild* aChild,
                          CompositorWidgetDelegate* aWidgetDelegate,
                          APZCTreeManagerChild* aAPZ,
                          const LayersId& aRootLayerTreeId);
  virtual ~RemoteCompositorSession();

  CompositorBridgeParent* GetInProcessBridge() const override;
  void SetContentController(GeckoContentController* aController) override;
  GeckoContentController* GetContentController();
  nsIWidget* GetWidget() const;
  RefPtr<IAPZCTreeManager> GetAPZCTreeManager() const override;
  void Shutdown() override;

  void NotifySessionLost();

 private:
  nsBaseWidget* mWidget;
  RefPtr<APZCTreeManagerChild> mAPZ;
  RefPtr<GeckoContentController> mContentController;
};

}  // namespace layers
}  // namespace mozilla

#endif  // include_mozilla_gfx_ipc_RemoteCompositorSession_h
