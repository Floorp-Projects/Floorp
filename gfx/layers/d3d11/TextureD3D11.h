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

class gfxD2DSurface;

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
  TextureClientD3D11(gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  virtual ~TextureClientD3D11();

  // TextureClient

  virtual bool IsAllocated() const MOZ_OVERRIDE { return mTexture || mTexture10; }

  virtual bool Lock(OpenMode aOpenMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ImplementsLocking() const MOZ_OVERRIDE { return true; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual bool CanExposeDrawTarget() const MOZ_OVERRIDE { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const MOZ_OVERRIDE;

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
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE;

  // TextureSource

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE { return this; }

  virtual ID3D11Texture2D* GetD3D11Texture() const MOZ_OVERRIDE;

  virtual DataTextureSource* AsDataTextureSource() MOZ_OVERRIDE { return this; }

  virtual void DeallocateDeviceData() MOZ_OVERRIDE { mTexture = nullptr; }

  virtual gfx::IntSize GetSize() const  MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  // BigImageIterator

  virtual BigImageIterator* AsBigImageIterator() MOZ_OVERRIDE { return mIsTiled ? this : nullptr; }

  virtual size_t GetTileCount() MOZ_OVERRIDE { return mTileTextures.size(); }

  virtual bool NextTile() MOZ_OVERRIDE { return (++mCurrentTile < mTileTextures.size()); }

  virtual nsIntRect GetTileRect() MOZ_OVERRIDE;

  virtual void EndBigImageIteration() MOZ_OVERRIDE { mIterating = false; }

  virtual void BeginBigImageIteration() MOZ_OVERRIDE
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

  virtual TextureSource* GetTextureSources() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE {}

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
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

class CompositingRenderTargetD3D11 : public CompositingRenderTarget,
                                     public TextureSourceD3D11
{
public:
  CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture,
                               const gfx::IntPoint& aOrigin);

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  void SetSize(const gfx::IntSize& aSize) { mSize = aSize; }

private:
  friend class CompositorD3D11;

  RefPtr<ID3D11RenderTargetView> mRTView;
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
