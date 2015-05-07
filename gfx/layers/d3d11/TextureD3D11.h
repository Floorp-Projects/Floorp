/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURED3D11_H
#define MOZILLA_GFX_TEXTURED3D11_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/GfxMessageUtils.h"
#include <d3d11.h>
#include <vector>

namespace mozilla {
namespace layers {

class CompositorD3D11;

/**
 * A TextureClient to share a D3D10 texture with the compositor thread.
 * The corresponding TextureHost is DXGITextureHostD3D11
 */
class TextureClientD3D11 : public TextureClient
{
public:
  TextureClientD3D11(ISurfaceAllocator* aAllocator,
                     gfx::SurfaceFormat aFormat,
                     TextureFlags aFlags);

  virtual ~TextureClientD3D11();

  // Creates a TextureClient and init width.
  static TemporaryRef<TextureClientD3D11>
  Create(ISurfaceAllocator* aAllocator,
         gfx::SurfaceFormat aFormat,
         TextureFlags aFlags,
         ID3D11Texture2D* aTexture,
         gfx::IntSize aSize);

  // TextureClient

  virtual bool IsAllocated() const override { return mTexture || mTexture10; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mIsLocked; }

  virtual bool ImplementsLocking() const override { return true; }

  virtual bool HasInternalBuffer() const override { return false; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool CanExposeDrawTarget() const override { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() override;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) override;

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override;

  virtual void SyncWithObject(SyncObject* aSyncObject) override;

protected:
  gfx::IntSize mSize;
  RefPtr<ID3D10Texture2D> mTexture10;
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
  bool mNeedsClear;
  bool mNeedsClearWhite;
};

class DXGIYCbCrTextureClient : public TextureClient
{
public:
  DXGIYCbCrTextureClient(ISurfaceAllocator* aAllocator,
                         TextureFlags aFlags);

  virtual ~DXGIYCbCrTextureClient();

  // Creates a TextureClient and init width.
  static TemporaryRef<DXGIYCbCrTextureClient>
  Create(ISurfaceAllocator* aAllocator,
         TextureFlags aFlags,
         IUnknown* aTextureY,
         IUnknown* aTextureCb,
         IUnknown* aTextureCr,
         HANDLE aHandleY,
         HANDLE aHandleCb,
         HANDLE aHandleCr,
         const gfx::IntSize& aSize,
         const gfx::IntSize& aSizeY,
         const gfx::IntSize& aSizeCbCr);

  // TextureClient

  virtual bool IsAllocated() const override{ return !!mHoldRefs[0]; }

