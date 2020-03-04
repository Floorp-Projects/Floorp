/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorSession.h"
#include "base/process_util.h"
#include "GPUChild.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/GPUProcessHost.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"

#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/widget/nsWindow.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

namespace mozilla {
namespace layers {

using namespace gfx;
using namespace widget;

CompositorSession::CompositorSession(nsBaseWidget* aWidget,
                                     CompositorWidgetDelegate* aDelegate,
                                     CompositorBridgeChild* aChild,
                                     const LayersId& aRootLayerTreeId)
    : mWidget(aWidget),
      mCompositorWidgetDelegate(aDelegate),
      mCompositorBridgeChild(aChild),
      mRootLayerTreeId(aRootLayerTreeId) {}

CompositorSession::~CompositorSession() = default;

CompositorBridgeChild* CompositorSession::GetCompositorBridgeChild() {
  return mCompositorBridgeChild;
}

#if defined(MOZ_WIDGET_ANDROID)
void CompositorSession::NotifyDisablingWebRender() {
  if (!mWidget) {
    return;
  }
  nsWindow* window = static_cast<nsWindow*>(mWidget);
  window->NotifyDisablingWebRender();
}
#endif  // defined(MOZ_WIDGET_ANDROID)

}  // namespace layers
}  // namespace mozilla
