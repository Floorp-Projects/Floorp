/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureD3D11.h"

#include "CompositorD3D11.h"
#include "Effects.h"
#include "MainThreadUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {

using namespace gfx;

namespace layers {

static const GUID sD3D11TextureUsage = {
    0xd89275b0,
    0x6c7d,
    0x4038,
    {0xb5, 0xfa, 0x4d, 0x87, 0x16, 0xd5, 0xcc, 0x4e}};

/* This class gets its lifetime tied to a D3D texture
 * and increments memory usage on construction and decrements
 * on destruction */
class TextureMemoryMeasurer final : public IUnknown {
 public:
  explicit TextureMemoryMeasurer(size_t aMemoryUsed) {
    mMemoryUsed = aMemoryUsed;
    gfxWindowsPlatform::sD3D11SharedTextures += mMemoryUsed;
    mRefCnt = 0;
  }
  STDMETHODIMP_(ULONG) AddRef() {
    mRefCnt++;
    return mRefCnt;
  }
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) {
    IUnknown* punk = nullptr;
    if (riid == IID_IUnknown) {
      punk = this;
    }
    *ppvObject = punk;
    if (punk) {
      punk->AddRef();
      return S_OK;
    } else {
      return E_NOINTERFACE;
    }
  }

  STDMETHODIMP_(ULONG) Release() {
    int refCnt = --mRefCnt;
    if (refCnt == 0) {
      gfxWindowsPlatform::sD3D11SharedTextures -= mMemoryUsed;
      delete this;
    }
    return refCnt;
  }

 private:
  int mRefCnt;
  int mMemoryUsed;

  ~TextureMemoryMeasurer() = default;
};

static DXGI_FORMAT SurfaceFormatToDXGIFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case SurfaceFormat::B8G8R8A8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case SurfaceFormat::B8G8R8X8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case SurfaceFormat::R8G8B8A8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case SurfaceFormat::R8G8B8X8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case SurfaceFormat::A8:
      return DXGI_FORMAT_R8_UNORM;
    case SurfaceFormat::A16:
      return DXGI_FORMAT_R16_UNORM;
    default:
      MOZ_ASSERT(false, "unsupported format");
      return DXGI_FORMAT_UNKNOWN;
  }
}

void ReportTextureMemoryUsage(ID3D11Texture2D* aTexture, size_t aBytes) {
  aTexture->SetPrivateDataInterface(sD3D11TextureUsage,
                                    new TextureMemoryMeasurer(aBytes));
}

static uint32_t GetRequiredTilesD3D11(uint32_t aSize, uint32_t aMaxSize) {
  uint32_t requiredTiles = aSize / aMaxSize;
  if (aSize % aMaxSize) {
    requiredTiles++;
  }
  return requiredTiles;
}

static IntRect GetTileRectD3D11(uint32_t aID, IntSize aSize,
                                uint32_t aMaxSize) {
  uint32_t horizontalTiles = GetRequiredTilesD3D11(aSize.width, aMaxSize);
  uint32_t verticalTiles = GetRequiredTilesD3D11(aSize.height, aMaxSize);

  uint32_t verticalTile = aID / horizontalTiles;
  uint32_t horizontalTile = aID % horizontalTiles;

  return IntRect(
      horizontalTile * aMaxSize, verticalTile * aMaxSize,
      horizontalTile < (horizontalTiles - 1) ? aMaxSize
                                             : aSize.width % aMaxSize,
      verticalTile < (verticalTiles - 1) ? aMaxSize : aSize.height % aMaxSize);
}

AutoTextureLock::AutoTextureLock(IDXGIKeyedMutex* aMutex, HRESULT& aResult,
                                 uint32_t aTimeout) {
  mMutex = aMutex;
  if (mMutex) {
    mResult = mMutex->AcquireSync(0, aTimeout);
    aResult = mResult;
  } else {
    aResult = E_INVALIDARG;
  }
}

AutoTextureLock::~AutoTextureLock() {
  if (mMutex && !FAILED(mResult) && mResult != WAIT_TIMEOUT &&
      mResult != WAIT_ABANDONED) {
    mMutex->ReleaseSync(0);
  }
}

ID3D11ShaderResourceView* TextureSourceD3D11::GetShaderResourceView() {
  MOZ_ASSERT(mTexture == GetD3D11Texture(),
             "You need to override GetShaderResourceView if you're overriding "
             "GetD3D11Texture!");

  if (!mSRV && mTexture) {
    RefPtr<ID3D11Device> device;
    mTexture->GetDevice(getter_AddRefs(device));

    // see comment in CompositingRenderTargetD3D11 constructor
    CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(D3D11_SRV_DIMENSION_TEXTURE2D,
                                             mFormatOverride);
    D3D11_SHADER_RESOURCE_VIEW_DESC* desc =
        mFormatOverride == DXGI_FORMAT_UNKNOWN ? nullptr : &srvDesc;

    HRESULT hr =
        device->CreateShaderResourceView(mTexture, desc, getter_AddRefs(mSRV));
    if (FAILED(hr)) {
      gfxCriticalNote << "[D3D11] TextureSourceD3D11:GetShaderResourceView "
                         "CreateSRV failure "
                      << gfx::hexa(hr);
      return nullptr;
    }
  }
  return mSRV;
}

DataTextureSourceD3D11::DataTextureSourceD3D11(ID3D11Device* aDevice,
                                               SurfaceFormat aFormat,
                                               TextureFlags aFlags)
    : mDevice(aDevice),
      mFormat(aFormat),
      mFlags(aFlags),
      mCurrentTile(0),
      mIsTiled(false),
      mIterating(false),
      mAllowTextureUploads(true) {}

DataTextureSourceD3D11::DataTextureSourceD3D11(ID3D11Device* aDevice,
                                               SurfaceFormat aFormat,
                                               ID3D11Texture2D* aTexture)
    : mDevice(aDevice),
      mFormat(aFormat),
      mFlags(TextureFlags::NO_FLAGS),
      mCurrentTile(0),
      mIsTiled(false),
      mIterating(false),
      mAllowTextureUploads(false) {
  mTexture = aTexture;
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  mSize = IntSize(desc.Width, desc.Height);
}

DataTextureSourceD3D11::DataTextureSourceD3D11(gfx::SurfaceFormat aFormat,
                                               TextureSourceProvider* aProvider,
                                               ID3D11Texture2D* aTexture)
    : DataTextureSourceD3D11(aProvider->GetD3D11Device(), aFormat, aTexture) {}

DataTextureSourceD3D11::DataTextureSourceD3D11(gfx::SurfaceFormat aFormat,
                                               TextureSourceProvider* aProvider,
                                               TextureFlags aFlags)
    : DataTextureSourceD3D11(aProvider->GetD3D11Device(), aFormat, aFlags) {}

DataTextureSourceD3D11::~DataTextureSourceD3D11() {}

enum class SerializeWithMoz2D : bool { No, Yes };

