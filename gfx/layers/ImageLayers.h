/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYER_H
#define GFX_IMAGELAYER_H

#include "Layers.h"                     // for Layer, etc
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {

class ImageContainer;

namespace layerscope {
class LayersPacket;
} // namespace layerscope

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
  void SetSamplingFilter(gfx::SamplingFilter aSamplingFilter)
  {
    if (mSamplingFilter != aSamplingFilter) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Filter", this));
      mSamplingFilter = aSamplingFilter;
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
  gfx::SamplingFilter GetSamplingFilter() { return mSamplingFilter; }
  const gfx::IntSize& GetScaleToSize() { return mScaleToSize; }
  ScaleMode GetScaleMode() { return mScaleMode; }

  MOZ_LAYER_DECL_NAME("ImageLayer", TYPE_IMAGE)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override;

  virtual const gfx::Matrix4x4& GetEffectiveTransformForBuffer() const override
  {
    return mEffectiveTransformForBuffer;
  }

protected:
  ImageLayer(LayerManager* aManager, void* aImplData);
  ~ImageLayer();
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;
  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  RefPtr<ImageContainer> mContainer;
  gfx::SamplingFilter mSamplingFilter;
  gfx::IntSize mScaleToSize;
  ScaleMode mScaleMode;
  gfx::Matrix4x4 mEffectiveTransformForBuffer;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_IMAGELAYER_H */
