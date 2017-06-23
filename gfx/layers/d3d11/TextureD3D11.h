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

class MOZ_RAII AutoTextureLock
{
public:
  AutoTextureLock(IDXGIKeyedMutex* aMutex, HRESULT& aResult,
                  uint32_t aTimeout = 0);
  ~AutoTextureLock();

private:
  RefPtr<IDXGIKeyedMutex> mMutex;
  HRESULT mResult;
};

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

  virtual bool Lock(OpenMode aMode) override;

  virtual void Unlock() override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  virtual TextureData*
  CreateSimilar(LayersIPCChannel* aAllocator,
                LayersBackend aLayersBackend,
                TextureFlags aFlags,
                TextureAllocationFlags aAllocFlags) const override;

  virtual void SyncWithObject(SyncObject* aSync) override;

  ID3D11Texture2D* GetD3D11Texture() { return mTexture; }

  virtual void Deallocate(LayersIPCChannel* aAllocator) override;

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
                                  LayersIPCChannel* aAllocator);

class DXGIYCbCrTextureData : public TextureData
{
public:
  static DXGIYCbCrTextureData*
  Create(IDirect3DTexture9* aTextureY,
         IDirect3DTexture9* aTextureCb,
         IDirect3DTexture9* aTextureCr,
         HANDLE aHandleY,
         HANDLE aHandleCb,
         HANDLE aHandleCr,
         const gfx::IntSize& aSize,
         const gfx::IntSize& aSizeY,
         const gfx::IntSize& aSizeCbCr);

  static DXGIYCbCrTextureData*
  Create(ID3D11Texture2D* aTextureCb,
         ID3D11Texture2D* aTextureY,
         ID3D11Texture2D* aTextureCr,
         const gfx::IntSize& aSize,
         const gfx::IntSize& aSizeY,
         const gfx::IntSize& aSizeCbCr);

  virtual bool Lock(OpenMode) override { return true; }

  virtual void Unlock() override {}

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override { return nullptr; }

  virtual void Deallocate(LayersIPCChannel* aAllocator) override;

  virtual bool UpdateFromSurface(gfx::SourceSurface*) override { return false; }

  virtual TextureFlags GetTextureFlags() const override
  {
    return TextureFlags::DEALLOCATE_MAIN_THREAD;
  }

  ID3D11Texture2D* GetD3D11Texture(size_t index) { return mD3D11Textures[index]; }

protected:
   RefPtr<ID3D11Texture2D> mD3D11Textures[3];
   RefPtr<IDirect3DTexture9> mD3D9Textures[3];
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
  /// Constructor allowing the texture to perform texture uploads.
  ///
  /// The texture can be used as an actual DataTextureSource.
  DataTextureSourceD3D11(ID3D11Device* aDevice, gfx::SurfaceFormat aFormat, TextureFlags aFlags);

  /// Constructor for textures created around DXGI shared handles, disallowing
  /// texture uploads.
  ///
  /// The texture CANNOT be used as a DataTextureSource.
  DataTextureSourceD3D11(ID3D11Device* aDevice, gfx::SurfaceFormat aFormat, ID3D11Texture2D* aTexture);

  DataTextureSourceD3D11(gfx::SurfaceFormat aFormat, TextureSourceProvider* aProvider, ID3D11Texture2D* aTexture);
  DataTextureSourceD3D11(gfx::SurfaceFormat aFormat, TextureSourceProvider* aProvider, TextureFlags aFlags);

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

  // Returns nullptr if this texture was created by a DXGI TextureHost.
  virtual DataTextureSource* AsDataTextureSource() override { return mAllowTextureUploads ? this : nullptr; }

  virtual void DeallocateDeviceData() override { mTexture = nullptr; }

