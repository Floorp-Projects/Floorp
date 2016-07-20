/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_InProcessCompositorSession_h_
#define _include_mozilla_gfx_ipc_InProcessCompositorSession_h_

#include "CompositorSession.h"
#include "mozilla/gfx/Point.h"
#include "Units.h"

namespace mozilla {
namespace layers {

// A CompositorSession where both the child and parent CompositorBridge reside
// in the same process.
class InProcessCompositorSession final : public CompositorSession
{
public:
  static RefPtr<InProcessCompositorSession> Create(
    nsIWidget* aWidget,
    ClientLayerManager* aLayerManager,
    const uint64_t& aRootLayerTreeId,
    CSSToLayoutDeviceScale aScale,
    bool aUseAPZ,
    bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize);

  CompositorBridgeParent* GetInProcessBridge() const override;
  void SetContentController(GeckoContentController* aController) override;
  already_AddRefed<IAPZCTreeManager> GetAPZCTreeManager() const override;
  void Shutdown() override;

private:
  InProcessCompositorSession(widget::CompositorWidget* aWidget,
                             CompositorBridgeChild* aChild,
                             CompositorBridgeParent* aParent);

private:
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;
  RefPtr<CompositorWidget> mCompositorWidget;
};

} // namespace layers
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_InProcessCompositorSession_h_