template <typename T>  // ID3D10Texture2D or ID3D11Texture2D
static bool LockD3DTexture(
    T* aTexture, SerializeWithMoz2D aSerialize = SerializeWithMoz2D::No) {
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  // Textures created by the DXVA decoders don't have a mutex for
  // synchronization
  if (mutex) {
    HRESULT hr;
    if (aSerialize == SerializeWithMoz2D::Yes) {
      AutoSerializeWithMoz2D serializeWithMoz2D(BackendType::DIRECT2D1_1);
      hr = mutex->AcquireSync(0, 10000);
    } else {
      hr = mutex->AcquireSync(0, 10000);
    }
    if (hr == WAIT_TIMEOUT) {
      RefPtr<ID3D11Device> device;
      aTexture->GetDevice(getter_AddRefs(device));
      if (!device) {
        gfxCriticalNote << "GFX: D3D11 lock mutex timeout - no device returned";
      } else if (device->GetDeviceRemovedReason() != S_OK) {
        gfxCriticalNote << "GFX: D3D11 lock mutex timeout - device removed";
      } else {
        gfxDevCrash(LogReason::D3DLockTimeout)
            << "D3D lock mutex timeout - device not removed";
      }
    } else if (hr == WAIT_ABANDONED) {
      gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
    }

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock the texture");
      return false;
    }
  }
  return true;
}

template <typename T>
static bool HasKeyedMutex(T* aTexture) {
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  return !!mutex;
}

template <typename T>  // ID3D10Texture2D or ID3D11Texture2D
static void UnlockD3DTexture(
    T* aTexture, SerializeWithMoz2D aSerialize = SerializeWithMoz2D::No) {
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  if (mutex) {
    HRESULT hr;
    if (aSerialize == SerializeWithMoz2D::Yes) {
      AutoSerializeWithMoz2D serializeWithMoz2D(BackendType::DIRECT2D1_1);
      hr = mutex->ReleaseSync(0);
    } else {
      hr = mutex->ReleaseSync(0);
    }
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }
}

D3D11TextureData::D3D11TextureData(ID3D11Texture2D* aTexture,
                                   gfx::IntSize aSize,
                                   gfx::SurfaceFormat aFormat,
                                   TextureAllocationFlags aFlags)
    : mSize(aSize),
      mFormat(aFormat),
      mNeedsClear(aFlags & ALLOC_CLEAR_BUFFER),
      mHasSynchronization(HasKeyedMutex(aTexture)),
      mTexture(aTexture),
      mAllocationFlags(aFlags) {
  MOZ_ASSERT(aTexture);
}

static void DestroyDrawTarget(RefPtr<DrawTarget>& aDT,
                              RefPtr<ID3D11Texture2D>& aTexture) {
  // An Azure DrawTarget needs to be locked when it gets nullptr'ed as this is
  // when it calls EndDraw. This EndDraw should not execute anything so it
  // shouldn't -really- need the lock but the debug layer chokes on this.
  LockD3DTexture(aTexture.get(), SerializeWithMoz2D::Yes);
  aDT = nullptr;

  // Do the serialization here, so we can hold it while destroying the texture.
  AutoSerializeWithMoz2D serializeWithMoz2D(BackendType::DIRECT2D1_1);
  UnlockD3DTexture(aTexture.get(), SerializeWithMoz2D::No);
  aTexture = nullptr;
}

D3D11TextureData::~D3D11TextureData() {
  if (mDrawTarget) {
    DestroyDrawTarget(mDrawTarget, mTexture);
  }
}

bool D3D11TextureData::Lock(OpenMode aMode) {
  if (!LockD3DTexture(mTexture.get(), SerializeWithMoz2D::Yes)) {
    return false;
  }

  if (NS_IsMainThread()) {
    if (!PrepareDrawTargetInLock(aMode)) {
      Unlock();
      return false;
    }
  }

  return true;
}

bool D3D11TextureData::PrepareDrawTargetInLock(OpenMode aMode) {
  // Make sure that successful write-lock means we will have a DrawTarget to
  // write into.
  if (!mDrawTarget && (aMode & OpenMode::OPEN_WRITE || mNeedsClear)) {
    mDrawTarget = BorrowDrawTarget();
    if (!mDrawTarget) {
      return false;
    }
  }

  // Reset transform
  mDrawTarget->SetTransform(Matrix());

  if (mNeedsClear) {
    mDrawTarget->ClearRect(Rect(0, 0, mSize.width, mSize.height));
    mNeedsClear = false;
  }

  return true;
}

void D3D11TextureData::Unlock() {
  UnlockD3DTexture(mTexture.get(), SerializeWithMoz2D::Yes);
}

void D3D11TextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = true;
  aInfo.hasSynchronization = mHasSynchronization;
}

void D3D11TextureData::SyncWithObject(RefPtr<SyncObjectClient> aSyncObject) {
  if (!aSyncObject || mHasSynchronization) {
    // When we have per texture synchronization we sync using the keyed mutex.
    return;
  }

  MOZ_ASSERT(aSyncObject->GetSyncType() == SyncObjectClient::SyncType::D3D11);
  SyncObjectD3D11Client* sync =
      static_cast<SyncObjectD3D11Client*>(aSyncObject.get());
  sync->RegisterTexture(mTexture);
}

bool D3D11TextureData::SerializeSpecific(
    SurfaceDescriptorD3D10* const aOutDesc) {
  RefPtr<IDXGIResource> resource;
  GetDXGIResource((IDXGIResource**)getter_AddRefs(resource));
  if (!resource) {
    return false;
  }
  HANDLE sharedHandle;
  HRESULT hr = resource->GetSharedHandle(&sharedHandle);
  if (FAILED(hr)) {
    LOGD3D11("Error getting shared handle for texture.");
    return false;
  }

  *aOutDesc = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle, mFormat,
                                     mSize, mYUVColorSpace, mColorRange);
  return true;
}

bool D3D11TextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  SurfaceDescriptorD3D10 desc;
  if (!SerializeSpecific(&desc)) return false;

  aOutDescriptor = std::move(desc);
  return true;
}

void D3D11TextureData::GetSubDescriptor(
    RemoteDecoderVideoSubDescriptor* const aOutDesc) {
  SurfaceDescriptorD3D10 ret;
  if (!SerializeSpecific(&ret)) return;

  *aOutDesc = std::move(ret);
}

D3D11TextureData* D3D11TextureData::Create(IntSize aSize, SurfaceFormat aFormat,
                                           TextureAllocationFlags aFlags,
                                           ID3D11Device* aDevice) {
  return Create(aSize, aFormat, nullptr, aFlags, aDevice);
}

D3D11TextureData* D3D11TextureData::Create(SourceSurface* aSurface,
                                           TextureAllocationFlags aFlags,
                                           ID3D11Device* aDevice) {
  return Create(aSurface->GetSize(), aSurface->GetFormat(), aSurface, aFlags,
                aDevice);
}

