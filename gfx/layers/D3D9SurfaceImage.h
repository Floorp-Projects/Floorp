/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

class D3D9RecycleAllocator : public TextureClientRecycleAllocator {
 public:
  D3D9RecycleAllocator(KnowsCompositor* aAllocator, IDirect3DDevice9* aDevice)
      : TextureClientRecycleAllocator(aAllocator), mDevice(aDevice) {}

  already_AddRefed<TextureClient> CreateOrRecycleClient(
      gfx::SurfaceFormat aFormat, const gfx::IntSize& aSize);

 protected:
  already_AddRefed<TextureClient> Allocate(
      gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
      TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) override;

  RefPtr<IDirect3DDevice9> mDevice;
};

/**
 * Wraps a D3D9 texture, shared with the compositor though DXGI.
 * At the moment it is only used with D3D11 compositing, and the corresponding
 * TextureHost is DXGITextureHostD3D11.
 */
class DXGID3D9TextureData : public TextureData {
 public:
  static DXGID3D9TextureData* Create(gfx::IntSize aSize,
                                     gfx::SurfaceFormat aFormat,
                                     TextureFlags aFlags,
                                     IDirect3DDevice9* aDevice);

  virtual ~DXGID3D9TextureData();

  void FillInfo(TextureData::Info& aInfo) const override;

  bool Lock(OpenMode) override { return true; }

  void Unlock() override {}

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  void Deallocate(LayersIPCChannel* aAllocator) override {}

  IDirect3DDevice9* GetD3D9Device() { return mDevice; }
  IDirect3DTexture9* GetD3D9Texture() { return mTexture; }
  HANDLE GetShareHandle() const { return mHandle; }
  already_AddRefed<IDirect3DSurface9> GetD3D9Surface() const;

  const D3DSURFACE_DESC& GetDesc() const { return mDesc; }

  gfx::IntSize GetSize() const {
    return gfx::IntSize(mDesc.Width, mDesc.Height);
  }

 protected:
  DXGID3D9TextureData(gfx::SurfaceFormat aFormat, IDirect3DTexture9* aTexture,
                      HANDLE aHandle, IDirect3DDevice9* aDevice);

  RefPtr<IDirect3DDevice9> mDevice;
  RefPtr<IDirect3DTexture9> mTexture;
  gfx::SurfaceFormat mFormat;
  HANDLE mHandle;
  D3DSURFACE_DESC mDesc;
};

// Image class that wraps a IDirect3DSurface9. This class copies the image
// passed into SetData(), so that it can be accessed from other D3D devices.
// This class also manages the synchronization of the copy, to ensure the
// resource is ready to use.
class D3D9SurfaceImage : public Image {
 public:
  D3D9SurfaceImage();
  virtual ~D3D9SurfaceImage();

  HRESULT AllocateAndCopy(D3D9RecycleAllocator* aAllocator,
                          IDirect3DSurface9* aSurface,
                          const gfx::IntRect& aRegion);

  // Returns the description of the shared surface.
  const D3DSURFACE_DESC& GetDesc() const;

  gfx::IntSize GetSize() const override;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;

  already_AddRefed<IDirect3DSurface9> GetD3D9Surface() const;

  HANDLE GetShareHandle() const;

  bool IsValid() const override { return mValid; }

  void Invalidate() { mValid = false; }

 private:
  gfx::IntSize mSize;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<IDirect3DTexture9> mTexture;
  HANDLE mShareHandle;
  D3DSURFACE_DESC mDesc;
  bool mValid;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_D3DSURFACEIMAGE_H
