/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
: Layer(aManager, aImplData), mFilter(GraphicsFilter::FILTER_GOOD)
, mScaleMode(ScaleMode::SCALE_NONE), mDisallowBigImage(false)
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
    sourceRect.SizeTo(gfx::ThebesIntSize(mContainer->GetCurrentSize()));
    if (mScaleMode != ScaleMode::SCALE_NONE &&
        sourceRect.width != 0.0 && sourceRect.height != 0.0) {
      NS_ASSERTION(mScaleMode == ScaleMode::STRETCH,
                   "No other scalemodes than stretch and none supported yet.");
      local.PreScale(mScaleToSize.width / sourceRect.width,
                     mScaleToSize.height / sourceRect.height, 1.0);
    }
  }
  // Snap our local transform first, and snap the inherited transform as well.
  // This makes our snapping equivalent to what would happen if our content
  // was drawn into a PaintedLayer (gfxContext would snap using the local
  // transform, then we'd snap again when compositing the PaintedLayer).
  mEffectiveTransform =
      SnapTransform(local, sourceRect, nullptr) *
      SnapTransformTranslation(aTransformToSurface, nullptr);
  ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
}

}
}