D3D11TextureData* D3D11TextureData::Create(IntSize aSize, SurfaceFormat aFormat,
                                           SourceSurface* aSurface,
                                           TextureAllocationFlags aFlags,
                                           ID3D11Device* aDevice) {
  if (aFormat == SurfaceFormat::A8) {
    // Currently we don't support A8 surfaces. Fallback.
    return nullptr;
  }

  // Just grab any device. We never use the immediate context, so the devices
  // are fine to use from any thread.
  RefPtr<ID3D11Device> device = aDevice;
  if (!device) {
    device = DeviceManagerDx::Get()->GetContentDevice();
    if (!device) {
      return nullptr;
    }
  }

  CD3D11_TEXTURE2D_DESC newDesc(
      DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width, aSize.height, 1, 1,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

  if (aFormat == SurfaceFormat::NV12) {
    newDesc.Format = DXGI_FORMAT_NV12;
  } else if (aFormat == SurfaceFormat::P010) {
    newDesc.Format = DXGI_FORMAT_P010;
  } else if (aFormat == SurfaceFormat::P016) {
    newDesc.Format = DXGI_FORMAT_P016;
  }

  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  if (!NS_IsMainThread()) {
    // On the main thread we use the syncobject to handle synchronization.
    if (!(aFlags & ALLOC_MANUAL_SYNCHRONIZATION)) {
      newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    }
  }

  if (aSurface && newDesc.MiscFlags == D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX &&
      !DeviceManagerDx::Get()->CanInitializeKeyedMutexTextures()) {
    return nullptr;
  }

  D3D11_SUBRESOURCE_DATA uploadData;
  D3D11_SUBRESOURCE_DATA* uploadDataPtr = nullptr;
  RefPtr<DataSourceSurface> srcSurf;
  DataSourceSurface::MappedSurface sourceMap;

  if (aSurface) {
    srcSurf = aSurface->GetDataSurface();

    if (!srcSurf) {
      gfxCriticalError()
          << "Failed to GetDataSurface in D3D11TextureData::Create";
      return nullptr;
    }

    if (!srcSurf->Map(DataSourceSurface::READ, &sourceMap)) {
      gfxCriticalError()
          << "Failed to map source surface for D3D11TextureData::Create";
      return nullptr;
    }
  }

  if (srcSurf && !DeviceManagerDx::Get()->HasCrashyInitData()) {
    uploadData.pSysMem = sourceMap.mData;
    uploadData.SysMemPitch = sourceMap.mStride;
    uploadData.SysMemSlicePitch = 0;  // unused

    uploadDataPtr = &uploadData;
  }

  // See bug 1397040
  RefPtr<ID3D10Multithread> mt;
  device->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  RefPtr<ID3D11Texture2D> texture11;

  {
    AutoSerializeWithMoz2D serializeWithMoz2D(BackendType::DIRECT2D1_1);
    D3D11MTAutoEnter lock(mt.forget());

    HRESULT hr = device->CreateTexture2D(&newDesc, uploadDataPtr,
                                         getter_AddRefs(texture11));

    if (FAILED(hr) || !texture11) {
      gfxCriticalNote << "[D3D11] 2 CreateTexture2D failure Size: " << aSize
                      << "texture11: " << texture11
                      << " Code: " << gfx::hexa(hr);
      return nullptr;
    }

    if (srcSurf && DeviceManagerDx::Get()->HasCrashyInitData()) {
      D3D11_BOX box;
      box.front = box.top = box.left = 0;
      box.back = 1;
      box.right = aSize.width;
      box.bottom = aSize.height;
      RefPtr<ID3D11DeviceContext> ctx;
      device->GetImmediateContext(getter_AddRefs(ctx));
      ctx->UpdateSubresource(texture11, 0, &box, sourceMap.mData,
                             sourceMap.mStride, 0);
    }
  }

  if (srcSurf) {
    srcSurf->Unmap();
  }

  // If we created the texture with a keyed mutex, then we expect all operations
  // on it to be synchronized using it. If we did an initial upload using
  // aSurface then bizarely this isn't covered, so we insert a manual
  // lock/unlock pair to force this.
  if (aSurface && newDesc.MiscFlags == D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
    if (!LockD3DTexture(texture11.get(), SerializeWithMoz2D::Yes)) {
      return nullptr;
    }
    UnlockD3DTexture(texture11.get(), SerializeWithMoz2D::Yes);
  }
  texture11->SetPrivateDataInterface(
      sD3D11TextureUsage,
      new TextureMemoryMeasurer(newDesc.Width * newDesc.Height * 4));
  return new D3D11TextureData(texture11, aSize, aFormat, aFlags);
}

void D3D11TextureData::Deallocate(LayersIPCChannel* aAllocator) {
  mDrawTarget = nullptr;
  mTexture = nullptr;
}

TextureData* D3D11TextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  return D3D11TextureData::Create(mSize, mFormat, aAllocFlags);
}

void D3D11TextureData::GetDXGIResource(IDXGIResource** aOutResource) {
  mTexture->QueryInterface(aOutResource);
}

TextureFlags D3D11TextureData::GetTextureFlags() const {
  TextureFlags flags = TextureFlags::NO_FLAGS;
  // With WebRender, resource open happens asynchronously on RenderThread.
  // During opening the resource on host side, TextureClient needs to be alive.
  // With WAIT_HOST_USAGE_END, keep TextureClient alive during host side usage.
  if (gfx::gfxVars::UseWebRender()) {
    flags |= TextureFlags::WAIT_HOST_USAGE_END;
  }
  return flags;
}

DXGIYCbCrTextureData* DXGIYCbCrTextureData::Create(
    IDirect3DTexture9* aTextureY, IDirect3DTexture9* aTextureCb,
    IDirect3DTexture9* aTextureCr, HANDLE aHandleY, HANDLE aHandleCb,
    HANDLE aHandleCr, const gfx::IntSize& aSize, const gfx::IntSize& aSizeY,
    const gfx::IntSize& aSizeCbCr, gfx::ColorDepth aColorDepth,
    YUVColorSpace aYUVColorSpace, gfx::ColorRange aColorRange) {
  if (!aHandleY || !aHandleCb || !aHandleCr || !aTextureY || !aTextureCb ||
      !aTextureCr) {
    return nullptr;
  }

  DXGIYCbCrTextureData* texture = new DXGIYCbCrTextureData();
  texture->mHandles[0] = aHandleY;
  texture->mHandles[1] = aHandleCb;
  texture->mHandles[2] = aHandleCr;
  texture->mD3D9Textures[0] = aTextureY;
  texture->mD3D9Textures[1] = aTextureCb;
  texture->mD3D9Textures[2] = aTextureCr;
  texture->mSize = aSize;
  texture->mSizeY = aSizeY;
  texture->mSizeCbCr = aSizeCbCr;
  texture->mColorDepth = aColorDepth;
  texture->mYUVColorSpace = aYUVColorSpace;
  texture->mColorRange = aColorRange;

  return texture;
}

DXGIYCbCrTextureData* DXGIYCbCrTextureData::Create(
    ID3D11Texture2D* aTextureY, ID3D11Texture2D* aTextureCb,
    ID3D11Texture2D* aTextureCr, const gfx::IntSize& aSize,
    const gfx::IntSize& aSizeY, const gfx::IntSize& aSizeCbCr,
    gfx::ColorDepth aColorDepth, YUVColorSpace aYUVColorSpace,
    gfx::ColorRange aColorRange) {
  if (!aTextureY || !aTextureCb || !aTextureCr) {
    return nullptr;
  }

  aTextureY->SetPrivateDataInterface(
      sD3D11TextureUsage,
      new TextureMemoryMeasurer(aSizeY.width * aSizeY.height));
  aTextureCb->SetPrivateDataInterface(
      sD3D11TextureUsage,
      new TextureMemoryMeasurer(aSizeCbCr.width * aSizeCbCr.height));
  aTextureCr->SetPrivateDataInterface(
      sD3D11TextureUsage,
      new TextureMemoryMeasurer(aSizeCbCr.width * aSizeCbCr.height));

  RefPtr<IDXGIResource> resource;

  aTextureY->QueryInterface((IDXGIResource**)getter_AddRefs(resource));

  HANDLE handleY;
  HRESULT hr = resource->GetSharedHandle(&handleY);
  if (FAILED(hr)) {
    return nullptr;
  }

  aTextureCb->QueryInterface((IDXGIResource**)getter_AddRefs(resource));

  HANDLE handleCb;
  hr = resource->GetSharedHandle(&handleCb);
  if (FAILED(hr)) {
    return nullptr;
  }

  aTextureCr->QueryInterface((IDXGIResource**)getter_AddRefs(resource));
  HANDLE handleCr;
  hr = resource->GetSharedHandle(&handleCr);
  if (FAILED(hr)) {
    return nullptr;
  }

  DXGIYCbCrTextureData* texture = new DXGIYCbCrTextureData();
  texture->mHandles[0] = handleY;
  texture->mHandles[1] = handleCb;
  texture->mHandles[2] = handleCr;
  texture->mD3D11Textures[0] = aTextureY;
  texture->mD3D11Textures[1] = aTextureCb;
  texture->mD3D11Textures[2] = aTextureCr;
  texture->mSize = aSize;
  texture->mSizeY = aSizeY;
  texture->mSizeCbCr = aSizeCbCr;
  texture->mColorDepth = aColorDepth;
  texture->mYUVColorSpace = aYUVColorSpace;
  texture->mColorRange = aColorRange;

  return texture;
}

void DXGIYCbCrTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = gfx::SurfaceFormat::YUV;
  aInfo.supportsMoz2D = false;
  aInfo.hasSynchronization = false;
}

void DXGIYCbCrTextureData::SerializeSpecific(
    SurfaceDescriptorDXGIYCbCr* const aOutDesc) {
  *aOutDesc = SurfaceDescriptorDXGIYCbCr(
      (WindowsHandle)mHandles[0], (WindowsHandle)mHandles[1],
      (WindowsHandle)mHandles[2], mSize, mSizeY, mSizeCbCr, mColorDepth,
      mYUVColorSpace, mColorRange);
}

bool DXGIYCbCrTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  SurfaceDescriptorDXGIYCbCr desc;
  SerializeSpecific(&desc);

  aOutDescriptor = std::move(desc);
  return true;
}

void DXGIYCbCrTextureData::GetSubDescriptor(
    RemoteDecoderVideoSubDescriptor* const aOutDesc) {
  SurfaceDescriptorDXGIYCbCr desc;
  SerializeSpecific(&desc);

  *aOutDesc = std::move(desc);
}

void DXGIYCbCrTextureData::Deallocate(LayersIPCChannel*) {
  mD3D9Textures[0] = nullptr;
  mD3D9Textures[1] = nullptr;
  mD3D9Textures[2] = nullptr;
  mD3D11Textures[0] = nullptr;
  mD3D11Textures[1] = nullptr;
  mD3D11Textures[2] = nullptr;
}

TextureFlags DXGIYCbCrTextureData::GetTextureFlags() const {
  TextureFlags flags = TextureFlags::NO_FLAGS;
  // With WebRender, resource open happens asynchronously on RenderThread.
  // During opening the resource on host side, TextureClient needs to be alive.
  // With WAIT_HOST_USAGE_END, keep TextureClient alive during host side usage.
  if (gfx::gfxVars::UseWebRender()) {
    flags |= TextureFlags::WAIT_HOST_USAGE_END;
  }
  return flags;
}

already_AddRefed<TextureHost> CreateTextureHostD3D11(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags) {
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      result =
          new DXGITextureHostD3D11(aFlags, aDesc.get_SurfaceDescriptorD3D10());
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr: {
      result = new DXGIYCbCrTextureHostD3D11(
          aFlags, aDesc.get_SurfaceDescriptorDXGIYCbCr());
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("Unsupported SurfaceDescriptor type");
    }
  }
  return result.forget();
}

already_AddRefed<DrawTarget> D3D11TextureData::BorrowDrawTarget() {
  MOZ_ASSERT(NS_IsMainThread() || NS_IsInCanvasThreadOrWorker());

  if (!mDrawTarget && mTexture) {
    // This may return a null DrawTarget
    mDrawTarget = Factory::CreateDrawTargetForD3D11Texture(mTexture, mFormat);
    if (!mDrawTarget) {
      gfxCriticalNote << "Could not borrow DrawTarget (D3D11) " << (int)mFormat;
    }
  }

  RefPtr<DrawTarget> result = mDrawTarget;
  return result.forget();
}

bool D3D11TextureData::UpdateFromSurface(gfx::SourceSurface* aSurface) {
  // Supporting texture updates after creation requires an ID3D11DeviceContext
  // and those aren't threadsafe. We'd need to either lock, or have a device for
  // whatever thread this runs on and we're trying to avoid extra devices (bug
  // 1284672).
  MOZ_ASSERT(false,
             "UpdateFromSurface not supported for D3D11! Use CreateFromSurface "
             "instead");
  return false;
}

DXGITextureHostD3D11::DXGITextureHostD3D11(
    TextureFlags aFlags, const SurfaceDescriptorD3D10& aDescriptor)
    : TextureHost(aFlags),
      mSize(aDescriptor.size()),
      mHandle(aDescriptor.handle()),
      mFormat(aDescriptor.format()),
      mYUVColorSpace(aDescriptor.yUVColorSpace()),
      mColorRange(aDescriptor.colorRange()),
      mIsLocked(false) {}

bool DXGITextureHostD3D11::EnsureTexture() {
  RefPtr<ID3D11Device> device;
  if (mTexture) {
    mTexture->GetDevice(getter_AddRefs(device));
    if (device == DeviceManagerDx::Get()->GetCompositorDevice()) {
      NS_WARNING("Incompatible texture.");
      return true;
    }
    mTexture = nullptr;
  }

  device = GetDevice();
  if (!device || device != DeviceManagerDx::Get()->GetCompositorDevice()) {
    NS_WARNING("No device or incompatible device.");
    return false;
  }

  HRESULT hr = device->OpenSharedResource(
      (HANDLE)mHandle, __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture));
  if (FAILED(hr)) {
    MOZ_ASSERT(false, "Failed to open shared texture");
    return false;
  }

  D3D11_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);
  mSize = IntSize(desc.Width, desc.Height);
  return true;
}

RefPtr<ID3D11Device> DXGITextureHostD3D11::GetDevice() {
  if (mFlags & TextureFlags::INVALID_COMPOSITOR) {
    return nullptr;
  }

  return mDevice;
}

bool DXGITextureHostD3D11::LockWithoutCompositor() {
  if (!mDevice) {
    mDevice = DeviceManagerDx::Get()->GetCompositorDevice();
  }
  return LockInternal();
}

void DXGITextureHostD3D11::UnlockWithoutCompositor() { UnlockInternal(); }

bool DXGITextureHostD3D11::LockInternal() {
  if (!GetDevice()) {
    NS_WARNING("trying to lock a TextureHost without a D3D device");
    return false;
  }

  if (!EnsureTextureSource()) {
    return false;
  }

  mIsLocked = LockD3DTexture(mTextureSource->GetD3D11Texture());

  return mIsLocked;
}

