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
  already_AddRefed<IDirect3DTexture9> InitTextures(
    DeviceManagerD3D9* aDeviceManager,
    const gfx::IntSize &aSize,
    _D3DFORMAT aFormat,
    RefPtr<IDirect3DSurface9>& aSurface,
    D3DLOCKED_RECT& aLockedRect);

  already_AddRefed<IDirect3DTexture9> DataToTexture(
    DeviceManagerD3D9* aDeviceManager,
    unsigned char *aData,
    int aStride,
    const gfx::IntSize &aSize,
    _D3DFORMAT aFormat,
    uint32_t aBPP);

  // aTexture should be in SYSTEMMEM, returns a texture in the default
  // pool (that is, in video memory).
  already_AddRefed<IDirect3DTexture9> TextureToTexture(
    DeviceManagerD3D9* aDeviceManager,
    IDirect3DTexture9* aTexture,
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
                            , public BigImageIterator
{
public:
  DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                        CompositorD3D9* aCompositor,
                        TextureFlags aFlags = TextureFlags::DEFAULT,
                        StereoMode aStereoMode = StereoMode::MONO);

  DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                        gfx::IntSize aSize,
                        CompositorD3D9* aCompositor,
                        IDirect3DTexture9* aTexture,
                        TextureFlags aFlags = TextureFlags::DEFAULT);

  virtual ~DataTextureSourceD3D9();

  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override;

  // TextureSource

  virtual TextureSourceD3D9* AsSourceD3D9() override { return this; }

  virtual IDirect3DTexture9* GetD3D9Texture() override;

  virtual DataTextureSource* AsDataTextureSource() override { return this; }

  virtual void DeallocateDeviceData() override { mTexture = nullptr; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual void SetCompositor(Compositor* aCompositor) override;

  // BigImageIterator

  virtual BigImageIterator* AsBigImageIterator() override { return mIsTiled ? this : nullptr; }

  virtual size_t GetTileCount() override { return mTileTextures.size(); }

  virtual bool NextTile() override { return (++mCurrentTile < mTileTextures.size()); }

  virtual gfx::IntRect GetTileRect() override;

  virtual void EndBigImageIteration() override { mIterating = false; }

  virtual void BeginBigImageIteration() override
  {
    mIterating = true;
    mCurrentTile = 0;
  }

  /**
   * Copy the content of aTexture using the GPU.
   */
  bool UpdateFromTexture(IDirect3DTexture9* aTexture, const nsIntRegion* aRegion);

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
 * Needs a D3D9 context on the client side.
 * The corresponding TextureHost is TextureHostD3D9.
 */
class TextureClientD3D9 : public TextureClient
{
public:
  TextureClientD3D9(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat,
                         TextureFlags aFlags);

  virtual ~TextureClientD3D9();

  // TextureClient

  virtual bool IsAllocated() const override { return !!mTexture; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const { return mFormat; }

  virtual bool CanExposeDrawTarget() const override { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() override;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) override;

  virtual bool HasInternalBuffer() const override { return true; }

  virtual already_AddRefed<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

private:
  RefPtr<IDirect3DTexture9> mTexture;
  nsRefPtr<IDirect3DSurface9> mD3D9Surface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
  bool mNeedsClear;
  bool mNeedsClearWhite;
  bool mLockRect;
};

/**
 * Wraps a D3D9 texture, shared with the compositor though DXGI.
 * At the moment it is only used with D3D11 compositing, and the corresponding
 * TextureHost is DXGITextureHostD3D11.
 */
class SharedTextureClientD3D9 : public TextureClient
{
public:
  SharedTextureClientD3D9(ISurfaceAllocator* aAllocator,
                          gfx::SurfaceFormat aFormat,
                          TextureFlags aFlags);

  virtual ~SharedTextureClientD3D9();

  // Creates a TextureClient and init width.
  static already_AddRefed<SharedTextureClientD3D9>
  Create(ISurfaceAllocator* aAllocator,
         gfx::SurfaceFormat aFormat,
         TextureFlags aFlags,
         IDirect3DTexture9* aTexture,
         HANDLE aSharedHandle,
         D3DSURFACE_DESC aDesc);

  // TextureClient

  virtual bool IsAllocated() const override { return !!mTexture; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const
  {
    return gfx::IntSize(mDesc.Width, mDesc.Height);
  }

  virtual bool HasInternalBuffer() const override { return true; }

  // This TextureClient should not be used in a context where we use CreateSimilar
  // (ex. component alpha) because the underlying texture data is always created by
  // an external producer.
  virtual already_AddRefed<TextureClient>
  CreateSimilar(TextureFlags, TextureAllocationFlags) const override { return nullptr; }

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

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

  virtual bool HasInternalBuffer() const override { return true; }

protected:
  TextureHostD3D9(TextureFlags aFlags);
  IDirect3DDevice9* GetDevice();

  virtual void UpdatedInternal(const nsIntRegion* aRegion) override;

  RefPtr<DataTextureSourceD3D9> mTextureSource;
  RefPtr<IDirect3DTexture9> mTexture;
  RefPtr<CompositorD3D9> mCompositor;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

class DXGITextureHostD3D9 : public TextureHost
{
public:
  DXGITextureHostD3D9(TextureFlags aFlags,
    const SurfaceDescriptorD3D10& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // TODO: cf bug 872568
  }

protected:
  void OpenSharedHandle();
  IDirect3DDevice9* GetDevice();

  RefPtr<DataTextureSourceD3D9> mTextureSource;
  RefPtr<CompositorD3D9> mCompositor;
  WindowsHandle mHandle;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  bool mIsLocked;
};

class DXGIYCbCrTextureHostD3D9 : public TextureHost
{
public:
  DXGIYCbCrTextureHostD3D9(TextureFlags aFlags,
                           const SurfaceDescriptorDXGIYCbCr& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return gfx::SurfaceFormat::YUV; }

  virtual bool Lock() override;
  virtual void Unlock() override;
  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

 protected:
  IDirect3DDevice9* GetDevice();

  HANDLE mHandles[3];
  RefPtr<IDirect3DTexture9> mTextures[3];
  RefPtr<DataTextureSourceD3D9> mTextureSources[3];

  RefPtr<CompositorD3D9> mCompositor;
  gfx::IntSize mSize;
  gfx::IntSize mSizeY;
  gfx::IntSize mSizeCbCr;
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

  virtual TextureSourceD3D9* AsSourceD3D9() override
  {
    MOZ_ASSERT(mTexture,
               "No texture, can't be indirectly rendered. Is this the screen backbuffer?");
    return this;
  }

  virtual gfx::IntSize GetSize() const override;

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
