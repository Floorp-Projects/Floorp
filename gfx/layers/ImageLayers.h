/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYER_H
#define GFX_IMAGELAYER_H

#include "Layers.h"                     // for Layer, etc
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {

class ImageContainer;

/**
 * A Layer which renders an Image.
 */
class ImageLayer : public Layer {
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ImageContainer. aContainer must have the same layer manager
   * as this layer.
   */
  virtual void SetContainer(ImageContainer* aContainer);

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the filter used to resample this image if necessary.
   */
  void SetFilter(GraphicsFilter aFilter)
  {
    if (mFilter != aFilter) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Filter", this));
      mFilter = aFilter;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the size to scale the image to and the mode at which to scale.
   */
  void SetScaleToSize(const gfx::IntSize &aSize, ScaleMode aMode)
  {
    if (mScaleToSize != aSize || mScaleMode != aMode) {
      mScaleToSize = aSize;
      mScaleMode = aMode;
      Mutated();
    }
  }


  ImageContainer* GetContainer() { return mContainer; }
  GraphicsFilter GetFilter() { return mFilter; }
  const gfx::IntSize& GetScaleToSize() { return mScaleToSize; }
  ScaleMode GetScaleMode() { return mScaleMode; }

  MOZ_LAYER_DECL_NAME("ImageLayer", TYPE_IMAGE)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface);

  /**
   * if true, the image will only be backed by a single tile texture
   */
  void SetDisallowBigImage(bool aDisallowBigImage)
  {
    if (mDisallowBigImage != aDisallowBigImage) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) DisallowBigImage", this));
      mDisallowBigImage = aDisallowBigImage;
      Mutated();
    }
  }

protected:
  ImageLayer(LayerManager* aManager, void* aImplData);
  ~ImageLayer();
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);


  nsRefPtr<ImageContainer> mContainer;
  GraphicsFilter mFilter;
  gfx::IntSize mScaleToSize;
  ScaleMode mScaleMode;
  bool mDisallowBigImage;
};

}
}

#endif /* GFX_IMAGELAYER_H */
