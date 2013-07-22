/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURED3D11_H
#define MOZILLA_GFX_TEXTURED3D11_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "gfxWindowsPlatform.h"
#include <d3d11.h>
#include <vector>

class gfxD2DSurface;

namespace mozilla {
namespace layers {

class TextureSourceD3D11
{
public:
  TextureSourceD3D11() {}
  virtual ~TextureSourceD3D11() {}

  virtual ID3D11Texture2D* GetD3D11Texture() const { return mTextures[0]; }
  virtual bool IsYCbCrSource() const { return false; }

  struct YCbCrTextures
  {
    ID3D11Texture2D *mY;
    ID3D11Texture2D *mCb;
    ID3D11Texture2D *mCr;
  };

  virtual YCbCrTextures GetYCbCrTextures()
  {
    YCbCrTextures textures = { mTextures[0], mTextures[1], mTextures[2] };
    return textures;
  }

protected:
  virtual gfx::IntSize GetSize() const { return mSize; }

  gfx::IntSize mSize;
  RefPtr<ID3D11Texture2D> mTextures[3];
};

class CompositingRenderTargetD3D11 : public CompositingRenderTarget,
                                     public TextureSourceD3D11
{
public:
  CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture);

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

  virtual void EnsureAllocated(gfx::IntSize aSize,
                               gfxASurface::gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxASurface* LockSurface() MOZ_OVERRIDE;
  virtual gfx::DrawTarget* LockDrawTarget() MOZ_OVERRIDE;
  virtual void Unlock() MOZ_OVERRIDE;

  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE
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

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() MOZ_OVERRIDE
  {
    return "DeprecatedTextureHostShmemD3D11";
  }
#endif

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

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "DeprecatedTextureHostDXGID3D11"; }
#endif

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
                                      , public TextureSourceD3D11
{
public:
  DeprecatedTextureHostYCbCrD3D11()
    : mDevice(nullptr)
  {
    mFormat = gfx::FORMAT_YUV;
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D11* AsSourceD3D11() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual bool IsYCbCrSource() const MOZ_OVERRIDE { return true; }

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() MOZ_OVERRIDE
  {
    return "TextureImageDeprecatedTextureHostD3D11";
  }
#endif

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;

private:
  RefPtr<ID3D11Device> mDevice;
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