already_AddRefed<gfx::DataSourceSurface> DXGITextureHostD3D11::GetAsSurface() {
  if (!gfxVars::UseWebRender()) {
    return nullptr;
  }

  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
      break;
    default: {
      MOZ_ASSERT_UNREACHABLE("DXGITextureHostD3D11: unsupported format!");
      return nullptr;
    }
  }

  AutoLockTextureHostWithoutCompositor autoLock(this);
  if (autoLock.Failed()) {
    NS_WARNING("Failed to lock the D3DTexture");
    return nullptr;
  }

  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(getter_AddRefs(device));

  D3D11_TEXTURE2D_DESC textureDesc = {0};
  mTexture->GetDesc(&textureDesc);

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));

  textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  textureDesc.Usage = D3D11_USAGE_STAGING;
  textureDesc.BindFlags = 0;
  textureDesc.MiscFlags = 0;
  textureDesc.MipLevels = 1;
  RefPtr<ID3D11Texture2D> cpuTexture;
  HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr,
                                       getter_AddRefs(cpuTexture));
  if (FAILED(hr)) {
    return nullptr;
  }

  context->CopyResource(cpuTexture, mTexture);

  D3D11_MAPPED_SUBRESOURCE mappedSubresource;
  hr = context->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mappedSubresource);
  if (FAILED(hr)) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> surf = gfx::CreateDataSourceSurfaceFromData(
      IntSize(textureDesc.Width, textureDesc.Height), GetFormat(),
      (uint8_t*)mappedSubresource.pData, mappedSubresource.RowPitch);
  context->Unmap(cpuTexture, 0);
  return surf.forget();
}

bool DXGITextureHostD3D11::EnsureTextureSource() {
  if (mTextureSource) {
    return true;
  }

  if (!EnsureTexture()) {
    DeviceManagerDx::Get()->ForceDeviceReset(
        ForcedDeviceResetReason::OPENSHAREDHANDLE);
    return false;
  }

  mTextureSource = new DataTextureSourceD3D11(mDevice, mFormat, mTexture);
  return true;
}

void DXGITextureHostD3D11::UnlockInternal() {
  UnlockD3DTexture(mTextureSource->GetD3D11Texture());
}

void DXGITextureHostD3D11::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  RefPtr<wr::RenderTextureHost> texture = new wr::RenderDXGITextureHost(
      mHandle, mFormat, mYUVColorSpace, mColorRange, mSize);

  wr::RenderThread::Get()->RegisterExternalImage(aExternalImageId,
                                                 texture.forget());
}

uint32_t DXGITextureHostD3D11::NumSubTextures() {
  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      return 1;
    }
    case gfx::SurfaceFormat::NV12:
    case gfx::SurfaceFormat::P010:
    case gfx::SurfaceFormat::P016: {
      return 2;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected format");
      return 1;
    }
  }
}

void DXGITextureHostD3D11::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called without ANGLE");
    return;
  }

  MOZ_ASSERT(mHandle);
  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;
  switch (mFormat) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      MOZ_ASSERT(aImageKeys.length() == 1);

      wr::ImageDescriptor descriptor(mSize, GetFormat());
      // Prefer TextureExternal unless the backend requires TextureRect.
      TextureHost::NativeTexturePolicy policy =
          TextureHost::BackendNativeTexturePolicy(aResources.GetBackendType(),
                                                  mSize);
      auto imageType = policy == TextureHost::NativeTexturePolicy::REQUIRE
                           ? wr::ExternalImageType::TextureHandle(
                                 wr::ImageBufferKind::TextureRect)
                           : wr::ExternalImageType::TextureHandle(
                                 wr::ImageBufferKind::TextureExternal);
      (aResources.*method)(aImageKeys[0], descriptor, aExtID, imageType, 0);
      break;
    }
    case gfx::SurfaceFormat::P010:
    case gfx::SurfaceFormat::P016:
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      MOZ_ASSERT(mSize.width % 2 == 0);
      MOZ_ASSERT(mSize.height % 2 == 0);

      wr::ImageDescriptor descriptor0(mSize, mFormat == gfx::SurfaceFormat::NV12
                                                 ? gfx::SurfaceFormat::A8
                                                 : gfx::SurfaceFormat::A16);
      wr::ImageDescriptor descriptor1(mSize / 2,
                                      mFormat == gfx::SurfaceFormat::NV12
                                          ? gfx::SurfaceFormat::R8G8
                                          : gfx::SurfaceFormat::R16G16);
      // Prefer TextureExternal unless the backend requires TextureRect.
      TextureHost::NativeTexturePolicy policy =
          TextureHost::BackendNativeTexturePolicy(aResources.GetBackendType(),
                                                  mSize);
      auto imageType = policy == TextureHost::NativeTexturePolicy::REQUIRE
                           ? wr::ExternalImageType::TextureHandle(
                                 wr::ImageBufferKind::TextureRect)
                           : wr::ExternalImageType::TextureHandle(
                                 wr::ImageBufferKind::TextureExternal);
      (aResources.*method)(aImageKeys[0], descriptor0, aExtID, imageType, 0);
      (aResources.*method)(aImageKeys[1], descriptor1, aExtID, imageType, 1);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

void DXGITextureHostD3D11::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  bool preferCompositorSurface =
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE);
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called without ANGLE");
    return;
  }

  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      aBuilder.PushImage(
          aBounds, aClip, true, aFilter, aImageKeys[0],
          !(mFlags & TextureFlags::NON_PREMULTIPLIED),
          wr::ColorF{1.0f, 1.0f, 1.0f, 1.0f}, preferCompositorSurface,
          SupportsExternalCompositing(aBuilder.GetBackendType()));
      break;
    }
    case gfx::SurfaceFormat::P010:
    case gfx::SurfaceFormat::P016:
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      aBuilder.PushNV12Image(
          aBounds, aClip, true, aImageKeys[0], aImageKeys[1],
          GetFormat() == gfx::SurfaceFormat::NV12 ? wr::ColorDepth::Color8
                                                  : wr::ColorDepth::Color16,
          wr::ToWrYuvColorSpace(mYUVColorSpace),
          wr::ToWrColorRange(mColorRange), aFilter, preferCompositorSurface,
          SupportsExternalCompositing(aBuilder.GetBackendType()));
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

bool DXGITextureHostD3D11::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  if (aBackend == WebRenderBackend::SOFTWARE) {
    return true;
  }
  // XXX Add P010 and P016 support.
  if (GetFormat() == gfx::SurfaceFormat::NV12 &&
      gfx::gfxVars::UseWebRenderDCompVideoOverlayWin()) {
    return true;
  }
  return false;
}

DXGIYCbCrTextureHostD3D11::DXGIYCbCrTextureHostD3D11(
    TextureFlags aFlags, const SurfaceDescriptorDXGIYCbCr& aDescriptor)
    : TextureHost(aFlags),
      mSize(aDescriptor.size()),
      mSizeY(aDescriptor.sizeY()),
      mSizeCbCr(aDescriptor.sizeCbCr()),
      mIsLocked(false),
      mColorDepth(aDescriptor.colorDepth()),
      mYUVColorSpace(aDescriptor.yUVColorSpace()),
      mColorRange(aDescriptor.colorRange()) {
  mHandles[0] = aDescriptor.handleY();
  mHandles[1] = aDescriptor.handleCb();
  mHandles[2] = aDescriptor.handleCr();
}

bool DXGIYCbCrTextureHostD3D11::EnsureTexture() {
  RefPtr<ID3D11Device> device;
  if (mTextures[0]) {
    mTextures[0]->GetDevice(getter_AddRefs(device));
    if (device == DeviceManagerDx::Get()->GetCompositorDevice()) {
      NS_WARNING("Incompatible texture.");
      return true;
    }
    mTextures[0] = nullptr;
    mTextures[1] = nullptr;
    mTextures[2] = nullptr;
  }

  if (!GetDevice() ||
      GetDevice() != DeviceManagerDx::Get()->GetCompositorDevice()) {
    NS_WARNING("No device or incompatible device.");
    return false;
  }

  device = GetDevice();
  RefPtr<ID3D11Texture2D> textures[3];

  HRESULT hr = device->OpenSharedResource(
      (HANDLE)mHandles[0], __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(textures[0]));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture for Y Plane");
    return false;
  }

  hr = device->OpenSharedResource(
      (HANDLE)mHandles[1], __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(textures[1]));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture for Cb Plane");
    return false;
  }

  hr = device->OpenSharedResource(
      (HANDLE)mHandles[2], __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(textures[2]));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture for Cr Plane");
    return false;
  }

  mTextures[0] = textures[0].forget();
  mTextures[1] = textures[1].forget();
  mTextures[2] = textures[2].forget();

  return true;
}

