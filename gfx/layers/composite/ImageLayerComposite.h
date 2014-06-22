/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ImageLayerComposite_H
#define GFX_ImageLayerComposite_H

#include "GLTextureImage.h"             // for TextureImage
#include "ImageLayers.h"                // for ImageLayer
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "nsISupportsImpl.h"            // for TextureImage::AddRef, etc
#include "nscore.h"                     // for nsACString

struct nsIntPoint;
struct nsIntRect;

namespace mozilla {
namespace layers {

class CompositableHost;
class ImageHost;
class Layer;

class ImageLayerComposite : public ImageLayer,
                            public LayerComposite
{
  typedef gl::TextureImage TextureImage;

public:
  ImageLayerComposite(LayerManagerComposite* aManager);

  virtual ~ImageLayerComposite();

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  virtual void Disconnect() MOZ_OVERRIDE;

  virtual bool SetCompositableHost(CompositableHost* aHost) MOZ_OVERRIDE;

  virtual Layer* GetLayer() MOZ_OVERRIDE;

  virtual void RenderLayer(const nsIntRect& aClipRect);

  virtual void ComputeEffectiveTransforms(const mozilla::gfx::Matrix4x4& aTransformToSurface) MOZ_OVERRIDE;

  virtual void CleanupResources() MOZ_OVERRIDE;

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE;

  virtual void GenEffectChain(EffectChain& aEffect) MOZ_OVERRIDE;

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

  virtual const char* Name() const { return "ImageLayerComposite"; }

protected:
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) MOZ_OVERRIDE;

private:
  gfx::Filter GetEffectFilter();

private:
  RefPtr<CompositableHost> mImageHost;
};

} /* layers */
} /* mozilla */

#endif /* GFX_ImageLayerComposite_H */
