/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCanvasLayer.h"

#include "AsyncCanvasRenderer.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "LayersLogging.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "PersistentBufferProvider.h"
#include "SharedSurface.h"
#include "SharedSurfaceGL.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "WebRenderCanvasRenderer.h"

namespace mozilla {
namespace layers {

WebRenderCanvasLayer::~WebRenderCanvasLayer()
{
  MOZ_COUNT_DTOR(WebRenderCanvasLayer);
}

CanvasRenderer*
WebRenderCanvasLayer::CreateCanvasRendererInternal()
{
  return new WebRenderCanvasRendererSync(mManager->AsWebRenderLayerManager());
}

void
WebRenderCanvasLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                  wr::IpcResourceUpdateQueue& aResources,
                                  const StackingContextHelper& aSc)
{
  WebRenderCanvasRendererSync* canvasRenderer = mCanvasRenderer->AsWebRenderCanvasRendererSync();
  MOZ_ASSERT(canvasRenderer);
  canvasRenderer->UpdateCompositableClient();

  Maybe<gfx::Matrix4x4> transform;
  if (canvasRenderer->NeedsYFlip()) {
    transform = Some(GetTransform().PreTranslate(0, mBounds.Height(), 0).PreScale(1, -1, 1));
  }

  ScrollingLayersHelper scroller(this, aBuilder, aResources, aSc);
  StackingContextHelper sc(aSc, aBuilder, this, transform);

  LayerRect rect(0, 0, mBounds.Width(), mBounds.Height());
  DumpLayerInfo("CanvasLayer", rect);

  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);

  if (gfxPrefs::LayersDump()) {
    printf_stderr("CanvasLayer %p texture-filter=%s\n",
                  this->GetLayer(),
                  Stringify(filter).c_str());
  }

  wr::WrImageKey key = GenerateImageKey();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(canvasRenderer->GetExternalImageId().value(), key));
  WrManager()->AddImageKeyForDiscard(key);

  wr::LayoutRect r = sc.ToRelativeLayoutRect(rect);
  aBuilder.PushImage(r, r, filter, key);
}

void
WebRenderCanvasLayer::ClearCachedResources()
{
  mCanvasRenderer->ClearCachedResources();
}

} // namespace layers
} // namespace mozilla
