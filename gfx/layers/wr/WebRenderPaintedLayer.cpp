/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderPaintedLayer.h"

#include "LayersLogging.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderPaintedLayer::PaintThebes(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  PROFILER_LABEL("WebRenderPaintedLayer", "PaintThebes",
    js::ProfileEntry::Category::GRAPHICS);

  mContentClient->BeginPaint();

  uint32_t flags = RotatedContentBuffer::PAINT_CAN_DRAW_ROTATED;

  PaintState state =
    mContentClient->BeginPaintBuffer(this, flags);
  mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

  if (!state.mRegionToDraw.IsEmpty() && !Manager()->GetPaintedLayerCallback()) {
    return;
  }

  // The area that became invalid and is visible needs to be repainted
  // (this could be the whole visible area if our buffer switched
  // from RGB to RGBA, because we might need to repaint with
  // subpixel AA)
  state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                GetLocalVisibleRegion().ToUnknownRegion());

  bool didUpdate = false;
  RotatedContentBuffer::DrawIterator iter;
  while (DrawTarget* target = mContentClient->BorrowDrawTargetForPainting(state, &iter)) {
    if (!target || !target->IsValid()) {
      if (target) {
        mContentClient->ReturnDrawTargetToBuffer(target);
      }
      continue;
    }

    SetAntialiasingFlags(this, target);

    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above
    Manager()->GetPaintedLayerCallback()(this,
                                              ctx,
                                              iter.mDrawRegion,
                                              iter.mDrawRegion,
                                              state.mClip,
                                              state.mRegionToInvalidate,
                                              Manager()->GetPaintedLayerCallbackData());

    ctx = nullptr;
    mContentClient->ReturnDrawTargetToBuffer(target);
    didUpdate = true;
  }

  mContentClient->EndPaint(aReadbackUpdates);

  if (didUpdate) {
    Mutated();

    // XXX It will cause reftests failures. See Bug 1340798.
    //mValidRegion.Or(mValidRegion, state.mRegionToDraw);

    ContentClientRemote* contentClientRemote = static_cast<ContentClientRemote*>(mContentClient.get());

    // Hold(this) ensures this layer is kept alive through the current transaction
    // The ContentClient assumes this layer is kept alive (e.g., in CreateBuffer),
    // so deleting this Hold for whatever reason will break things.
    Manager()->Hold(this);

    contentClientRemote->Updated(state.mRegionToDraw,
                                 mVisibleRegion.ToUnknownRegion(),
                                 state.mDidSelfCopy);
  }
}

void
WebRenderPaintedLayer::RenderLayerWithReadback(ReadbackProcessor *aReadback)
{
  if (!mContentClient) {
    mContentClient = ContentClient::CreateContentClient(Manager()->WrBridge());
    if (!mContentClient) {
      return;
    }
    mContentClient->Connect();
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetPaintedLayerUpdates(this, &readbackUpdates);
  }

  PaintThebes(&readbackUpdates);
}

void
WebRenderPaintedLayer::RenderLayer()
{
  // XXX We won't keep using ContentClient for WebRenderPaintedLayer in the future and
  // there is a crash problem for ContentClient on MacOS. So replace ContentClient with
  // ImageClient. See bug 1341001.
  //RenderLayerWithReadback(nullptr);

  if (!mImageContainer) {
    mImageContainer = LayerManager::CreateImageContainer();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  if (!mExternalImageId) {
    mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
    MOZ_ASSERT(mExternalImageId);
  }

  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  LayerIntSize size = bounds.Size();
  if (size.IsEmpty()) {
      if (gfxPrefs::LayersDump()) {
        printf_stderr("PaintedLayer %p skipping\n", this->GetLayer());
      }
      return;
  }

  IntSize imageSize(size.width, size.height);
  RefPtr<TextureClient> texture = mImageClient->GetTextureClientRecycler()->CreateOrRecycle(SurfaceFormat::B8G8R8A8,
                                                                                            imageSize,
                                                                                            BackendSelector::Content,
                                                                                            TextureFlags::DEFAULT);
  if (!texture) {
    return;
  }

  {
    TextureClientAutoLock autoLock(texture, OpenMode::OPEN_WRITE_ONLY);
    if (!autoLock.Succeeded()) {
      return;
    }
    RefPtr<DrawTarget> target = texture->BorrowDrawTarget();
    if (!target) {
      return;
    }
    target->ClearRect(Rect(0, 0, imageSize.width, imageSize.height));
    target->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above

    Manager()->GetPaintedLayerCallback()(this,
                                         ctx,
                                         visibleRegion.ToUnknownRegion(), visibleRegion.ToUnknownRegion(),
                                         DrawRegionClip::DRAW, nsIntRegion(), Manager()->GetPaintedLayerCallbackData());
  }
  RefPtr<TextureWrapperImage> image = new TextureWrapperImage(texture, IntRect(IntPoint(0, 0), imageSize));
  mImageContainer->SetCurrentImageInTransaction(image);
  if (!mImageClient->UpdateImage(mImageContainer, /* unused */0)) {
    return;
   }
 
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  Matrix4x4 transform = GetTransform();

  // Since we are creating a stacking context below using the visible region of
  // this layer, we need to make sure the image display item has coordinates
  // relative to the visible region.
  Rect rect(0, 0, size.width, size.height);
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToVisible(transform.Inverse().TransformBounds(IntRectToRect(GetClipRect().ref().ToUnknownRect())));
  } else {
      clip = rect;
  }

  Maybe<WrImageMask> mask = buildMaskLayer();
  Rect relBounds = VisibleBoundsRelativeToParent();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    gfx::Matrix4x4 boundTransform = transform;
    boundTransform._41 = 0.0f;
    boundTransform._42 = 0.0f;
    boundTransform._43 = 0.0f;
    relBounds.MoveTo(boundTransform.TransformPoint(relBounds.TopLeft()));
  }

  Rect overflow(0, 0, relBounds.width, relBounds.height);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  if (gfxPrefs::LayersDump()) {
    printf_stderr("PaintedLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
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
  WrBridge()->AddWebRenderCommand(OpDPPushImage(wr::ToWrRect(rect), wr::ToWrRect(clip), Nothing(), wr::ImageRendering::Auto, key));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

} // namespace layers
} // namespace mozilla
