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
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "PersistentBufferProvider.h"
#include "SharedSurface.h"
#include "SharedSurfaceGL.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

WebRenderCanvasLayer::~WebRenderCanvasLayer()
{
  MOZ_COUNT_DTOR(WebRenderCanvasLayer);

  if (mExternalImageId.isSome()) {
    WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
  }
}

void
WebRenderCanvasLayer::Initialize(const Data& aData)
{
  ShareableCanvasLayer::Initialize(aData);

  // XXX: Use basic surface factory until we support shared surface.
  if (!mGLContext || mGLFrontbuffer)
    return;

  gl::GLScreenBuffer* screen = mGLContext->Screen();
  auto factory = MakeUnique<gl::SurfaceFactory_Basic>(mGLContext, screen->mCaps, mFlags);
  screen->Morph(Move(factory));
}

void
WebRenderCanvasLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  UpdateCompositableClient();

  if (mExternalImageId.isNothing()) {
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mCanvasClient));
  }

  gfx::Matrix4x4 transform = GetTransform();
  const bool needsYFlip = (mOriginPos == gl::OriginPos::BottomLeft);
  if (needsYFlip) {
    transform.PreTranslate(0, mBounds.height, 0).PreScale(1, -1, 1);
  }

  gfx::Rect relBounds = GetWrRelBounds();
  gfx::Rect rect = RelativeToVisible(gfx::Rect(0, 0, mBounds.width, mBounds.height));

  gfx::Rect clipRect = GetWrClipRect(rect);
  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  WrClipRegion clip = aBuilder.BuildClipRegion(wr::ToWrRect(clipRect), mask.ptrOr(nullptr));

  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
  wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  DumpLayerInfo("CanvasLayer", rect);
  if (gfxPrefs::LayersDump()) {
    printf_stderr("CanvasLayer %p texture-filter=%s\n",
                  this->GetLayer(),
                  Stringify(filter).c_str());
  }

  WrImageKey key = GetImageKey();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId.value(), key));
  Manager()->AddImageKeyForDiscard(key);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                               1.0f,
                               //GetAnimations(),
                               transform,
                               mixBlendMode);
  aBuilder.PushImage(wr::ToWrRect(rect), clip, filter, key);
  aBuilder.PopStackingContext();
}

void
WebRenderCanvasLayer::AttachCompositable()
{
  mCanvasClient->Connect();
}

CompositableForwarder*
WebRenderCanvasLayer::GetForwarder()
{
  return Manager()->WrBridge();
}

} // namespace layers
} // namespace mozilla
