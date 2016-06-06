/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ImageLayerComposite_H
#define GFX_ImageLayerComposite_H

#include "GLTextureImage.h"             // for TextureImage
#include "ImageLayers.h"                // for ImageLayer
#include "mozilla/Attributes.h"         // for override
#include "mozilla/gfx/Rect.h"
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "nsISupportsImpl.h"            // for TextureImage::AddRef, etc
#include "nscore.h"                     // for nsACString
#include "CompositableHost.h"           // for CompositableHost

namespace mozilla {
namespace layers {

class ImageHost;
class Layer;

class ImageLayerComposite : public ImageLayer,
                            public LayerComposite
{
  typedef gl::TextureImage TextureImage;

public:
  explicit ImageLayerComposite(LayerManagerComposite* aManager);

protected:
  virtual ~ImageLayerComposite();

public:
  virtual LayerRenderState GetRenderState() override;

  virtual void Disconnect() override;

  virtual bool SetCompositableHost(CompositableHost* aHost) override;

  virtual Layer* GetLayer() override;

  virtual void SetLayerManager(LayerManagerComposite* aManager) override;

  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;

  virtual void ComputeEffectiveTransforms(const mozilla::gfx::Matrix4x4& aTransformToSurface) override;

  virtual void CleanupResources() override;

  CompositableHost* GetCompositableHost() override;

  virtual void GenEffectChain(EffectChain& aEffect) override;

  virtual LayerComposite* AsLayerComposite() override { return this; }

  virtual const char* Name() const override { return "ImageLayerComposite"; }

  virtual bool IsOpaque() override;

  virtual nsIntRegion GetFullyRenderedRegion() override;

protected:
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

private:
  gfx::SamplingFilter GetSamplingFilter();

private:
  RefPtr<ImageHost> mImageHost;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_ImageLayerComposite_H */
