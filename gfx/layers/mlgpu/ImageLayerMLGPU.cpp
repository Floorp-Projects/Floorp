/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLayerMLGPU.h"
#include "LayerManagerMLGPU.h"

namespace mozilla {

using namespace gfx;

namespace layers {

ImageLayerMLGPU::ImageLayerMLGPU(LayerManagerMLGPU* aManager)
  : ImageLayer(aManager, static_cast<HostLayer*>(this))
  , TexturedLayerMLGPU(aManager)
{
}

ImageLayerMLGPU::~ImageLayerMLGPU()
{
  CleanupResources();
}

void
ImageLayerMLGPU::ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
{
  Matrix4x4 local = GetLocalTransform();

  // Snap image edges to pixel boundaries.
  gfxRect sourceRect(0, 0, 0, 0);
  if (mHost && mHost->IsAttached()) {
    IntSize size = mHost->GetImageSize();
    sourceRect.SizeTo(size.width, size.height);
  }

  // Snap our local transform first, and snap the inherited transform as well.
  // This makes our snapping equivalent to what would happen if our content
  // was drawn into a PaintedLayer (gfxContext would snap using the local
  // transform, then we'd snap again when compositing the PaintedLayer).
  mEffectiveTransform =
      SnapTransform(local, sourceRect, nullptr) *
      SnapTransformTranslation(aTransformToSurface, nullptr);
  mEffectiveTransformForBuffer = mEffectiveTransform;

  if (mScaleMode == ScaleMode::STRETCH &&
      mScaleToSize.width != 0.0 &&
      mScaleToSize.height != 0.0)
  {
    Size scale(sourceRect.Width() / mScaleToSize.width,
               sourceRect.Height() / mScaleToSize.height);
    mScale = Some(scale);
  }

  ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
}

gfx::SamplingFilter
ImageLayerMLGPU::GetSamplingFilter()
{
  return ImageLayer::GetSamplingFilter();
}

bool
ImageLayerMLGPU::IsContentOpaque()
{
  if (mPictureRect.Width() == 0 || mPictureRect.Height() == 0) {
    return false;
  }
  if (mScaleMode == ScaleMode::STRETCH) {
    return gfx::IsOpaque(mHost->CurrentTextureHost()->GetFormat());
  }
  return false;
}

void
ImageLayerMLGPU::SetRenderRegion(LayerIntRegion&& aRegion)
{
  switch (mScaleMode) {
  case ScaleMode::STRETCH:
    // See bug 1264142.
    aRegion.AndWith(LayerIntRect(0, 0, mScaleToSize.width, mScaleToSize.height));
    break;
  default:
    // Clamp the visible region to the texture size. (see bug 1396507)
    MOZ_ASSERT(mScaleMode == ScaleMode::SCALE_NONE);
    aRegion.AndWith(LayerIntRect(0, 0, mPictureRect.Width(), mPictureRect.Height()));
    break;
  }
  LayerMLGPU::SetRenderRegion(std::move(aRegion));
}

void
ImageLayerMLGPU::CleanupResources()
{
  if (mHost) {
    mHost->CleanupResources();
    mHost->Detach(this);
  }
  mTexture = nullptr;
  mBigImageTexture = nullptr;
  mHost = nullptr;
}

void
ImageLayerMLGPU::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  ImageLayer::PrintInfo(aStream, aPrefix);
  if (mHost && mHost->IsAttached()) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mHost->PrintInfo(aStream, pfx.get());
  }
}

void
ImageLayerMLGPU::Disconnect()
{
  CleanupResources();
}

void
ImageLayerMLGPU::ClearCachedResources()
{
  CleanupResources();
}

} // namespace layers
} // namespace mozilla
