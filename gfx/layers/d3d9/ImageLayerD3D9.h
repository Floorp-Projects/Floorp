/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_IMAGELAYERD3D9_H
#define GFX_IMAGELAYERD3D9_H

#include "LayerManagerD3D9.h"
#include "ImageLayers.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

class ShadowBufferD3D9;

class THEBES_API ImageContainerD3D9 : public ImageContainer
{
public:
  ImageContainerD3D9(IDirect3DDevice9 *aDevice);
  virtual ~ImageContainerD3D9() {}

  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats);

  virtual void SetCurrentImage(Image* aImage);

  virtual already_AddRefed<Image> GetCurrentImage();

  virtual already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSize);

  virtual gfxIntSize GetCurrentSize();

  virtual bool SetLayerManager(LayerManager *aManager);

  virtual LayerManager::LayersBackend GetBackendType() { return LayerManager::LAYERS_D3D9; }

  IDirect3DDevice9 *device() { return mDevice; }
  void SetDevice(IDirect3DDevice9 *aDevice) { mDevice = aDevice; }

private:
  nsRefPtr<Image> mActiveImage;

  nsRefPtr<IDirect3DDevice9> mDevice;
};

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
};

class THEBES_API ImageD3D9
{
public:
  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;
};

class THEBES_API PlanarYCbCrImageD3D9 : public PlanarYCbCrImage,
                                        public ImageD3D9
{
public:
  PlanarYCbCrImageD3D9();
  ~PlanarYCbCrImageD3D9() {}

  virtual void SetData(const Data &aData);

  /*
   * Upload the data from out mData into our textures. For now we use this to
   * make sure the textures are created and filled on the main thread.
   */
  void AllocateTextures(IDirect3DDevice9 *aDevice);
  /*
   * XXX
   * Free the textures, we call this from the main thread when we're done
   * drawing this frame. We cannot free this from the constructor since it may
   * be destroyed off the main-thread and might not be able to properly clean
   * up its textures
   */
  void FreeTextures();
  bool HasData() { return mHasData; }

  PRUint32 GetDataSize() { return mBuffer ? mBufferSize : 0; }

  virtual already_AddRefed<gfxASurface> GetAsSurface();

  nsAutoArrayPtr<PRUint8> mBuffer;
  PRUint32 mBufferSize;
  LayerManagerD3D9 *mManager;
  Data mData;
  gfxIntSize mSize;
  nsRefPtr<IDirect3DTexture9> mYTexture;
  nsRefPtr<IDirect3DTexture9> mCrTexture;
  nsRefPtr<IDirect3DTexture9> mCbTexture;
  bool mHasData;
};


class THEBES_API CairoImageD3D9 : public CairoImage,
                                  public ImageD3D9
{
public:
  CairoImageD3D9(IDirect3DDevice9 *aDevice)
    : CairoImage(static_cast<ImageD3D9*>(this))
    , mDevice(aDevice)
  { }
  ~CairoImageD3D9();

  virtual void SetData(const Data &aData);

  virtual already_AddRefed<gfxASurface> GetAsSurface();

  IDirect3DDevice9 *device() { return mDevice; }
  void SetDevice(IDirect3DDevice9 *aDevice);

  /**
   * Uploading a texture may fail if the screen is locked. If this happens,
   * we need to save the backing surface and retry when we are asked to paint.
   */
  virtual IDirect3DTexture9* GetOrCreateTexture();
  const gfxIntSize& GetSize() { return mSize; }

  bool HasAlpha() {
    return mCachedSurface->GetContentType() ==
      gfxASurface::CONTENT_COLOR_ALPHA;
  }

private:
  gfxIntSize mSize;
  nsRefPtr<gfxASurface> mCachedSurface;
  nsRefPtr<IDirect3DDevice9> mDevice;
  nsRefPtr<IDirect3DTexture9> mTexture;
  LayerManagerD3D9 *mManager;
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

private:
  nsRefPtr<ShadowBufferD3D9> mBuffer;
  nsRefPtr<PlanarYCbCrImageD3D9> mYCbCrImage;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYERD3D9_H */
