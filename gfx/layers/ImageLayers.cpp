/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLayers.h"
#include "ImageContainer.h"             // for ImageContainer
#include "gfxRect.h"                    // for gfxRect
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for ImageContainer::Release, etc
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

ImageLayer::ImageLayer(LayerManager* aManager, void* aImplData)
: Layer(aManager, aImplData), mSamplingFilter(gfx::SamplingFilter::GOOD)
, mScaleMode(ScaleMode::SCALE_NONE)
{}

ImageLayer::~ImageLayer()
{}

void ImageLayer::SetContainer(ImageContainer* aContainer) 
{
  mContainer = aContainer;
}

void ImageLayer::ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
{
  gfx::Matrix4x4 local = GetLocalTransform();

  // Snap image edges to pixel boundaries
  gfxRect sourceRect(0, 0, 0, 0);
  if (mContainer) {
    sourceRect.SizeTo(gfx::SizeDouble(mContainer->GetCurrentSize()));
  }
  // Snap our local transform first, and snap the inherited transform as well.
  // This makes our snapping equivalent to what would happen if our content
  // was drawn into a PaintedLayer (gfxContext would snap using the local
  // transform, then we'd snap again when compositing the PaintedLayer).
  mEffectiveTransform =
      SnapTransform(local, sourceRect, nullptr) *
      SnapTransformTranslation(aTransformToSurface, nullptr);

  if (mScaleMode != ScaleMode::SCALE_NONE && !sourceRect.IsZero()) {
    NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                 "No other scalemodes than stretch and none supported yet.");
    local.PreScale(mScaleToSize.width / sourceRect.Width(),
                   mScaleToSize.height / sourceRect.Height(), 1.0);

    mEffectiveTransformForBuffer =
        SnapTransform(local, sourceRect, nullptr) *
        SnapTransformTranslation(aTransformToSurface, nullptr);
  } else {
    mEffectiveTransformForBuffer = mEffectiveTransform;
  }

  ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
}

} // namespace layers
} // namespace mozilla