  virtual bool Lock(OpenMode aOpenMode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override{ return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  virtual gfx::IntSize GetSize() const
  {
    return mSize;
  }

  virtual bool HasInternalBuffer() const override{ return true; }

    // This TextureClient should not be used in a context where we use CreateSimilar
    // (ex. component alpha) because the underlying texture data is always created by
    // an external producer.
    virtual TemporaryRef<TextureClient>
    CreateSimilar(TextureFlags, TextureAllocationFlags) const override{ return nullptr; }

private:
  RefPtr<IUnknown> mHoldRefs[3];
  HANDLE mHandles[3];
  gfx::IntSize mSize;
  gfx::IntSize mSizeY;
  gfx::IntSize mSizeCbCr;
  bool mIsLocked;
};


/**
 * TextureSource that provides with the necessary APIs to be composited by a
 * CompositorD3D11.
 */
class TextureSourceD3D11
{
public:
  TextureSourceD3D11() {}
  virtual ~TextureSourceD3D11() {}

  virtual ID3D11Texture2D* GetD3D11Texture() const { return mTexture; }

protected:
  virtual gfx::IntSize GetSize() const { return mSize; }

  gfx::IntSize mSize;
  RefPtr<ID3D11Texture2D> mTexture;
};

/**
 * A TextureSource that implements the DataTextureSource interface.
 * it can be used without a TextureHost and is able to upload texture data
 * from a gfx::DataSourceSurface.
 */
class DataTextureSourceD3D11 : public DataTextureSource
                             , public TextureSourceD3D11
                             , public BigImageIterator
{
public:
  DataTextureSourceD3D11(gfx::SurfaceFormat aFormat, CompositorD3D11* aCompositor,
                         TextureFlags aFlags);

  DataTextureSourceD3D11(gfx::SurfaceFormat aFormat, CompositorD3D11* aCompositor,
                         ID3D11Texture2D* aTexture);

  virtual ~DataTextureSourceD3D11();


  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override;

  // TextureSource

  virtual TextureSourceD3D11* AsSourceD3D11() override { return this; }

  virtual ID3D11Texture2D* GetD3D11Texture() const override;

  virtual DataTextureSource* AsDataTextureSource() override { return this; }

  virtual void DeallocateDeviceData() override { mTexture = nullptr; }

  virtual gfx::IntSize GetSize() const  override { return mSize; }

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

protected:
  gfx::IntRect GetTileRect(uint32_t aIndex) const;

  void Reset();

  std::vector< RefPtr<ID3D11Texture2D> > mTileTextures;
  RefPtr<CompositorD3D11> mCompositor;
  gfx::SurfaceFormat mFormat;
  TextureFlags mFlags;
  uint32_t mCurrentTile;
  bool mIsTiled;
  bool mIterating;

};

/**
 * A TextureHost for shared D3D11 textures.
 */
class DXGITextureHostD3D11 : public TextureHost
{
public:
  DXGITextureHostD3D11(TextureFlags aFlags,
                       const SurfaceDescriptorD3D10& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

protected:
  ID3D11Device* GetDevice();

  bool OpenSharedHandle();

  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<DataTextureSourceD3D11> mTextureSource;
  RefPtr<CompositorD3D11> mCompositor;
  gfx::IntSize mSize;
  WindowsHandle mHandle;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
};

class DXGIYCbCrTextureHostD3D11 : public TextureHost
{
public:
  DXGIYCbCrTextureHostD3D11(TextureFlags aFlags,
                            const SurfaceDescriptorDXGIYCbCr& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override{}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual gfx::SurfaceFormat GetFormat() const override{ return gfx::SurfaceFormat::YUV; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override{ return mSize; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

protected:
  ID3D11Device* GetDevice();

  bool OpenSharedHandle();

  RefPtr<ID3D11Texture2D> mTextures[3];
  RefPtr<DataTextureSourceD3D11> mTextureSources[3];

  RefPtr<CompositorD3D11> mCompositor;
  gfx::IntSize mSize;
  WindowsHandle mHandles[3];
  bool mIsLocked;
};

class CompositingRenderTargetD3D11 : public CompositingRenderTarget,
                                     public TextureSourceD3D11
{
public:
  CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture,
                               const gfx::IntPoint& aOrigin);

  virtual TextureSourceD3D11* AsSourceD3D11() override { return this; }

  void BindRenderTarget(ID3D11DeviceContext* aContext);

  virtual gfx::IntSize GetSize() const override;

  void SetSize(const gfx::IntSize& aSize) { mSize = aSize; }

private:
  friend class CompositorD3D11;

  RefPtr<ID3D11RenderTargetView> mRTView;
};

class SyncObjectD3D11 : public SyncObject
{
public:
  SyncObjectD3D11(SyncHandle aSyncHandle);

  virtual SyncType GetSyncType() { return SyncType::D3D11; }
  virtual void FinalizeFrame();

  void RegisterTexture(ID3D11Texture2D* aTexture);
  void RegisterTexture(ID3D10Texture2D* aTexture);

private:
  RefPtr<ID3D11Texture2D> mD3D11Texture;
  RefPtr<ID3D10Texture2D> mD3D10Texture;
  std::vector<ID3D10Texture2D*> mD3D10SyncedTextures;
  std::vector<ID3D11Texture2D*> mD3D11SyncedTextures;
  SyncHandle mHandle;
};

inline uint32_t GetMaxTextureSizeForFeatureLevel(D3D_FEATURE_LEVEL aFeatureLevel)
{
  int32_t maxTextureSize;
  switch (aFeatureLevel) {
  case D3D_FEATURE_LEVEL_11_1:
  case D3D_FEATURE_LEVEL_11_0:
    maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  case D3D_FEATURE_LEVEL_10_1:
  case D3D_FEATURE_LEVEL_10_0:
    maxTextureSize = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  case D3D_FEATURE_LEVEL_9_3:
    maxTextureSize = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    break;
  default:
    maxTextureSize = D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  }
  return maxTextureSize;
}

}
}

#endif /* MOZILLA_GFX_TEXTURED3D11_H */
