/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteCompositorSession.h"

#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCTreeManagerChild.h"

namespace mozilla {
namespace layers {

using namespace gfx;
using namespace widget;

RemoteCompositorSession::RemoteCompositorSession(CompositorBridgeChild* aChild,
                                                 CompositorWidgetDelegate* aWidgetDelegate,
                                                 APZCTreeManagerChild* aAPZ,
                                                 const uint64_t& aRootLayerTreeId)
 : CompositorSession(aWidgetDelegate, aChild, aRootLayerTreeId)
 , mAPZ(aAPZ)
{
}

CompositorBridgeParent*
RemoteCompositorSession::GetInProcessBridge() const
{
  return nullptr;
}

void
RemoteCompositorSession::SetContentController(GeckoContentController* aController)
{
  mCompositorBridgeChild->SendPAPZConstructor(new APZChild(aController), 0);
}

RefPtr<IAPZCTreeManager>
RemoteCompositorSession::GetAPZCTreeManager() const
{
  return mAPZ;
}

void
RemoteCompositorSession::Shutdown()
{
  mCompositorBridgeChild->Destroy();
  mCompositorBridgeChild = nullptr;
  mCompositorWidgetDelegate = nullptr;
}

} // namespace layers
} // namespace mozilla
