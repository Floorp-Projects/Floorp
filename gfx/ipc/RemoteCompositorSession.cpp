/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteCompositorSession.h"
#include "gfxPlatform.h"
#include "mozilla/VsyncDispatcher.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/Unused.h"
#include "nsBaseWidget.h"
#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/layers/UiCompositorControllerChild.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

namespace mozilla {
namespace layers {

using namespace gfx;
using namespace widget;

RemoteCompositorSession::RemoteCompositorSession(
    nsBaseWidget* aWidget, CompositorBridgeChild* aChild,
    CompositorWidgetDelegate* aWidgetDelegate, APZCTreeManagerChild* aAPZ,
    const LayersId& aRootLayerTreeId)
    : CompositorSession(aWidget, aWidgetDelegate, aChild, aRootLayerTreeId),
      mAPZ(aAPZ) {
  MOZ_ASSERT(!gfxPlatform::IsHeadless());
  GPUProcessManager::Get()->RegisterRemoteProcessSession(this);
  if (mAPZ) {
    mAPZ->SetCompositorSession(this);
  }
}

RemoteCompositorSession::~RemoteCompositorSession() {
  // This should have been shutdown first.
  MOZ_ASSERT(!mCompositorBridgeChild);
#if defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(!mUiCompositorControllerChild);
#endif  // defined(MOZ_WIDGET_ANDROID)
}

void RemoteCompositorSession::NotifySessionLost() {
  // Re-entrancy should be impossible: when we are being notified of a lost
  // session, we have by definition not shut down yet. We will shutdown, but
  // then will be removed from the notification list.
  mWidget->NotifyCompositorSessionLost(this);
}

CompositorBridgeParent* RemoteCompositorSession::GetInProcessBridge() const {
  return nullptr;
}

void RemoteCompositorSession::SetContentController(
    GeckoContentController* aController) {
  mContentController = aController;
  mCompositorBridgeChild->SendPAPZConstructor(new APZChild(aController),
                                              LayersId{0});
}

GeckoContentController* RemoteCompositorSession::GetContentController() {
  return mContentController.get();
}

nsIWidget* RemoteCompositorSession::GetWidget() const { return mWidget; }

RefPtr<IAPZCTreeManager> RemoteCompositorSession::GetAPZCTreeManager() const {
  return mAPZ;
}

void RemoteCompositorSession::Shutdown() {
  mContentController = nullptr;
  if (mAPZ) {
    mAPZ->SetCompositorSession(nullptr);
    mAPZ->Destroy();
  }
  mCompositorBridgeChild->Destroy();
  mCompositorBridgeChild = nullptr;
  mCompositorWidgetDelegate = nullptr;
  mWidget = nullptr;
#if defined(MOZ_WIDGET_ANDROID)
  if (mUiCompositorControllerChild) {
    mUiCompositorControllerChild->Destroy();
    mUiCompositorControllerChild = nullptr;
  }
#endif  // defined(MOZ_WIDGET_ANDROID)
  GPUProcessManager::Get()->UnregisterRemoteProcessSession(this);
}

}  // namespace layers
}  // namespace mozilla
