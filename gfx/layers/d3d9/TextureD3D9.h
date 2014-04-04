/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURED3D9_H
#define MOZILLA_GFX_TEXTURED3D9_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/GfxMessageUtils.h"
#include "gfxWindowsPlatform.h"
#include "d3d9.h"
#include <vector>
#include "DeviceManagerD3D9.h"

namespace mozilla {
namespace gfxs {
class DrawTarget;
}
}

namespace mozilla {
namespace layers {

class CompositorD3D9;

class TextureSourceD3D9
{
  friend class DeviceManagerD3D9;

public:
  TextureSourceD3D9()
    : mPreviousHost(nullptr)
    , mNextHost(nullptr)
    , mCreatingDeviceManager(nullptr)
  {}
  virtual ~TextureSourceD3D9();

  virtual IDirect3DTexture9* GetD3D9Texture() { return mTexture; }

  StereoMode GetStereoMode() const { return mStereoMode; };

  // Release all texture memory resources held by the texture host.
  virtual void ReleaseTextureResources()
  {
    mTexture = nullptr;
  }

protected:
  virtual gfx::IntSize GetSize() const { return mSize; }
  void SetSize(const gfx::IntSize& aSize) { mSize = aSize; }

  // Helper methods for creating and copying textures.
  TemporaryRef<IDirect3DTexture9> InitTextures(
    DeviceManagerD3D9* aDeviceManager,
    const gfx::IntSize &aSize,
    _D3DFORMAT aFormat,
    RefPtr<IDirect3DSurface9>& aSurface,
    D3DLOCKED_RECT& aLockedRect);

  TemporaryRef<IDirect3DTexture9> DataToTexture(
    DeviceManagerD3D9* aDeviceManager,
    unsigned char *aData,
    int aStride,
    const gfx::IntSize &aSize,
    _D3DFORMAT aFormat,
    uint32_t aBPP);

  // aTexture should be in SYSTEMMEM, returns a texture in the default
  // pool (that is, in video memory).
  TemporaryRef<IDirect3DTexture9> TextureToTexture(
    DeviceManagerD3D9* aDeviceManager,
    IDirect3DTexture9* aTexture,
    const gfx::IntSize& aSize,
    _D3DFORMAT aFormat);

  TemporaryRef<IDirect3DTexture9> SurfaceToTexture(
    DeviceManagerD3D9* aDeviceManager,
    gfxWindowsSurface* aSurface,
    const gfx::IntSize& aSize,
    _D3DFORMAT aFormat);

  gfx::IntSize mSize;

  // Linked list of all objects holding d3d9 textures.
  TextureSourceD3D9* mPreviousHost;
  TextureSourceD3D9* mNextHost;
  // The device manager that created our textures.
  DeviceManagerD3D9* mCreatingDeviceManager;

  StereoMode mStereoMode;
  RefPtr<IDirect3DTexture9> mTexture;
};

/**
 * A TextureSource that implements the DataTextureSource interface.
 * it can be used without a TextureHost and is able to upload texture data
 * from a gfx::DataSourceSurface.
 */
class DataTextureSourceD3D9 : public DataTextureSource
                            , public TextureSourceD3D9
                            , public TileIterator
{
public:
  DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                        CompositorD3D9* aCompositor,
                        TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT,
                        StereoMode aStereoMode = StereoMode::MONO);

  DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                        gfx::IntSize aSize,
                        CompositorD3D9* aCompositor,
                        IDirect3DTexture9* aTexture,
                        TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);

  virtual ~DataTextureSourceD3D9();

  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE;

  // TextureSource

  virtual TextureSourceD3D9* AsSourceD3D9() MOZ_OVERRIDE { return this; }

  virtual IDirect3DTexture9* GetD3D9Texture() MOZ_OVERRIDE;

  virtual DataTextureSource* AsDataTextureSource() MOZ_OVERRIDE { return this; }

  virtual void DeallocateDeviceData() MOZ_OVERRIDE { mTexture = nullptr; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  // TileIterator

  virtual TileIterator* AsTileIterator() MOZ_OVERRIDE { return mIsTiled ? this : nullptr; }

  virtual size_t GetTileCount() MOZ_OVERRIDE { return mTileTextures.size(); }

  virtual bool NextTile() MOZ_OVERRIDE { return (++mCurrentTile < mTileTextures.size()); }

  virtual nsIntRect GetTileRect() MOZ_OVERRIDE;

  virtual void EndTileIteration() MOZ_OVERRIDE { mIterating = false; }

  virtual void BeginTileIteration() MOZ_OVERRIDE
  {
    mIterating = true;
    mCurrentTile = 0;
  }

  /**
   * Copy the content of aTexture using the GPU.
   */
  bool UpdateFromTexture(IDirect3DTexture9* aTexture, const nsIntRegion* aRegion);

  // To use with DIBTextureHostD3D9

  bool Update(gfxWindowsSurface* aSurface);

protected:
  gfx::IntRect GetTileRect(uint32_t aTileIndex) const;

  void Reset();

  std::vector< RefPtr<IDirect3DTexture9> > mTileTextures;
  RefPtr<CompositorD3D9> mCompositor;
  gfx::SurfaceFormat mFormat;
  uint32_t mCurrentTile;
  TextureFlags mFlags;
  bool mIsTiled;
  bool mIterating;
};

/**
 * Can only be drawn into through Cairo and need a D3D9 context on the client side.
 * The corresponding TextureHost is TextureHostD3D9.
 */
class CairoTextureClientD3D9 : public TextureClient
{
public:
  CairoTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  virtual ~CairoTextureClientD3D9();

