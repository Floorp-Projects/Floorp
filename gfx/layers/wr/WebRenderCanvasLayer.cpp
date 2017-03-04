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

  if (mExternalImageId) {
    WrBridge()->DeallocExternalImageId(mExternalImageId);
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
WebRenderCanvasLayer::RenderLayer()
{
  UpdateCompositableClient();

  if (!mExternalImageId) {
    mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mCanvasClient);
  }

  MOZ_ASSERT(mExternalImageId);

  gfx::Matrix4x4 transform = GetTransform();
  const bool needsYFlip = (mOriginPos == gl::OriginPos::BottomLeft);
  if (needsYFlip) {
    transform.PreTranslate(0, mBounds.height, 0).PreScale(1, -1, 1);
  }
  gfx::Rect rect(0, 0, mBounds.width, mBounds.height);
  rect = RelativeToVisible(rect);

  gfx::Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToVisible(transform.Inverse().TransformBounds(IntRectToRect(GetClipRect().ref().ToUnknownRect())));
  } else {
      clip = rect;
  }

  gfx::Rect relBounds = VisibleBoundsRelativeToParent();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    gfx::Matrix4x4 boundTransform = transform;
    boundTransform._41 = 0.0f;
    boundTransform._42 = 0.0f;
    boundTransform._43 = 0.0f;
    relBounds.MoveTo(boundTransform.TransformPoint(relBounds.TopLeft()));
  }

  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  Maybe<WrImageMask> mask = buildMaskLayer();
  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  if (gfxPrefs::LayersDump()) {
    printf_stderr("CanvasLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, texture-filter=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
                  Stringify(filter).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  WrBridge()->AddWebRenderCommand(
      OpDPPushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              mask,
                              1.0f,
                              GetAnimations(),
                              transform,
                              mixBlendMode,
                              FrameMetrics::NULL_SCROLL_ID));
  WrImageKey key;
  key.mNamespace = WrBridge()->GetNamespace();
  key.mHandle = WrBridge()->GetNextResourceId();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(mExternalImageId, key));
  WrBridge()->AddWebRenderCommand(OpDPPushImage(wr::ToWrRect(rect), wr::ToWrRect(clip), Nothing(), filter, key));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

void
WebRenderCanvasLayer::AttachCompositable()
{
  mCanvasClient->Connect();
}

} // namespace layers
} // namespace mozilla
