/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

using namespace gfx;

WebRenderImageLayer::WebRenderImageLayer(WebRenderLayerManager* aLayerManager)
  : ImageLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  , mImageClientContainerType(CompositableType::UNKNOWN)
{
  MOZ_COUNT_CTOR(WebRenderImageLayer);
}

WebRenderImageLayer::~WebRenderImageLayer()
{
  MOZ_COUNT_DTOR(WebRenderImageLayer);

  if (mKey.isSome()) {
    WrManager()->AddImageKeyForDiscard(mKey.value());
  }

  if (mExternalImageId.isSome()) {
    WrBridge()->DeallocExternalImageId(mExternalImageId.ref());
  }
  if (mPipelineId.isSome()) {
    WrBridge()->RemovePipelineIdForAsyncCompositable(mPipelineId.ref());
  }
}

CompositableType
WebRenderImageLayer::GetImageClientType()
{
  if (mImageClientContainerType != CompositableType::UNKNOWN) {
    return mImageClientContainerType;
  }

  if (mContainer->IsAsync()) {
    mImageClientContainerType = CompositableType::IMAGE_BRIDGE;
    return mImageClientContainerType;
  }

  AutoLockImage autoLock(mContainer);

  mImageClientContainerType = autoLock.HasImage()
    ? CompositableType::IMAGE : CompositableType::UNKNOWN;
  return mImageClientContainerType;
}

already_AddRefed<gfx::SourceSurface>
WebRenderImageLayer::GetAsSourceSurface()
{
  if (!mContainer) {
    return nullptr;
  }
  AutoLockImage autoLock(mContainer);
  Image *image = autoLock.GetImage();
  if (!image) {
    return nullptr;
  }
  RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
  if (!surface || !surface->IsValid()) {
    return nullptr;
  }
  return surface.forget();
}

void
WebRenderImageLayer::ClearCachedResources()
{
  if (mImageClient) {
    mImageClient->ClearCachedResources();
  }
}

bool
WebRenderImageLayer::SupportsAsyncUpdate()
{
  if (GetImageClientType() == CompositableType::IMAGE_BRIDGE &&
      mPipelineId.isSome()) {
    return true;
  }
  return false;
}

