/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexturedLayerMLGPU.h"
#include "LayerManagerMLGPU.h"
#include "RenderViewMLGPU.h"
#include "FrameBuilder.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/ImageHost.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

using namespace gfx;

TexturedLayerMLGPU::TexturedLayerMLGPU(LayerManagerMLGPU* aManager)
 : LayerMLGPU(aManager)
{
}

TexturedLayerMLGPU::~TexturedLayerMLGPU()
{
  // Note: we have to cleanup resources in derived classes, since we can't
  // easily tell in our destructor if we have a TempImageLayerMLGPU, which
  // should not have its compositable detached, and we can't call GetLayer
  // here.
}

bool
TexturedLayerMLGPU::SetCompositableHost(CompositableHost* aHost)
{
  switch (aHost->GetType()) {
    case CompositableType::IMAGE:
      mHost = aHost->AsImageHost();
      return true;
    default:
      return false;
  }
}

CompositableHost*
TexturedLayerMLGPU::GetCompositableHost()
{
  if (mHost && mHost->IsAttached()) {
    return mHost.get();
  }
  return nullptr;
}

RefPtr<TextureSource>
TexturedLayerMLGPU::BindAndGetTexture()
{
  if (!mHost) {
    return nullptr;
  }

  LayerManagerMLGPU* lm = GetLayerManager()->AsLayerManagerMLGPU();

  // Note: we don't call FinishRendering since mask layers do not need
  // composite notifications or bias updates. (This function should
  // not be called for non-mask-layers).
  ImageHost::RenderInfo info;
  if (!mHost->PrepareToRender(lm->GetTextureSourceProvider(), &info)) {
    return nullptr;
  }

  RefPtr<TextureSource> source = mHost->AcquireTextureSource(info);
  if (!source) {
    return nullptr;
  }

  mTexture = source;
  return source;
}

bool
TexturedLayerMLGPU::OnPrepareToRender(FrameBuilder* aBuilder)
{
  if (!mHost) {
    return false;
  }

  LayerManagerMLGPU* lm = GetLayerManager()->AsLayerManagerMLGPU();

  ImageHost::RenderInfo info;
  if (!mHost->PrepareToRender(lm->GetTextureSourceProvider(), &info)) {
    return false;
  }

  RefPtr<TextureSource> source = mHost->AcquireTextureSource(info);
  if (!source) {
    return false;
  }

  if (source->AsBigImageIterator()) {
    mBigImageTexture = source;
    mTexture = nullptr;
  } else {
    mTexture = source;
  }

  mPictureRect = IntRect(0, 0, info.img->mPictureRect.width, info.img->mPictureRect.height);

  mHost->FinishRendering(info);
  return true;
}

void
TexturedLayerMLGPU::AssignToView(FrameBuilder* aBuilder,
                                 RenderViewMLGPU* aView,
                                 Maybe<Polygon>&& aGeometry)
{
  if (mBigImageTexture) {
    BigImageIterator* iter = mBigImageTexture->AsBigImageIterator();
    iter->BeginBigImageIteration();
    AssignBigImage(aBuilder, aView, iter, aGeometry);
    iter->EndBigImageIteration();
  } else {
    LayerMLGPU::AssignToView(aBuilder, aView, Move(aGeometry));
  }
}

void
TexturedLayerMLGPU::AssignBigImage(FrameBuilder* aBuilder,
                                   RenderViewMLGPU* aView,
                                   BigImageIterator* aIter,
                                   const Maybe<Polygon>& aGeometry)
{
  const Matrix4x4& transform = GetLayer()->GetEffectiveTransformForBuffer();

  // Note that we don't need to assign these in any particular order, since
  // they do not overlap.
  do {
    IntRect tileRect = aIter->GetTileRect();
    IntRect rect = tileRect.Intersect(mPictureRect);
    if (rect.IsEmpty()) {
      continue;
    }

    {
      Rect screenRect = transform.TransformBounds(Rect(rect));
      screenRect.MoveBy(-aView->GetTargetOffset());
      screenRect = screenRect.Intersect(Rect(mComputedClipRect.ToUnknownRect()));
      if (screenRect.IsEmpty()) {
        // This tile is not in the clip region, so skip it.
        continue;
      }
    }

    RefPtr<TextureSource> tile = mBigImageTexture->ExtractCurrentTile();
    if (!tile) {
      continue;
    }

    // Create a temporary item.
    RefPtr<TempImageLayerMLGPU> item = new TempImageLayerMLGPU(aBuilder->GetManager());
    item->Init(this, tile, rect);

    Maybe<Polygon> geometry = aGeometry;
    item->AddBoundsToView(aBuilder, aView, Move(geometry));

    // Since the layer tree is not holding this alive, we have to ask the
    // FrameBuilder to do it for us.
    aBuilder->RetainTemporaryLayer(item);
  } while (aIter->NextTile());
}

TempImageLayerMLGPU::TempImageLayerMLGPU(LayerManagerMLGPU* aManager)
 : ImageLayer(aManager, static_cast<HostLayer*>(this)),
   TexturedLayerMLGPU(aManager)
{
}

TempImageLayerMLGPU::~TempImageLayerMLGPU()
{
}

void
TempImageLayerMLGPU::Init(TexturedLayerMLGPU* aSource,
                          const RefPtr<TextureSource>& aTexture,
                          const gfx::IntRect& aPictureRect)
{
  // ImageLayer properties.
  mEffectiveTransform = aSource->GetLayer()->GetEffectiveTransform();
  mEffectiveTransformForBuffer = aSource->GetLayer()->GetEffectiveTransformForBuffer();

  // Base LayerMLGPU properties.
  mComputedClipRect = aSource->GetComputedClipRect();
  mMask = aSource->GetMask();
  mComputedOpacity = aSource->GetComputedOpacity();

  // TexturedLayerMLGPU properties.
  mHost = aSource->GetImageHost();
  mTexture = aTexture;
  mPictureRect = aPictureRect;

  // Local properties.
  mFilter = aSource->GetSamplingFilter();
  mShadowVisibleRegion = aSource->GetShadowVisibleRegion();
  mIsOpaque = aSource->IsContentOpaque();

  // Set this layer to prepared so IsPrepared() assertions don't fire.
  MarkPrepared();
}

} // namespace layers
} // namespace mozilla