RefPtr<ID3D11Device> DXGIYCbCrTextureHostD3D11::GetDevice() { return nullptr; }

bool DXGIYCbCrTextureHostD3D11::EnsureTextureSource() { return false; }

void DXGIYCbCrTextureHostD3D11::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  RefPtr<wr::RenderTextureHost> texture = new wr::RenderDXGIYCbCrTextureHost(
      mHandles, mYUVColorSpace, mColorDepth, mColorRange, mSizeY, mSizeCbCr);

  wr::RenderThread::Get()->RegisterExternalImage(aExternalImageId,
                                                 texture.forget());
}

uint32_t DXGIYCbCrTextureHostD3D11::NumSubTextures() {
  // ycbcr use 3 sub textures.
  return 3;
}

void DXGIYCbCrTextureHostD3D11::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called without ANGLE");
    return;
  }

  MOZ_ASSERT(mHandles[0] && mHandles[1] && mHandles[2]);
  MOZ_ASSERT(aImageKeys.length() == 3);
  // Assume the chroma planes are rounded up if the luma plane is odd sized.
  MOZ_ASSERT((mSizeCbCr.width == mSizeY.width ||
              mSizeCbCr.width == (mSizeY.width + 1) >> 1) &&
             (mSizeCbCr.height == mSizeY.height ||
              mSizeCbCr.height == (mSizeY.height + 1) >> 1));

  auto method = aOp == TextureHost::ADD_IMAGE
                    ? &wr::TransactionBuilder::AddExternalImage
                    : &wr::TransactionBuilder::UpdateExternalImage;

  // Prefer TextureExternal unless the backend requires TextureRect.
  // Use a size that is the maximum of the Y and CbCr sizes.
  IntSize textureSize = std::max(mSizeY, mSizeCbCr);
  TextureHost::NativeTexturePolicy policy =
      TextureHost::BackendNativeTexturePolicy(aResources.GetBackendType(),
                                              textureSize);
  auto imageType = policy == TextureHost::NativeTexturePolicy::REQUIRE
                       ? wr::ExternalImageType::TextureHandle(
                             wr::ImageBufferKind::TextureRect)
                       : wr::ExternalImageType::TextureHandle(
                             wr::ImageBufferKind::TextureExternal);

  // y
  wr::ImageDescriptor descriptor0(mSizeY, gfx::SurfaceFormat::A8);
  // cb and cr
  wr::ImageDescriptor descriptor1(mSizeCbCr, gfx::SurfaceFormat::A8);
  (aResources.*method)(aImageKeys[0], descriptor0, aExtID, imageType, 0);
  (aResources.*method)(aImageKeys[1], descriptor1, aExtID, imageType, 1);
  (aResources.*method)(aImageKeys[2], descriptor1, aExtID, imageType, 2);
}

void DXGIYCbCrTextureHostD3D11::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  if (!gfx::gfxVars::UseWebRenderANGLE()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called without ANGLE");
    return;
  }

  MOZ_ASSERT(aImageKeys.length() == 3);

  aBuilder.PushYCbCrPlanarImage(
      aBounds, aClip, true, aImageKeys[0], aImageKeys[1], aImageKeys[2],
      wr::ToWrColorDepth(mColorDepth), wr::ToWrYuvColorSpace(mYUVColorSpace),
      wr::ToWrColorRange(mColorRange), aFilter,
      aFlags.contains(PushDisplayItemFlag::PREFER_COMPOSITOR_SURFACE),
      SupportsExternalCompositing(aBuilder.GetBackendType()));
}

bool DXGIYCbCrTextureHostD3D11::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  return aBackend == WebRenderBackend::SOFTWARE;
}

bool DataTextureSourceD3D11::Update(DataSourceSurface* aSurface,
                                    nsIntRegion* aDestRegion,
                                    IntPoint* aSrcOffset,
                                    IntPoint* aDstOffset) {
  // Incremental update with a source offset is only used on Mac so it is not
  // clear that we ever will need to support it for D3D.
  MOZ_ASSERT(!aSrcOffset);
  MOZ_RELEASE_ASSERT(!aDstOffset);
  MOZ_ASSERT(aSurface);

  MOZ_ASSERT(mAllowTextureUploads);
  if (!mAllowTextureUploads) {
    return false;
  }

  HRESULT hr;

  if (!mDevice) {
    return false;
  }

  uint32_t bpp = BytesPerPixel(aSurface->GetFormat());
  DXGI_FORMAT dxgiFormat = SurfaceFormatToDXGIFormat(aSurface->GetFormat());

  mSize = aSurface->GetSize();
  mFormat = aSurface->GetFormat();

  CD3D11_TEXTURE2D_DESC desc(dxgiFormat, mSize.width, mSize.height, 1, 1);

  int32_t maxSize = GetMaxTextureSizeFromDevice(mDevice);
  if ((mSize.width <= maxSize && mSize.height <= maxSize) ||
      (mFlags & TextureFlags::DISALLOW_BIGIMAGE)) {
    if (mTexture) {
      D3D11_TEXTURE2D_DESC currentDesc;
      mTexture->GetDesc(&currentDesc);

      // Make sure there's no size mismatch, if there is, recreate.
      if (currentDesc.Width != mSize.width ||
          currentDesc.Height != mSize.height ||
          currentDesc.Format != dxgiFormat) {
        mTexture = nullptr;
        // Make sure we upload the whole surface.
        aDestRegion = nullptr;
      }
    }

    nsIntRegion* regionToUpdate = aDestRegion;
    if (!mTexture) {
      hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(mTexture));
      mIsTiled = false;
      if (FAILED(hr) || !mTexture) {
        Reset();
        return false;
      }

      if (mFlags & TextureFlags::COMPONENT_ALPHA) {
        regionToUpdate = nullptr;
      }
    }

    DataSourceSurface::MappedSurface map;
    if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) {
      gfxCriticalError() << "Failed to map surface.";
      Reset();
      return false;
    }

    RefPtr<ID3D11DeviceContext> context;
    mDevice->GetImmediateContext(getter_AddRefs(context));

    if (regionToUpdate) {
      for (auto iter = regionToUpdate->RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();
        D3D11_BOX box;
        box.front = 0;
        box.back = 1;
        box.left = rect.X();
        box.top = rect.Y();
        box.right = rect.XMost();
        box.bottom = rect.YMost();

        void* data = map.mData + map.mStride * rect.Y() +
                     BytesPerPixel(aSurface->GetFormat()) * rect.X();

        context->UpdateSubresource(mTexture, 0, &box, data, map.mStride,
                                   map.mStride * rect.Height());
      }
    } else {
      context->UpdateSubresource(mTexture, 0, nullptr, map.mData, map.mStride,
                                 map.mStride * mSize.height);
    }

    aSurface->Unmap();
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D11(mSize.width, maxSize) *
                         GetRequiredTilesD3D11(mSize.height, maxSize);

    mTileTextures.resize(tileCount);
    mTileSRVs.resize(tileCount);
    mTexture = nullptr;

    DataSourceSurface::ScopedMap map(aSurface, DataSourceSurface::READ);
    if (!map.IsMapped()) {
      gfxCriticalError() << "Failed to map surface.";
      Reset();
      return false;
    }

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);

      desc.Width = tileRect.Width();
      desc.Height = tileRect.Height();
      desc.Usage = D3D11_USAGE_IMMUTABLE;

      D3D11_SUBRESOURCE_DATA initData;
      initData.pSysMem =
          map.GetData() + tileRect.Y() * map.GetStride() + tileRect.X() * bpp;
      initData.SysMemPitch = map.GetStride();

      hr = mDevice->CreateTexture2D(&desc, &initData,
                                    getter_AddRefs(mTileTextures[i]));
      if (FAILED(hr) || !mTileTextures[i]) {
        Reset();
        return false;
      }
    }
  }
  return true;
}

