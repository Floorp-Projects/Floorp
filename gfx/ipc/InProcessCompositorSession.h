/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_InProcessCompositorSession_h_
#define _include_mozilla_gfx_ipc_InProcessCompositorSession_h_

#include "CompositorSession.h"
#include "mozilla/gfx/Point.h"
#include "Units.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class CompositorOptions;
class LayerManager;

// A CompositorSession where both the child and parent CompositorBridge reside
// in the same process.
class InProcessCompositorSession final : public CompositorSession {
 public:
  static RefPtr<InProcessCompositorSession> Create(
      nsBaseWidget* baseWidget, LayerManager* aLayerManager,
      const LayersId& aRootLayerTreeId, CSSToLayoutDeviceScale aScale,
      const CompositorOptions& aOptions, bool aUseExternalSurfaceSize,
      const gfx::IntSize& aSurfaceSize, uint32_t aNamespace);

  CompositorBridgeParent* GetInProcessBridge() const override;
  void SetContentController(GeckoContentController* aController) override;
  nsIWidget* GetWidget() const;
  RefPtr<IAPZCTreeManager> GetAPZCTreeManager() const override;
  void Shutdown() override;

  void NotifySessionLost();

 private:
  InProcessCompositorSession(nsBaseWidget* aWidget,
                             widget::CompositorWidget* aCompositorWidget,
                             CompositorBridgeChild* aChild,
                             CompositorBridgeParent* aParent);

 private:
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;
  RefPtr<CompositorWidget> mCompositorWidget;
};

}  // namespace layers
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_InProcessCompositorSession_h_
