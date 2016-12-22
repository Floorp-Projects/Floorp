/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderPaintedLayer.h"

#include "LayersLogging.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

WebRenderPaintedLayer::~WebRenderPaintedLayer()
{
  MOZ_COUNT_DTOR(WebRenderPaintedLayer);

  if (mExternalImageId) {
    Manager()->AddExternalImageIdForDiscard(mExternalImageId);
  }
}

void
WebRenderPaintedLayer::RenderLayer()
{
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  gfx::IntSize size = bounds.Size().ToUnknownSize();

  if (size.IsEmpty()) {
      return;
  }

  if (!mImageContainer) {
    mImageContainer = LayerManager::CreateImageContainer();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WRBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  // If we already have previous frame's mExternalImageId, remove that id here.
  if (mExternalImageId) {
    Manager()->AddExternalImageIdForDiscard(mExternalImageId);
  }
  // Try to use different image id for each content updates.
  mExternalImageId = WRBridge()->AllocExternalImageIdForCompositable(mImageClient,
                                                                     size,
                                                                     SurfaceFormat::B8G8R8A8);
  MOZ_ASSERT(mExternalImageId);

  RefPtr<TextureClient> texture = mImageClient->GetTextureClientRecycler()
    ->CreateOrRecycle(SurfaceFormat::B8G8R8A8,
                      size,
                      BackendSelector::Content,
                      TextureFlags::DEFAULT);
  if (!texture) {
    return;
  }
  MOZ_ASSERT(texture->CanExposeDrawTarget());

  {
    TextureClientAutoLock autoLock(texture, OpenMode::OPEN_WRITE_ONLY);
    if (!autoLock.Succeeded()) {
      return;
    }
    RefPtr<DrawTarget> target = texture->BorrowDrawTarget();
    if (!target || !target->IsValid()) {
      return;
    }

    target->SetTransform(Matrix().PreTranslate(-bounds.x, -bounds.y));
    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
    MOZ_ASSERT(ctx); // already checked the target above

    Manager()->GetPaintedLayerCallback()(this,
                                         ctx,
                                         visibleRegion.ToUnknownRegion(), visibleRegion.ToUnknownRegion(),
                                         DrawRegionClip::DRAW, nsIntRegion(), Manager()->GetPaintedLayerCallbackData());

#if 0
    static int count;
    char buf[400];
    sprintf(buf, "wrout%d.png", count++);
    gfxUtils::WriteAsPNG(target, buf);
#endif
  }
  RefPtr<TextureWrapperImage> image =
    new TextureWrapperImage(texture, IntRect(IntPoint(0, 0), size));
  mImageContainer->SetCurrentImageInTransaction(image);

  if (!mImageClient->UpdateImage(mImageContainer, /* unused */0)) {
    return;
  }

  WRScrollFrameStackingContextGenerator scrollFrames(this);

  // Since we are creating a stacking context below using the visible region of
  // this layer, we need to make sure the image display item has coordinates
  // relative to the visible region.
  Rect rect = RelativeToVisible(IntRectToRect(bounds.ToUnknownRect()));
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("PaintedLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());

  Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  Rect overflow(0, 0, relBounds.width, relBounds.height);
  Matrix4x4 transform;// = GetTransform();

  WRBridge()->AddWebRenderCommand(
      OpPushDLBuilder(toWrRect(relBounds), toWrRect(overflow), transform, FrameMetrics::NULL_SCROLL_ID));
  WRBridge()->AddWebRenderCommand(OpDPPushExternalImageId(toWrRect(rect), toWrRect(clip), Nothing(), mExternalImageId));

  if (gfxPrefs::LayersDump()) printf_stderr("PaintedLayer %p using %s as bounds/overflow, %s for transform\n", this, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->AddWebRenderCommand(OpPopDLBuilder());
}

} // namespace layers
} // namespace mozilla
