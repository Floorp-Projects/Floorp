/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteCompositorSession.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/Unused.h"
#include "nsBaseWidget.h"

namespace mozilla {
namespace layers {

using namespace gfx;
using namespace widget;

RemoteCompositorSession::RemoteCompositorSession(nsBaseWidget* aWidget,
                                                 CompositorBridgeChild* aChild,
                                                 CompositorWidgetDelegate* aWidgetDelegate,
                                                 APZCTreeManagerChild* aAPZ,
                                                 const uint64_t& aRootLayerTreeId)
 : CompositorSession(aWidgetDelegate, aChild, aRootLayerTreeId),
   mWidget(aWidget),
   mAPZ(aAPZ)
{
  GPUProcessManager::Get()->RegisterSession(this);
  if (mAPZ) {
    mAPZ->SetCompositorSession(this);
  }
}

RemoteCompositorSession::~RemoteCompositorSession()
{
  // This should have been shutdown first.
  MOZ_ASSERT(!mCompositorBridgeChild);
}

void
RemoteCompositorSession::NotifyDeviceReset(uint64_t aSeqNo)
{
  MOZ_ASSERT(mWidget);
  mWidget->OnRenderingDeviceReset(aSeqNo);
}

void
RemoteCompositorSession::NotifySessionLost()
{
  // Re-entrancy should be impossible: when we are being notified of a lost
  // session, we have by definition not shut down yet. We will shutdown, but
  // then will be removed from the notification list.
  MOZ_ASSERT(mWidget);
  mWidget->NotifyRemoteCompositorSessionLost(this);
}

CompositorBridgeParent*
RemoteCompositorSession::GetInProcessBridge() const
{
  return nullptr;
}

void
RemoteCompositorSession::SetContentController(GeckoContentController* aController)
{
  mContentController = aController;
  mCompositorBridgeChild->SendPAPZConstructor(new APZChild(aController), 0);
}

GeckoContentController*
RemoteCompositorSession::GetContentController()
{
  return mContentController.get();
}

nsIWidget*
RemoteCompositorSession::GetWidget()
{
  return mWidget;
}

RefPtr<IAPZCTreeManager>
RemoteCompositorSession::GetAPZCTreeManager() const
{
  return mAPZ;
}

bool
RemoteCompositorSession::Reset(const nsTArray<LayersBackend>& aBackendHints,
                               uint64_t aSeqNo,
                               TextureFactoryIdentifier* aOutIdentifier)
{
  bool didReset;
  Unused << mCompositorBridgeChild->SendReset(aBackendHints, aSeqNo, &didReset, aOutIdentifier);
  return didReset;
}

void
RemoteCompositorSession::Shutdown()
{
  mContentController = nullptr;
  if (mAPZ) {
    mAPZ->SetCompositorSession(nullptr);
  }
  mCompositorBridgeChild->Destroy();
  mCompositorBridgeChild = nullptr;
  mCompositorWidgetDelegate = nullptr;
  mWidget = nullptr;
  GPUProcessManager::Get()->UnregisterSession(this);
}

} // namespace layers
} // namespace mozilla
