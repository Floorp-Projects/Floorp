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
    int aSurfaceWidth, int aSurfaceHeight);

  CompositorBridgeParent* GetInProcessBridge() const override;
  void SetContentController(GeckoContentController* aController) override;
  uint64_t RootLayerTreeId() const override;
  APZCTreeManager* GetAPZCTreeManager() const override;
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
                                   int aSurfaceWidth, int aSurfaceHeight)
{
  RefPtr<InProcessCompositorSession> session = new InProcessCompositorSession(
    aWidgetProxy,
    aLayerManager,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceWidth, aSurfaceHeight);
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
                                                       int aSurfaceWidth, int aSurfaceHeight)
{
  mCompositorBridgeParent = new CompositorBridgeParent(
    aWidgetProxy,
    aScale,
    aUseAPZ,
    aUseExternalSurfaceSize,
    aSurfaceWidth,
    aSurfaceHeight);
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

APZCTreeManager*
InProcessCompositorSession::GetAPZCTreeManager() const
{
  return mCompositorBridgeParent->GetAPZCTreeManager(RootLayerTreeId());
}

void
InProcessCompositorSession::Shutdown()
{
  // XXX CompositorBridgeChild and CompositorBridgeParent might be re-created in
  // ClientLayerManager destructor. See bug 1133426.
  RefPtr<CompositorBridgeChild> compositorChild = mCompositorBridgeChild;
  RefPtr<CompositorBridgeParent> compositorParent = mCompositorBridgeParent;
  mCompositorBridgeChild->Destroy();
  mCompositorBridgeChild = nullptr;
}

} // namespace layers
} // namespace mozilla
