/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYERD3D9_H
#define GFX_IMAGELAYERD3D9_H

#include "LayerManagerD3D9.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

class ImageLayerD3D9 : public ImageLayer,
                       public LayerD3D9
{
public:
  ImageLayerD3D9(LayerManagerD3D9 *aManager)
    : ImageLayer(aManager, nullptr)
    , LayerD3D9(aManager)
  {
    mImplData = static_cast<LayerD3D9*>(this);
  }

  // LayerD3D9 Implementation
  virtual Layer* GetLayer();

  virtual void RenderLayer();

  virtual already_AddRefed<IDirect3DTexture9> GetAsTexture(gfxIntSize* aSize);

private:
  IDirect3DTexture9* GetTexture(Image *aImage, bool& aHasAlpha);
};

class ImageD3D9
{
public:
  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;
};


struct TextureD3D9BackendData : public ImageBackendData
{
  nsRefPtr<IDirect3DTexture9> mTexture;
};

struct PlanarYCbCrD3D9BackendData : public ImageBackendData
{
  nsRefPtr<IDirect3DTexture9> mYTexture;
  nsRefPtr<IDirect3DTexture9> mCrTexture;
  nsRefPtr<IDirect3DTexture9> mCbTexture;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYERD3D9_H */