  // TextureClient

  virtual bool IsAllocated() const MOZ_OVERRIDE { return !!mTexture; }

  virtual bool Lock(OpenMode aOpenMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const { return mFormat; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

private:
  RefPtr<IDirect3DTexture9> mTexture;
  nsRefPtr<IDirect3DSurface9> mD3D9Surface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  nsRefPtr<gfxASurface> mSurface;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
  bool mNeedsClear;
  bool mLockRect;
};

/**
 * Can only be drawn into through Cairo.
 * Prefer CairoTextureClientD3D9 when possible.
 * The coresponding TextureHost is DIBTextureHostD3D9.
 */
class DIBTextureClientD3D9 : public TextureClient
{
public:
  DIBTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  virtual ~DIBTextureClientD3D9();

  // TextureClient

  virtual bool IsAllocated() const MOZ_OVERRIDE { return !!mSurface; }

  virtual bool Lock(OpenMode aOpenMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const { return mFormat; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual bool CanExposeDrawTarget() const MOZ_OVERRIDE { return true; }

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

protected:
  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

/**
 * Wraps a D3D9 texture, shared with the compositor though DXGI.
 * At the moment it is only used with D3D11 compositing, and the corresponding
 * TextureHost is DXGITextureHostD3D11.
 */
class SharedTextureClientD3D9 : public TextureClient
{
public:
  SharedTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  virtual ~SharedTextureClientD3D9();

  // TextureClient

  virtual bool IsAllocated() const MOZ_OVERRIDE { return !!mTexture; }

  virtual bool Lock(OpenMode aOpenMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  void InitWith(IDirect3DTexture9* aTexture, HANDLE aSharedHandle, D3DSURFACE_DESC aDesc)
  {
    MOZ_ASSERT(!mTexture);
    mTexture = aTexture;
    mHandle = aSharedHandle;
    mDesc = aDesc;
  }

  virtual gfx::IntSize GetSize() const
  {
    return gfx::IntSize(mDesc.Width, mDesc.Height);
  }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) MOZ_OVERRIDE
  {
    return false;
  }

private:
  RefPtr<IDirect3DTexture9> mTexture;
  gfx::SurfaceFormat mFormat;
  HANDLE mHandle;
  D3DSURFACE_DESC mDesc;
  bool mIsLocked;
};

class TextureHostD3D9 : public TextureHost
{
public:
  TextureHostD3D9(TextureFlags aFlags,
                  const SurfaceDescriptorD3D9& aDescriptor);

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual void Updated(const nsIntRegion* aRegion) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr;
  }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

protected:
  TextureHostD3D9(TextureFlags aFlags);
  IDirect3DDevice9* GetDevice();

  RefPtr<DataTextureSourceD3D9> mTextureSource;
  RefPtr<IDirect3DTexture9> mTexture;
  RefPtr<CompositorD3D9> mCompositor;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

class DIBTextureHostD3D9 : public TextureHost
{
public:
  DIBTextureHostD3D9(TextureFlags aFlags,
                     const SurfaceDescriptorDIB& aDescriptor);

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual void Updated(const nsIntRegion* aRegion = nullptr);

  virtual bool CanExposeDrawTarget() const MOZ_OVERRIDE { return true; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

protected:
  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<DataTextureSourceD3D9> mTextureSource;
  RefPtr<CompositorD3D9> mCompositor;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  bool mIsLocked;
};

class CompositingRenderTargetD3D9 : public CompositingRenderTarget,
                                    public TextureSourceD3D9
{
public:
  CompositingRenderTargetD3D9(IDirect3DTexture9* aTexture,
                              SurfaceInitMode aInit,
                              const gfx::IntRect& aRect);
  // use for rendering to the main window, cannot be rendered as a texture
  CompositingRenderTargetD3D9(IDirect3DSurface9* aSurface,
                              SurfaceInitMode aInit,
                              const gfx::IntRect& aRect);
  virtual ~CompositingRenderTargetD3D9();

  virtual TextureSourceD3D9* AsSourceD3D9() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mTexture,
               "No texture, can't be indirectly rendered. Is this the screen backbuffer?");
    return this;
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  void BindRenderTarget(IDirect3DDevice9* aDevice);

  IDirect3DSurface9* GetD3D9Surface() const { return mSurface; }

private:
  friend class CompositorD3D9;

  nsRefPtr<IDirect3DSurface9> mSurface;
  SurfaceInitMode mInitMode;
  bool mInitialized;
};

}
}

#endif /* MOZILLA_GFX_TEXTURED3D9_H */
