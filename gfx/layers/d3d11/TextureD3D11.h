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
class TextureClientD3D11 : public TextureClient,
                           public TextureClientDrawTarget
{
public:
  TextureClientD3D11(gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  virtual ~TextureClientD3D11();

  // TextureClient

  virtual bool IsAllocated() const MOZ_OVERRIDE { return !!mTexture; }

  virtual bool Lock(OpenMode aOpenMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE { return nullptr; }

  // TextureClientDrawTarget

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual TextureClientDrawTarget* AsTextureClientDrawTarget() MOZ_OVERRIDE { return this; }

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

protected:
  gfx::IntSize mSize;
  RefPtr<ID3D10Texture2D> mTexture;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::SurfaceFormat mFormat;
  bool mIsLocked;
  bool mNeedsClear;
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
                             , public TileIterator
{
public:
  DataTextureSourceD3D11(gfx::SurfaceFormat aFormat, CompositorD3D11* aCompositor);

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

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE;

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

class DeprecatedTextureClientD3D11 : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientD3D11(CompositableForwarder* aCompositableForwarder,
                               const TextureInfo& aTextureInfo);
  virtual ~DeprecatedTextureClientD3D11();

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE
  {
    return aType == TEXTURE_CONTENT;
  }

  virtual bool EnsureAllocated(gfx::IntSize aSize,
                               gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxASurface* LockSurface() MOZ_OVERRIDE;
  virtual gfx::DrawTarget* LockDrawTarget() MOZ_OVERRIDE;
  virtual gfx::BackendType BackendType() MOZ_OVERRIDE
  {
    return gfx::BackendType::DIRECT2D;
  }

  virtual void Unlock() MOZ_OVERRIDE;

  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxContentType GetContentType() MOZ_OVERRIDE
  {
    return mContentType;
  }

private:
  void EnsureSurface();
  void EnsureDrawTarget();
  void LockTexture();
  void ReleaseTexture();
  void ClearDT();

  RefPtr<ID3D10Texture2D> mTexture;
  nsRefPtr<gfxD2DSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  bool mIsLocked;
  gfxContentType mContentType;
};

class DeprecatedTextureHostShmemD3D11 : public DeprecatedTextureHost
                                      , public TextureSourceD3D11
                                      , public TileIterator
{
public:
  DeprecatedTextureHostShmemD3D11()
    : mDevice(nullptr)
    , mIsTiled(false)
    , mCurrentTile(0)
    , mIterating(false)
  {
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE { return this; }

  virtual ID3D11Texture2D* GetD3D11Texture() const MOZ_OVERRIDE
  {
    return mIsTiled ? mTileTextures[mCurrentTile].get()
                    : TextureSourceD3D11::GetD3D11Texture();
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE
  {
    return LayerRenderState();
  }

  virtual bool Lock() MOZ_OVERRIDE { return true; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr;
  }

  virtual const char* Name() MOZ_OVERRIDE
  {
    return "DeprecatedTextureHostShmemD3D11";
  }

  virtual void BeginTileIteration() MOZ_OVERRIDE
  {
    mIterating = true;
    mCurrentTile = 0;
  }
  virtual void EndTileIteration() MOZ_OVERRIDE
  {
    mIterating = false;
  }
  virtual nsIntRect GetTileRect() MOZ_OVERRIDE;
  virtual size_t GetTileCount() MOZ_OVERRIDE { return mTileTextures.size(); }
  virtual bool NextTile() MOZ_OVERRIDE
  {
    return (++mCurrentTile < mTileTextures.size());
  }

  virtual TileIterator* AsTileIterator() MOZ_OVERRIDE
  {
    return mIsTiled ? this : nullptr;
  }
protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint *aOffset = nullptr) MOZ_OVERRIDE;

private:
  gfx::IntRect GetTileRect(uint32_t aID) const;

  RefPtr<ID3D11Device> mDevice;
  bool mIsTiled;
  std::vector< RefPtr<ID3D11Texture2D> > mTileTextures;
  uint32_t mCurrentTile;
  bool mIterating;
};

class DeprecatedTextureHostDXGID3D11 : public DeprecatedTextureHost
                                     , public TextureSourceD3D11
{
public:
  DeprecatedTextureHostDXGID3D11()
    : mDevice(nullptr)
  {
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual bool Lock() MOZ_OVERRIDE;
  virtual void Unlock() MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

  virtual const char* Name() { return "DeprecatedTextureHostDXGID3D11"; }

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;
private:
  void LockTexture();
  void ReleaseTexture();

  RefPtr<ID3D11Device> mDevice;
};

class DeprecatedTextureHostYCbCrD3D11 : public DeprecatedTextureHost
{
public:
  DeprecatedTextureHostYCbCrD3D11();
  ~DeprecatedTextureHostYCbCrD3D11();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE
  {
    return mFirstSource->AsSourceD3D11();
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  TextureSource* GetSubSource(int index) MOZ_OVERRIDE
  {
    return mFirstSource ? mFirstSource->GetSubSource(index) : nullptr;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

  virtual const char* Name() MOZ_OVERRIDE
  {
    return "TextureImageDeprecatedTextureHostD3D11";
  }

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;

  ID3D11Device* GetDevice();

private:
  gfx::IntSize mSize;
  RefPtr<DataTextureSource> mFirstSource;
  RefPtr<CompositorD3D11> mCompositor;
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