  virtual gfx::IntSize GetSize() const  override { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const override { return mFormat; }

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

  RefPtr<TextureSource> ExtractCurrentTile() override;

  void Reset();
protected:
  gfx::IntRect GetTileRect(uint32_t aIndex) const;

  std::vector< RefPtr<ID3D11Texture2D> > mTileTextures;
  std::vector< RefPtr<ID3D11ShaderResourceView> > mTileSRVs;
  RefPtr<ID3D11Device> mDevice;
  gfx::SurfaceFormat mFormat;
  TextureFlags mFlags;
  uint32_t mCurrentTile;
  bool mIsTiled;
  bool mIterating;
  // Sadly, the code was originally organized so that this class is used both in
  // the cases where we want to perform texture uploads through the DataTextureSource
  // interface, and the cases where we wrap the texture around an existing DXGI
  // handle in which case we should not use it as a DataTextureSource.
  // This member differentiates the two scenarios. When it is false the texture
  // "pretends" to not be a DataTextureSource.
  bool mAllowTextureUploads;
};

already_AddRefed<TextureClient>
CreateD3D11TextureClientWithDevice(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags,
                                   ID3D11Device* aDevice,
                                   LayersIPCChannel* aAllocator);


/**
 * A TextureHost for shared D3D11 textures.
 */
class DXGITextureHostD3D11 : public TextureHost
{
public:
  DXGITextureHostD3D11(TextureFlags aFlags,
                       const SurfaceDescriptorD3D10& aDescriptor);

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;
  virtual bool AcquireTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

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

  virtual void GetWRImageKeys(nsTArray<wr::ImageKey>& aImageKeys,
                              const std::function<wr::ImageKey()>& aImageKeyAllocator) override;

  virtual void AddWRImage(wr::WebRenderAPI* aAPI,
                          Range<const wr::ImageKey>& aImageKeys,
                          const wr::ExternalImageId& aExtID) override;

  virtual void PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                 const WrRect& aBounds,
                                 const WrClipRegionToken aClip,
                                 wr::ImageRendering aFilter,
                                 Range<const wr::ImageKey>& aImageKeys) override;

protected:
  bool LockInternal();
  void UnlockInternal();

  bool EnsureTextureSource();

  RefPtr<ID3D11Device> GetDevice();

  bool OpenSharedHandle();

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<DataTextureSourceD3D11> mTextureSource;
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
  virtual bool AcquireTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual void DeallocateDeviceData() override{}

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  virtual gfx::SurfaceFormat GetFormat() const override{ return gfx::SurfaceFormat::YUV; }

  // Bug 1305906 fixes YUVColorSpace handling
  virtual YUVColorSpace GetYUVColorSpace() const override { return YUVColorSpace::BT601; }

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr;
  }

  virtual void GetWRImageKeys(nsTArray<wr::ImageKey>& aImageKeys,
                              const std::function<wr::ImageKey()>& aImageKeyAllocator) override;

  virtual void AddWRImage(wr::WebRenderAPI* aAPI,
                          Range<const wr::ImageKey>& aImageKeys,
                          const wr::ExternalImageId& aExtID) override;

  virtual void PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                 const WrRect& aBounds,
                                 const WrClipRegionToken aClip,
                                 wr::ImageRendering aFilter,
                                 Range<const wr::ImageKey>& aImageKeys) override;

private:
  bool EnsureTextureSource();

protected:
  RefPtr<ID3D11Device> GetDevice();

  bool OpenSharedHandle();

  RefPtr<ID3D11Texture2D> mTextures[3];
  RefPtr<DataTextureSourceD3D11> mTextureSources[3];

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
  explicit SyncObjectD3D11(SyncHandle aSyncHandle, ID3D11Device* aDevice);
  virtual void FinalizeFrame();
  virtual bool IsSyncObjectValid();

  virtual SyncType GetSyncType() { return SyncType::D3D11; }

  void RegisterTexture(ID3D11Texture2D* aTexture);

private:
  bool Init();

private:
  SyncHandle mSyncHandle;
  RefPtr<ID3D11Device> mD3D11Device;
  RefPtr<ID3D11Texture2D> mD3D11Texture;
  RefPtr<IDXGIKeyedMutex> mKeyedMutex;
  std::vector<ID3D11Texture2D*> mD3D11SyncedTextures;
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

uint32_t GetMaxTextureSizeFromDevice(ID3D11Device* aDevice);
void ReportTextureMemoryUsage(ID3D11Texture2D* aTexture, size_t aBytes);

class AutoLockD3D11Texture
{
public:
  explicit AutoLockD3D11Texture(ID3D11Texture2D* aTexture);
  ~AutoLockD3D11Texture();

private:
  RefPtr<IDXGIKeyedMutex> mMutex;
};

class D3D11MTAutoEnter
{
public:
  explicit D3D11MTAutoEnter(already_AddRefed<ID3D10Multithread> aMT)
    : mMT(aMT)
  {
    mMT->Enter();
  }
  ~D3D11MTAutoEnter() { mMT->Leave(); }

private:
  RefPtr<ID3D10Multithread> mMT;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTURED3D11_H */
