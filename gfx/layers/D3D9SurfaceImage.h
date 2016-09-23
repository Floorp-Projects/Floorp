/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D3DSURFACEIMAGE_H
#define GFX_D3DSURFACEIMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "d3d9.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"

namespace mozilla {
namespace layers {

class TextureClient;

class D3D9RecycleAllocator : public TextureClientRecycleAllocator
{
public:
  explicit D3D9RecycleAllocator(TextureForwarder* aAllocator,
                                IDirect3DDevice9* aDevice)
    : TextureClientRecycleAllocator(aAllocator)
    , mDevice(aDevice)
  {}

  already_AddRefed<TextureClient>
  CreateOrRecycleClient(gfx::SurfaceFormat aFormat,
                        const gfx::IntSize& aSize);

protected:
  virtual already_AddRefed<TextureClient>
  Allocate(gfx::SurfaceFormat aFormat,
           gfx::IntSize aSize,
           BackendSelector aSelector,
           TextureFlags aTextureFlags,
           TextureAllocationFlags aAllocFlags) override;

  RefPtr<IDirect3DDevice9> mDevice;
};

// Image class that wraps a IDirect3DSurface9. This class copies the image
// passed into SetData(), so that it can be accessed from other D3D devices.
// This class also manages the synchronization of the copy, to ensure the
// resource is ready to use.
class D3D9SurfaceImage : public Image {
public:
  explicit D3D9SurfaceImage();
  virtual ~D3D9SurfaceImage();

  HRESULT AllocateAndCopy(D3D9RecycleAllocator* aAllocator,
                          IDirect3DSurface9* aSurface,
                          const gfx::IntRect& aRegion);

  // Returns the description of the shared surface.
  const D3DSURFACE_DESC& GetDesc() const;

  gfx::IntSize GetSize() override;

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  virtual TextureClient* GetTextureClient(TextureForwarder* aForwarder) override;

  already_AddRefed<IDirect3DSurface9> GetD3D9Surface();

  HANDLE GetShareHandle() const;

  virtual bool IsValid() override { return mValid; }

  void Invalidate() { mValid = false; }

private:

  gfx::IntSize mSize;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<IDirect3DTexture9> mTexture;
  HANDLE mShareHandle;
  D3DSURFACE_DESC mDesc;
  bool mValid;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_D3DSURFACEIMAGE_H
