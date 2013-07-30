/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ImageLayerComposite_H
#define GFX_ImageLayerComposite_H

#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/layers/ShadowLayers.h"

#include "mozilla/layers/LayerManagerComposite.h"
#include "ImageLayers.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

class ImageHost;

class ImageLayerComposite : public ImageLayer,
                            public LayerComposite
{
  typedef gl::TextureImage TextureImage;

public:
  ImageLayerComposite(LayerManagerComposite* aManager);

  virtual ~ImageLayerComposite();

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  virtual void Disconnect() MOZ_OVERRIDE;

  virtual void SetCompositableHost(CompositableHost* aHost) MOZ_OVERRIDE;

  virtual Layer* GetLayer() MOZ_OVERRIDE;

  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect);

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) MOZ_OVERRIDE;

  virtual void CleanupResources() MOZ_OVERRIDE;

  CompositableHost* GetCompositableHost() MOZ_OVERRIDE;

  virtual LayerComposite* AsLayerComposite() MOZ_OVERRIDE { return this; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "ImageLayerComposite"; }

protected:
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix) MOZ_OVERRIDE;
#endif

private:
  RefPtr<CompositableHost> mImageHost;
};

} /* layers */
} /* mozilla */

#endif /* GFX_ImageLayerComposite_H */