ID3D11Texture2D* DataTextureSourceD3D11::GetD3D11Texture() const {
  return mIterating ? mTileTextures[mCurrentTile] : mTexture;
}

RefPtr<TextureSource> DataTextureSourceD3D11::ExtractCurrentTile() {
  MOZ_ASSERT(mIterating);
  return new DataTextureSourceD3D11(mDevice, mFormat,
                                    mTileTextures[mCurrentTile]);
}

ID3D11ShaderResourceView* DataTextureSourceD3D11::GetShaderResourceView() {
  if (mIterating) {
    if (!mTileSRVs[mCurrentTile]) {
      if (!mTileTextures[mCurrentTile]) {
        return nullptr;
      }

      RefPtr<ID3D11Device> device;
      mTileTextures[mCurrentTile]->GetDevice(getter_AddRefs(device));
      HRESULT hr = device->CreateShaderResourceView(
          mTileTextures[mCurrentTile], nullptr,
          getter_AddRefs(mTileSRVs[mCurrentTile]));
      if (FAILED(hr)) {
        gfxCriticalNote
            << "[D3D11] DataTextureSourceD3D11:GetShaderResourceView CreateSRV "
               "failure "
            << gfx::hexa(hr);
        return nullptr;
      }
    }
    return mTileSRVs[mCurrentTile];
  }

  return TextureSourceD3D11::GetShaderResourceView();
}

void DataTextureSourceD3D11::Reset() {
  mTexture = nullptr;
  mTileSRVs.resize(0);
  mTileTextures.resize(0);
  mIsTiled = false;
  mSize.width = 0;
  mSize.height = 0;
}

IntRect DataTextureSourceD3D11::GetTileRect(uint32_t aIndex) const {
  return GetTileRectD3D11(aIndex, mSize, GetMaxTextureSizeFromDevice(mDevice));
}

IntRect DataTextureSourceD3D11::GetTileRect() {
  IntRect rect = GetTileRect(mCurrentTile);
  return IntRect(rect.X(), rect.Y(), rect.Width(), rect.Height());
}

CompositingRenderTargetD3D11::CompositingRenderTargetD3D11(
    ID3D11Texture2D* aTexture, const gfx::IntPoint& aOrigin,
    DXGI_FORMAT aFormatOverride)
    : CompositingRenderTarget(aOrigin) {
  MOZ_ASSERT(aTexture);

  mTexture = aTexture;

  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(getter_AddRefs(device));

  mFormatOverride = aFormatOverride;

  // If we happen to have a typeless underlying DXGI surface, we need to be
  // explicit about the format here. (Such a surface could come from an external
  // source, such as the Oculus compositor)
  CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(D3D11_RTV_DIMENSION_TEXTURE2D,
                                         mFormatOverride);
  D3D11_RENDER_TARGET_VIEW_DESC* desc =
      aFormatOverride == DXGI_FORMAT_UNKNOWN ? nullptr : &rtvDesc;

  HRESULT hr =
      device->CreateRenderTargetView(mTexture, desc, getter_AddRefs(mRTView));

  if (FAILED(hr)) {
    LOGD3D11("Failed to create RenderTargetView.");
  }
}

void CompositingRenderTargetD3D11::BindRenderTarget(
    ID3D11DeviceContext* aContext) {
  if (mClearOnBind) {
    FLOAT clear[] = {0, 0, 0, 0};
    aContext->ClearRenderTargetView(mRTView, clear);
    mClearOnBind = false;
  }
  ID3D11RenderTargetView* view = mRTView;
  aContext->OMSetRenderTargets(1, &view, nullptr);
}

IntSize CompositingRenderTargetD3D11::GetSize() const {
  return TextureSourceD3D11::GetSize();
}

static inline bool ShouldDevCrashOnSyncInitFailure() {
  // Compositor shutdown does not wait for video decoding to finish, so it is
  // possible for the compositor to destroy the SyncObject before video has a
  // chance to initialize it.
  if (!NS_IsMainThread()) {
    return false;
  }

  // Note: CompositorIsInGPUProcess is a main-thread-only function.
  return !CompositorBridgeChild::CompositorIsInGPUProcess() &&
         !DeviceManagerDx::Get()->HasDeviceReset();
}

SyncObjectD3D11Host::SyncObjectD3D11Host(ID3D11Device* aDevice)
    : mSyncHandle(0), mDevice(aDevice) {
  MOZ_ASSERT(aDevice);
}

bool SyncObjectD3D11Host::Init() {
  CD3D11_TEXTURE2D_DESC desc(
      DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr =
      mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    gfxWarning() << "Could not create a sync texture: " << gfx::hexa(hr);
    return false;
  }

  hr = texture->QueryInterface((IDXGIResource**)getter_AddRefs(mSyncTexture));
  if (FAILED(hr) || !mSyncTexture) {
    gfxWarning() << "Could not QI sync texture: " << gfx::hexa(hr);
    return false;
  }

  hr = mSyncTexture->QueryInterface(
      (IDXGIKeyedMutex**)getter_AddRefs(mKeyedMutex));
  if (FAILED(hr) || !mKeyedMutex) {
    gfxWarning() << "Could not QI keyed-mutex: " << gfx::hexa(hr);
    return false;
  }

  hr = mSyncTexture->GetSharedHandle(&mSyncHandle);
  if (FAILED(hr) || !mSyncHandle) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "layers::SyncObjectD3D11Renderer::Init",
        []() -> void { Accumulate(Telemetry::D3D11_SYNC_HANDLE_FAILURE, 1); }));
    gfxWarning() << "Could not get sync texture shared handle: "
                 << gfx::hexa(hr);
    return false;
  }

  return true;
}

SyncHandle SyncObjectD3D11Host::GetSyncHandle() { return mSyncHandle; }

bool SyncObjectD3D11Host::Synchronize(bool aFallible) {
  HRESULT hr;
  AutoTextureLock lock(mKeyedMutex, hr, 10000);

  if (hr == WAIT_TIMEOUT) {
    hr = mDevice->GetDeviceRemovedReason();
    if (hr != S_OK) {
      // Since the timeout is related to the driver-removed. Return false for
      // error handling.
      gfxCriticalNote << "GFX: D3D11 timeout with device-removed:"
                      << gfx::hexa(hr);
    } else if (aFallible) {
      gfxCriticalNote << "GFX: D3D11 timeout on the D3D11 sync lock.";
    } else {
      // There is no driver-removed event. Crash with this timeout.
      MOZ_CRASH("GFX: D3D11 normal status timeout");
    }

    return false;
  }
  if (hr == WAIT_ABANDONED) {
    gfxCriticalNote << "GFX: AL_D3D11 abandoned sync";
  }

  return true;
}

