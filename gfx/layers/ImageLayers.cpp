/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLayers.h"
#include "ImageContainer.h"

namespace mozilla {
namespace layers {

ImageLayer::ImageLayer(LayerManager* aManager, void* aImplData)
: Layer(aManager, aImplData), mFilter(gfxPattern::FILTER_GOOD)
, mScaleMode(SCALE_NONE), mForceSingleTile(false) 
{}

ImageLayer::~ImageLayer()
{}

void ImageLayer::SetContainer(ImageContainer* aContainer) 
{
  mContainer = aContainer;
}

void ImageLayer::ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
{
  // Snap image edges to pixel boundaries
  gfxRect snap(0, 0, 0, 0);
  if (mContainer) {
    gfxIntSize size = mContainer->GetCurrentSize();
    snap.SizeTo(gfxSize(size.width, size.height));
  }
  // Snap our local transform first, and snap the inherited transform as well.
  // This makes our snapping equivalent to what would happen if our content
  // was drawn into a ThebesLayer (gfxContext would snap using the local
  // transform, then we'd snap again when compositing the ThebesLayer).
  mEffectiveTransform =
      SnapTransform(GetLocalTransform(), snap, nullptr)*
      SnapTransform(aTransformToSurface, gfxRect(0, 0, 0, 0), nullptr);
  ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
}

}
}
