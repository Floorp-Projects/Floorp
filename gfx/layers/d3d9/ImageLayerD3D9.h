/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYERD3D9_H
#define GFX_IMAGELAYERD3D9_H

#include "LayerManagerD3D9.h"
#include "ImageLayers.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

class ShadowBufferD3D9;

class THEBES_API ImageLayerD3D9 : public ImageLayer,
                                  public LayerD3D9
{
public:
  ImageLayerD3D9(LayerManagerD3D9 *aManager)
    : ImageLayer(aManager, NULL)
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

class THEBES_API ImageD3D9
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

class ShadowImageLayerD3D9 : public ShadowImageLayer,
                            public LayerD3D9
{
public:
  ShadowImageLayerD3D9(LayerManagerD3D9* aManager);
  virtual ~ShadowImageLayerD3D9();

  // ShadowImageLayer impl
  virtual void Swap(const SharedImage& aFront,
                    SharedImage* aNewBack);

  virtual void Disconnect();

  // LayerD3D9 impl
  virtual void Destroy();

  virtual Layer* GetLayer();

  virtual void RenderLayer();

  virtual already_AddRefed<IDirect3DTexture9> GetAsTexture(gfxIntSize* aSize);

private:
  nsRefPtr<ShadowBufferD3D9> mBuffer;
  nsRefPtr<PlanarYCbCrImage> mYCbCrImage;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYERD3D9_H */
