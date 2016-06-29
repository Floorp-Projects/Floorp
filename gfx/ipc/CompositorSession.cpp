/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorSession.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "base/process_util.h"

namespace mozilla {
namespace layers {

class InProcessCompositorSession final : public CompositorSession
{
public:
  InProcessCompositorSession(
    widget::CompositorWidgetProxy* aWidgetProxy,
    ClientLayerManager* aLayerManager,
    CSSToLayoutDeviceScale aScale,
    bool aUseAPZ,
    bool aUseExternalSurfaceSize,
    const gfx::IntSize& aSurfaceSize);

  CompositorBridgeParent* GetInProcessBridge() const override;
  void SetContentController(GeckoContentController* aController) override;
  uint64_t RootLayerTreeId() const override;
  already_AddRefed<APZCTreeManager> GetAPZCTreeManager() const override;
  void Shutdown() override;

private:
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;
};

already_AddRefed<CompositorSession>
CompositorSession::CreateInProcess(widget::CompositorWidgetProxy* aWidgetProxy,
                                   ClientLayerManager* aLayerManager,
                                   CSSToLayoutDeviceScale aScale,
                                   bool aUseAPZ,
                                   bool aUseExternalSurfaceSize,
                                   const gfx::IntSize& aSurfaceSize)
{
  RefPtr<InProcessCompositorSession> session = new InProcessCompositorSession(
    aWidgetProxy,
    aLayerManager,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceSize);
  return session.forget();
}

CompositorSession::CompositorSession()
{
}

CompositorSession::~CompositorSession()
{
}

CompositorBridgeChild*
CompositorSession::GetCompositorBridgeChild()
{
  return mCompositorBridgeChild;
}

InProcessCompositorSession::InProcessCompositorSession(widget::CompositorWidgetProxy* aWidgetProxy,
                                                       ClientLayerManager* aLayerManager,
                                                       CSSToLayoutDeviceScale aScale,
                                                       bool aUseAPZ,
                                                       bool aUseExternalSurfaceSize,
                                                       const gfx::IntSize& aSurfaceSize)
{
  mCompositorBridgeParent = new CompositorBridgeParent(
    aWidgetProxy,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceSize);
  mCompositorBridgeChild = new CompositorBridgeChild(aLayerManager);
  mCompositorBridgeChild->OpenSameProcess(mCompositorBridgeParent);
  mCompositorBridgeParent->SetOtherProcessId(base::GetCurrentProcId());
}

CompositorBridgeParent*
InProcessCompositorSession::GetInProcessBridge() const
{
  return mCompositorBridgeParent;
}

void
InProcessCompositorSession::SetContentController(GeckoContentController* aController)
{
  mCompositorBridgeParent->SetControllerForLayerTree(RootLayerTreeId(), aController);
}

uint64_t
InProcessCompositorSession::RootLayerTreeId() const
{
  return mCompositorBridgeParent->RootLayerTreeId();
}

already_AddRefed<APZCTreeManager>
InProcessCompositorSession::GetAPZCTreeManager() const
{
  return mCompositorBridgeParent->GetAPZCTreeManager(RootLayerTreeId());
}

void
InProcessCompositorSession::Shutdown()
{
  // Destroy will synchronously wait for the parent to acknowledge shutdown,
  // at which point CBP will defer a Release on the compositor thread. We
  // can safely release our reference now, and let the destructor run on either
  // thread.
  mCompositorBridgeChild->Destroy();
  mCompositorBridgeChild = nullptr;
  mCompositorBridgeParent = nullptr;
}

} // namespace layers
} // namespace mozilla
