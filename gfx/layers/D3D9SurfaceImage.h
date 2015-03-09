/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D3DSURFACEIMAGE_H
#define GFX_D3DSURFACEIMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "nsAutoPtr.h"
#include "d3d9.h"

namespace mozilla {
namespace layers {

// Image class that wraps a IDirect3DSurface9. This class copies the image
// passed into SetData(), so that it can be accessed from other D3D devices.
// This class also manages the synchronization of the copy, to ensure the
// resource is ready to use.
class D3D9SurfaceImage : public Image
                       , public ISharedImage {
public:

  struct Data {
    Data(IDirect3DSurface9* aSurface, const nsIntRect& aRegion)
      : mSurface(aSurface), mRegion(aRegion) {}
    RefPtr<IDirect3DSurface9> mSurface;
    nsIntRect mRegion;
  };

  D3D9SurfaceImage();
  virtual ~D3D9SurfaceImage();

  virtual ISharedImage* AsSharedImage() MOZ_OVERRIDE { return this; }

  // Copies the surface into a sharable texture's surface, and initializes
  // the image.
  HRESULT SetData(const Data& aData);

  // Returns the description of the shared surface.
  const D3DSURFACE_DESC& GetDesc() const;

  // Returns the HANDLE that can be used to open the image as a shared resource.
  // If the operation to copy the original resource to the shared resource
  // hasn't finished yet, this function blocks until the synchronization is
  // complete.
  HANDLE GetShareHandle();

  gfx::IntSize GetSize() MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() MOZ_OVERRIDE;

  virtual TextureClient* GetTextureClient(CompositableClient* aClient) MOZ_OVERRIDE;
  virtual uint8_t* GetBuffer() MOZ_OVERRIDE { return nullptr; }

private:

  // Blocks the calling thread until the copy operation started in SetData()
  // is complete, whereupon the texture is safe to use.
  void EnsureSynchronized();

  gfx::IntSize mSize;
  RefPtr<IDirect3DTexture9> mTexture;
  RefPtr<IDirect3DQuery9> mQuery;
  RefPtr<TextureClient> mTextureClient;
  HANDLE mShareHandle;
  D3DSURFACE_DESC mDesc;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_D3DSURFACEIMAGE_H
