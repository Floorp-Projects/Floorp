/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURED3D11_H
#define MOZILLA_GFX_TEXTURED3D11_H

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/GfxMessageUtils.h"
#include <d3d11.h>
#include "d3d9.h"
#include <vector>

namespace mozilla {
namespace layers {

class CompositorD3D11;

class DXGITextureData : public TextureData
{
public:
  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescrptor) override;

  static DXGITextureData*
  Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat, TextureAllocationFlags aFlags);

protected:
  bool PrepareDrawTargetInLock(OpenMode aMode);

  DXGITextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                  bool aNeedsClear, bool aNeedsClearWhite,
                  bool aIsForOutOfBandContent);

  virtual void GetDXGIResource(IDXGIResource** aOutResource) = 0;

  // Hold on to the DrawTarget because it is expensive to create one each ::Lock.
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mNeedsClear;
  bool mNeedsClearWhite;
  bool mHasSynchronization;
  bool mIsForOutOfBandContent;
};

class D3D11TextureData : public DXGITextureData
{
public:
  // If aDevice is null, use one provided by gfxWindowsPlatform.
  static DXGITextureData*
  Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
         TextureAllocationFlags aAllocFlags,
         ID3D11Device* aDevice = nullptr);
  static DXGITextureData*
  Create(gfx::SourceSurface* aSurface,
         TextureAllocationFlags aAllocFlags,
         ID3D11Device* aDevice = nullptr);

  virtual bool UpdateFromSurface(gfx::SourceSurface* aSurface) override;

  virtual bool Lock(OpenMode aMode, FenceHandle*) override;

  virtual void Unlock() override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual TextureData*
  CreateSimilar(ClientIPCAllocator* aAllocator,
                TextureFlags aFlags,
                TextureAllocationFlags aAllocFlags) const override;

  // TODO - merge this with the FenceHandle API!
  virtual void SyncWithObject(SyncObject* aSync) override;

  ID3D11Texture2D* GetD3D11Texture() { return mTexture; }

  virtual void Deallocate(ClientIPCAllocator* aAllocator) override;

  D3D11TextureData* AsD3D11TextureData() override {
    return this;
  }

  ~D3D11TextureData();
protected:
  D3D11TextureData(ID3D11Texture2D* aTexture,
                   gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                   bool aNeedsClear, bool aNeedsClearWhite,
                   bool aIsForOutOfBandContent);

  virtual void GetDXGIResource(IDXGIResource** aOutResource) override;

  static DXGITextureData*
  Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
         gfx::SourceSurface* aSurface,
         TextureAllocationFlags aAllocFlags,
         ID3D11Device* aDevice = nullptr);

  RefPtr<ID3D11Texture2D> mTexture;
};

already_AddRefed<TextureClient>
CreateD3D11extureClientWithDevice(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                  TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags,
                                  ID3D11Device* aDevice,
                                  ClientIPCAllocator* aAllocator);

class DXGIYCbCrTextureData : public TextureData
{
public:
  static DXGIYCbCrTextureData*
  Create(ClientIPCAllocator* aAllocator,
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

  static DXGIYCbCrTextureData*
  Create(ClientIPCAllocator* aAllocator,
         TextureFlags aFlags,
         ID3D11Texture2D* aTextureCb,
         ID3D11Texture2D* aTextureY,
         ID3D11Texture2D* aTextureCr,
         const gfx::IntSize& aSize,
         const gfx::IntSize& aSizeY,
         const gfx::IntSize& aSizeCbCr);

  virtual bool Lock(OpenMode, FenceHandle*) override { return true; }

  virtual void Unlock() override {}

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override { return nullptr; }

  // This TextureData should not be used in a context where we use CreateSimilar
  // (ex. component alpha) because the underlying texture is always created by
  // an external producer.
  virtual DXGIYCbCrTextureData*
  CreateSimilar(ClientIPCAllocator*, TextureFlags, TextureAllocationFlags) const override { return nullptr; }

  virtual void Deallocate(ClientIPCAllocator* aAllocator) override;

  virtual bool UpdateFromSurface(gfx::SourceSurface*) override { return false; }

  virtual TextureFlags GetTextureFlags() const override
  {
    return TextureFlags::DEALLOCATE_MAIN_THREAD;
  }

protected:
   RefPtr<IUnknown> mHoldRefs[3];
   HANDLE mHandles[3];
   gfx::IntSize mSize;
   gfx::IntSize mSizeY;
   gfx::IntSize mSizeCbCr;
};

/**
 * TextureSource that provides with the necessary APIs to be composited by a
 * CompositorD3D11.
 */
class TextureSourceD3D11
{
public:
  TextureSourceD3D11() : mFormatOverride(DXGI_FORMAT_UNKNOWN) {}
  virtual ~TextureSourceD3D11() {}

  virtual ID3D11Texture2D* GetD3D11Texture() const { return mTexture; }
  virtual ID3D11ShaderResourceView* GetShaderResourceView();
protected:
  virtual gfx::IntSize GetSize() const { return mSize; }

  gfx::IntSize mSize;
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<ID3D11ShaderResourceView> mSRV;
  DXGI_FORMAT mFormatOverride;
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

  virtual const char* Name() const override { return "DataTextureSourceD3D11"; }

  // DataTextureSource

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override;

  // TextureSource

  virtual TextureSourceD3D11* AsSourceD3D11() override { return this; }

  virtual ID3D11Texture2D* GetD3D11Texture() const override;

  virtual ID3D11ShaderResourceView* GetShaderResourceView() override;

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
  std::vector< RefPtr<ID3D11ShaderResourceView> > mTileSRVs;
  RefPtr<CompositorD3D11> mCompositor;
  gfx::SurfaceFormat mFormat;
  TextureFlags mFlags;
  uint32_t mCurrentTile;
  bool mIsTiled;
  bool mIterating;

};

already_AddRefed<TextureClient>
CreateD3D11TextureClientWithDevice(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags,
                                   ID3D11Device* aDevice,
                                   ClientIPCAllocator* aAllocator);


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

  virtual Compositor* GetCompositor() override;

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

  virtual bool Lock() override;
  virtual void Unlock() override;

  virtual bool LockWithoutCompositor() override;
  virtual void UnlockWithoutCompositor() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

protected:
  bool LockInternal();
  void UnlockInternal();

  RefPtr<ID3D11Device> GetDevice();

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

  virtual Compositor* GetCompositor() override;

  virtual gfx::SurfaceFormat GetFormat() const override{ return gfx::SurfaceFormat::YUV; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

protected:
  RefPtr<ID3D11Device> GetDevice();

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
                               const gfx::IntPoint& aOrigin,
                               DXGI_FORMAT aFormatOverride = DXGI_FORMAT_UNKNOWN);

  virtual const char* Name() const override { return "CompositingRenderTargetD3D11"; }

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
  virtual void FinalizeFrame();

  virtual SyncType GetSyncType() { return SyncType::D3D11; }

  void RegisterTexture(ID3D11Texture2D* aTexture);

private:
  RefPtr<ID3D11Texture2D> mD3D11Texture;
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