SyncObjectD3D11Client::SyncObjectD3D11Client(SyncHandle aSyncHandle,
                                             ID3D11Device* aDevice)
    : mSyncLock("SyncObjectD3D11"), mSyncHandle(aSyncHandle), mDevice(aDevice) {
  MOZ_ASSERT(aDevice);
}

SyncObjectD3D11Client::SyncObjectD3D11Client(SyncHandle aSyncHandle)
    : mSyncLock("SyncObjectD3D11"), mSyncHandle(aSyncHandle) {}

bool SyncObjectD3D11Client::Init(ID3D11Device* aDevice, bool aFallible) {
  if (mKeyedMutex) {
    return true;
  }

  HRESULT hr = aDevice->OpenSharedResource(
      mSyncHandle, __uuidof(ID3D11Texture2D),
      (void**)(ID3D11Texture2D**)getter_AddRefs(mSyncTexture));
  if (FAILED(hr) || !mSyncTexture) {
    gfxCriticalNote << "Failed to OpenSharedResource for SyncObjectD3D11: "
                    << hexa(hr);
    if (!aFallible && ShouldDevCrashOnSyncInitFailure()) {
      gfxDevCrash(LogReason::D3D11FinalizeFrame)
          << "Without device reset: " << hexa(hr);
    }
    return false;
  }

  hr = mSyncTexture->QueryInterface(__uuidof(IDXGIKeyedMutex),
                                    getter_AddRefs(mKeyedMutex));
  if (FAILED(hr) || !mKeyedMutex) {
    // Leave both the critical error and MOZ_CRASH for now; the critical error
    // lets us "save" the hr value.  We will probably eventually replace this
    // with gfxDevCrash.
    if (!aFallible) {
      gfxCriticalError() << "Failed to get KeyedMutex (2): " << hexa(hr);
      MOZ_CRASH("GFX: Cannot get D3D11 KeyedMutex");
    } else {
      gfxCriticalNote << "Failed to get KeyedMutex (3): " << hexa(hr);
    }
    return false;
  }

  return true;
}

void SyncObjectD3D11Client::RegisterTexture(ID3D11Texture2D* aTexture) {
  mSyncedTextures.push_back(aTexture);
}

bool SyncObjectD3D11Client::IsSyncObjectValid() {
  MOZ_ASSERT(mDevice);
  return true;
}

// We have only 1 sync object. As a thing that somehow works,
// we copy each of the textures that need to be synced with the compositor
// into our sync object and only use a lock for this sync object.
// This way, we don't have to sync every texture we send to the compositor.
// We only have to do this once per transaction.
bool SyncObjectD3D11Client::Synchronize(bool aFallible) {
  MOZ_ASSERT(mDevice);
  // Since this can be called from either the Paint or Main thread.
  // We don't want this to race since we initialize the sync texture here
  // too.
  MutexAutoLock syncLock(mSyncLock);

  if (!mSyncedTextures.size()) {
    return true;
  }
  if (!Init(mDevice, aFallible)) {
    return false;
  }

  return SynchronizeInternal(mDevice, aFallible);
}

bool SyncObjectD3D11Client::SynchronizeInternal(ID3D11Device* aDevice,
                                                bool aFallible) {
  mSyncLock.AssertCurrentThreadOwns();

  HRESULT hr;
  AutoTextureLock lock(mKeyedMutex, hr, 20000);

  if (hr == WAIT_TIMEOUT) {
    if (DeviceManagerDx::Get()->HasDeviceReset()) {
      gfxWarning() << "AcquireSync timed out because of device reset.";
      return false;
    }
    if (aFallible) {
      gfxWarning() << "Timeout on the D3D11 sync lock.";
    } else {
      gfxDevCrash(LogReason::D3D11SyncLock)
          << "Timeout on the D3D11 sync lock.";
    }
    return false;
  }

  D3D11_BOX box;
  box.front = box.top = box.left = 0;
  box.back = box.bottom = box.right = 1;

  RefPtr<ID3D11DeviceContext> ctx;
  aDevice->GetImmediateContext(getter_AddRefs(ctx));

  for (auto iter = mSyncedTextures.begin(); iter != mSyncedTextures.end();
       iter++) {
    ctx->CopySubresourceRegion(mSyncTexture, 0, 0, 0, 0, *iter, 0, &box);
  }

  mSyncedTextures.clear();

  return true;
}

uint32_t GetMaxTextureSizeFromDevice(ID3D11Device* aDevice) {
  return GetMaxTextureSizeForFeatureLevel(aDevice->GetFeatureLevel());
}

AutoLockD3D11Texture::AutoLockD3D11Texture(ID3D11Texture2D* aTexture) {
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
  if (!mMutex) {
    return;
  }
  HRESULT hr = mMutex->AcquireSync(0, 10000);
  if (hr == WAIT_TIMEOUT) {
    MOZ_CRASH("GFX: IMFYCbCrImage timeout");
  }

  if (FAILED(hr)) {
    NS_WARNING("Failed to lock the texture");
  }
}

AutoLockD3D11Texture::~AutoLockD3D11Texture() {
  if (!mMutex) {
    return;
  }
  HRESULT hr = mMutex->ReleaseSync(0);
  if (FAILED(hr)) {
    NS_WARNING("Failed to unlock the texture");
  }
}

SyncObjectD3D11ClientContentDevice::SyncObjectD3D11ClientContentDevice(
    SyncHandle aSyncHandle)
    : SyncObjectD3D11Client(aSyncHandle) {}

bool SyncObjectD3D11ClientContentDevice::Synchronize(bool aFallible) {
  // Since this can be called from either the Paint or Main thread.
  // We don't want this to race since we initialize the sync texture here
  // too.
  MutexAutoLock syncLock(mSyncLock);

  MOZ_ASSERT(mContentDevice);

  if (!mSyncedTextures.size()) {
    return true;
  }

  if (!Init(mContentDevice, aFallible)) {
    return false;
  }

  RefPtr<ID3D11Device> dev;
  mSyncTexture->GetDevice(getter_AddRefs(dev));

  if (dev == DeviceManagerDx::Get()->GetContentDevice()) {
    if (DeviceManagerDx::Get()->HasDeviceReset()) {
      return false;
    }
  }

  if (dev != mContentDevice) {
    gfxWarning() << "Attempt to sync texture from invalid device.";
    return false;
  }

  return SyncObjectD3D11Client::SynchronizeInternal(dev, aFallible);
}

bool SyncObjectD3D11ClientContentDevice::IsSyncObjectValid() {
  RefPtr<ID3D11Device> dev;
  // There is a case that devices are not initialized yet with WebRender.
  if (gfxPlatform::GetPlatform()->DevicesInitialized()) {
    dev = DeviceManagerDx::Get()->GetContentDevice();
  }

  // Update mDevice if the ContentDevice initialization is detected.
  if (!mContentDevice && dev && NS_IsMainThread() && gfxVars::UseWebRender()) {
    mContentDevice = dev;
  }

  if (!dev || (NS_IsMainThread() && dev != mContentDevice)) {
    return false;
  }
  return true;
}

void SyncObjectD3D11ClientContentDevice::EnsureInitialized() {
  if (mContentDevice) {
    return;
  }

  if (XRE_IsGPUProcess() || !gfxPlatform::GetPlatform()->DevicesInitialized()) {
    return;
  }

  mContentDevice = DeviceManagerDx::Get()->GetContentDevice();
}

}  // namespace layers
}  // namespace mozilla
