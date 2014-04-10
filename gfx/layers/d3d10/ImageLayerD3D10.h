/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_IMAGELAYERD3D10_H
#define GFX_IMAGELAYERD3D10_H

#include "LayerManagerD3D10.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

class ImageLayerD3D10 : public ImageLayer,
                        public LayerD3D10
{
public:
  ImageLayerD3D10(LayerManagerD3D10 *aManager)
    : ImageLayer(aManager, nullptr)
    , LayerD3D10(aManager)
  {
    mImplData = static_cast<LayerD3D10*>(this);
  }

  // LayerD3D10 Implementation
  virtual Layer* GetLayer();

  virtual void RenderLayer();

  void AllocateTexturesYCbCr(PlanarYCbCrImage *aImage);

  virtual already_AddRefed<ID3D10ShaderResourceView> GetAsTexture(gfx::IntSize* aSize);

private:
 ID3D10ShaderResourceView* GetImageSRView(Image* aImage, bool& aHasAlpha, IDXGIKeyedMutex **aMutex = nullptr);
};

struct PlanarYCbCrD3D10BackendData : public ImageBackendData
{
  nsRefPtr<ID3D10Texture2D> mYTexture;
  nsRefPtr<ID3D10Texture2D> mCrTexture;
  nsRefPtr<ID3D10Texture2D> mCbTexture;
  nsRefPtr<ID3D10ShaderResourceView> mYView;
  nsRefPtr<ID3D10ShaderResourceView> mCbView;
  nsRefPtr<ID3D10ShaderResourceView> mCrView;
};

struct TextureD3D10BackendData : public ImageBackendData
{
  nsRefPtr<ID3D10Texture2D> mTexture;
  nsRefPtr<ID3D10ShaderResourceView> mSRView;
};

class RemoteDXGITextureImage : public Image {
public:
  RemoteDXGITextureImage() : Image(nullptr, ImageFormat::REMOTE_IMAGE_DXGI_TEXTURE) {}

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() MOZ_OVERRIDE;

  mozilla::gfx::IntSize GetSize() { return mSize; }

  TextureD3D10BackendData *GetD3D10TextureBackendData(ID3D10Device *aDevice);

  mozilla::gfx::IntSize mSize;
  RemoteImageData::Format mFormat;
  HANDLE mHandle;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYERD3D10_H */