void
WebRenderImageLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                 const StackingContextHelper& aSc)
{
  if (!mContainer) {
     return;
  }

  CompositableType type = GetImageClientType();
  if (type == CompositableType::UNKNOWN) {
    return;
  }

  MOZ_ASSERT(GetImageClientType() != CompositableType::UNKNOWN);

  if (GetImageClientType() == CompositableType::IMAGE && !mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return;
    }
    mImageClient->Connect();
  }

  if (GetImageClientType() == CompositableType::IMAGE_BRIDGE && mPipelineId.isNothing()) {
    MOZ_ASSERT(!mImageClient);
    // Alloc async image pipeline id.
    mPipelineId = Some(WrBridge()->GetCompositorBridgeChild()->GetNextPipelineId());
    WrBridge()->AddPipelineIdForAsyncCompositable(mPipelineId.ref(),
                                                  mContainer->GetAsyncContainerHandle());
  } else if (GetImageClientType() == CompositableType::IMAGE && mExternalImageId.isNothing())  {
    MOZ_ASSERT(mImageClient);
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
    MOZ_ASSERT(mExternalImageId.isSome());
  }

  if (GetImageClientType() == CompositableType::IMAGE_BRIDGE) {
    MOZ_ASSERT(!mImageClient);
    MOZ_ASSERT(mExternalImageId.isNothing());

    // Push IFrame for async image pipeline.

    ParentLayerRect bounds = GetLocalTransformTyped().TransformBounds(Bounds());

    // As with WebRenderTextLayer, because we don't push a stacking context for
    // this async image pipeline, WR doesn't know about the transform on this layer.
    // Therefore we need to apply that transform to the bounds before we pass it on to WR.
    // The conversion from ParentLayerPixel to LayerPixel below is a result of
    // changing the reference layer from "this layer" to the "the layer that
    // created aSc".
    LayerRect rect = ViewAs<LayerPixel>(bounds,
        PixelCastJustification::MovingDownToChildren);
    DumpLayerInfo("Image Layer async", rect);

    // XXX Remove IFrame for async image pipeline when partial display list update is supported.
    WrClipRegionToken clipRegion = aBuilder.PushClipRegion(aSc.ToRelativeWrRect(rect));
    aBuilder.PushIFrame(aSc.ToRelativeWrRect(rect), clipRegion, mPipelineId.ref());

    // Prepare data that are necessary for async image pipelin.
    // They are used within WebRenderCompositableHolder

    gfx::Matrix4x4 scTransform = GetTransform();
    // Translate is applied as part of PushIFrame()
    scTransform.PostTranslate(-rect.x, -rect.y, 0);
    // Adjust transform as to apply origin
    LayerPoint scOrigin = Bounds().TopLeft();
    scTransform.PreTranslate(-scOrigin.x, -scOrigin.y, 0);

    MaybeIntSize scaleToSize;
    if (mScaleMode != ScaleMode::SCALE_NONE) {
      NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                   "No other scalemodes than stretch and none supported yet.");
      scaleToSize = Some(mScaleToSize);
    }
    LayerRect scBounds = BoundsForStackingContext();
    wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);
    wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

    StackingContextHelper sc(aSc, aBuilder, this);
    Maybe<WrImageMask> mask = BuildWrMaskLayer(&sc);

    WrBridge()->AddWebRenderParentCommand(OpUpdateAsyncImagePipeline(mPipelineId.value(),
                                                                     scBounds,
                                                                     scTransform,
                                                                     scaleToSize,
                                                                     ClipRect(),
                                                                     mask,
                                                                     filter,
                                                                     mixBlendMode));
    return;
  }

  MOZ_ASSERT(GetImageClientType() == CompositableType::IMAGE);
  MOZ_ASSERT(mImageClient->AsImageClientSingle());

  AutoLockImage autoLock(mContainer);
  Image* image = autoLock.GetImage();
  if (!image) {
    return;
  }
  gfx::IntSize size = image->GetSize();
  mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                        mContainer,
                        mKey,
                        mExternalImageId.ref());
  if (mKey.isNothing()) {
    return;
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);
  StackingContextHelper sc(aSc, aBuilder, this);

  LayerRect rect(0, 0, size.width, size.height);
  if (mScaleMode != ScaleMode::SCALE_NONE) {
    NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                 "No other scalemodes than stretch and none supported yet.");
    rect = LayerRect(0, 0, mScaleToSize.width, mScaleToSize.height);
  }

  WrClipRegionToken clip = aBuilder.PushClipRegion(
      sc.ToRelativeWrRect(rect));
  wr::ImageRendering filter = wr::ToImageRendering(mSamplingFilter);

  DumpLayerInfo("Image Layer", rect);
  if (gfxPrefs::LayersDump()) {
    printf_stderr("ImageLayer %p texture-filter=%s \n",
                  GetLayer(),
                  Stringify(filter).c_str());
  }
  aBuilder.PushImage(sc.ToRelativeWrRect(rect), clip, filter, mKey.value());
}

Maybe<WrImageMask>
WebRenderImageLayer::RenderMaskLayer(const gfx::Matrix4x4& aTransform)
{
  if (!mContainer) {
     return Nothing();
  }

  CompositableType type = GetImageClientType();
  if (type == CompositableType::UNKNOWN) {
    return Nothing();
  }

  MOZ_ASSERT(GetImageClientType() == CompositableType::IMAGE);
  if (GetImageClientType() != CompositableType::IMAGE) {
    return Nothing();
  }

  if (!mImageClient) {
    mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                  WrBridge(),
                                                  TextureFlags::DEFAULT);
    if (!mImageClient) {
      return Nothing();
    }
    mImageClient->Connect();
  }

  if (mExternalImageId.isNothing()) {
    mExternalImageId = Some(WrBridge()->AllocExternalImageIdForCompositable(mImageClient));
  }

  AutoLockImage autoLock(mContainer);
  Image* image = autoLock.GetImage();
  if (!image) {
    return Nothing();
  }

  MOZ_ASSERT(mImageClient->AsImageClientSingle());
  mKey = UpdateImageKey(mImageClient->AsImageClientSingle(),
                        mContainer,
                        mKey,
                        mExternalImageId.ref());
  if (mKey.isNothing()) {
    return Nothing();
  }

  gfx::IntSize size = image->GetSize();
  WrImageMask imageMask;
  imageMask.image = mKey.value();
  Rect maskRect = aTransform.TransformBounds(Rect(0, 0, size.width, size.height));
  imageMask.rect = wr::ToWrRect(maskRect);
  imageMask.repeat = false;
  return Some(imageMask);
}

} // namespace layers
} // namespace mozilla
