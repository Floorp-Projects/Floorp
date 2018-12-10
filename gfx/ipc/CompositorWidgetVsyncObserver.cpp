/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetVsyncObserver.h"
#include "mozilla/gfx/VsyncBridgeChild.h"

namespace mozilla {
namespace widget {

CompositorWidgetVsyncObserver::CompositorWidgetVsyncObserver(
    RefPtr<VsyncBridgeChild> aVsyncBridge,
    const layers::LayersId& aRootLayerTreeId)
    : mVsyncBridge(aVsyncBridge), mRootLayerTreeId(aRootLayerTreeId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

bool CompositorWidgetVsyncObserver::NotifyVsync(const VsyncEvent& aVsync) {
  // Vsync notifications should only arrive on the vsync thread.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!NS_IsMainThread());

  mVsyncBridge->NotifyVsync(aVsync, mRootLayerTreeId);
  return true;
}

}  // namespace widget
}  // namespace mozilla
