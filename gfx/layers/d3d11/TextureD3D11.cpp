/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureD3D11.h"
#include "CompositorD3D11.h"
#include "gfxContext.h"
#include "Effects.h"
#include "gfxWindowsPlatform.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "ReadbackManagerD3D11.h"
#include "mozilla/gfx/DeviceManagerD3D11.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {

using namespace gfx;

namespace layers {

static const GUID sD3D11TextureUsage =
{ 0xd89275b0, 0x6c7d, 0x4038, { 0xb5, 0xfa, 0x4d, 0x87, 0x16, 0xd5, 0xcc, 0x4e } };

/* This class gets its lifetime tied to a D3D texture
 * and increments memory usage on construction and decrements
 * on destruction */
class TextureMemoryMeasurer : public IUnknown
{
public:
  TextureMemoryMeasurer(size_t aMemoryUsed)
  {
    mMemoryUsed = aMemoryUsed;
    gfxWindowsPlatform::sD3D11SharedTextures += mMemoryUsed;
    mRefCnt = 0;
  }
  STDMETHODIMP_(ULONG) AddRef() {
    mRefCnt++;
    return mRefCnt;
  }
  STDMETHODIMP QueryInterface(REFIID riid,
                              void **ppvObject)
  {
    IUnknown *punk = nullptr;
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
};

static DXGI_FORMAT
SurfaceFormatToDXGIFormat(gfx::SurfaceFormat aFormat)
{
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
    default:
      MOZ_ASSERT(false, "unsupported format");
      return DXGI_FORMAT_UNKNOWN;
  }
}

static uint32_t
GetRequiredTilesD3D11(uint32_t aSize, uint32_t aMaxSize)
{
  uint32_t requiredTiles = aSize / aMaxSize;
  if (aSize % aMaxSize) {
    requiredTiles++;
  }
  return requiredTiles;
}

static IntRect
GetTileRectD3D11(uint32_t aID, IntSize aSize, uint32_t aMaxSize)
{
  uint32_t horizontalTiles = GetRequiredTilesD3D11(aSize.width, aMaxSize);
  uint32_t verticalTiles = GetRequiredTilesD3D11(aSize.height, aMaxSize);

  uint32_t verticalTile = aID / horizontalTiles;
  uint32_t horizontalTile = aID % horizontalTiles;

  return IntRect(horizontalTile * aMaxSize,
                 verticalTile * aMaxSize,
                 horizontalTile < (horizontalTiles - 1) ? aMaxSize : aSize.width % aMaxSize,
                 verticalTile < (verticalTiles - 1) ? aMaxSize : aSize.height % aMaxSize);
}

ID3D11ShaderResourceView*
TextureSourceD3D11::GetShaderResourceView()
{
  MOZ_ASSERT(mTexture == GetD3D11Texture(), "You need to override GetShaderResourceView if you're overriding GetD3D11Texture!");

  if (!mSRV && mTexture) {
    RefPtr<ID3D11Device> device;
    mTexture->GetDevice(getter_AddRefs(device));

    // see comment in CompositingRenderTargetD3D11 constructor
    CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(D3D11_SRV_DIMENSION_TEXTURE2D, mFormatOverride);
    D3D11_SHADER_RESOURCE_VIEW_DESC *desc = mFormatOverride == DXGI_FORMAT_UNKNOWN ? nullptr : &srvDesc;

    HRESULT hr = device->CreateShaderResourceView(mTexture, desc, getter_AddRefs(mSRV));
    if (FAILED(hr)) {
      gfxCriticalNote << "[D3D11] TextureSourceD3D11:GetShaderResourceView CreateSRV failure " << gfx::hexa(hr);
      return nullptr;
    }
  }
  return mSRV;
}

DataTextureSourceD3D11::DataTextureSourceD3D11(SurfaceFormat aFormat,
                                               CompositorD3D11* aCompositor,
                                               TextureFlags aFlags)
  : mCompositor(aCompositor)
  , mFormat(aFormat)
  , mFlags(aFlags)
  , mCurrentTile(0)
  , mIsTiled(false)
  , mIterating(false)
{
  MOZ_COUNT_CTOR(DataTextureSourceD3D11);
}

DataTextureSourceD3D11::DataTextureSourceD3D11(SurfaceFormat aFormat,
                                               CompositorD3D11* aCompositor,
                                               ID3D11Texture2D* aTexture)
: mCompositor(aCompositor)
, mFormat(aFormat)
, mFlags(TextureFlags::NO_FLAGS)
, mCurrentTile(0)
, mIsTiled(false)
, mIterating(false)
{
  MOZ_COUNT_CTOR(DataTextureSourceD3D11);

  mTexture = aTexture;
  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  mSize = IntSize(desc.Width, desc.Height);
}



DataTextureSourceD3D11::~DataTextureSourceD3D11()
{
  MOZ_COUNT_DTOR(DataTextureSourceD3D11);
}


template<typename T> // ID3D10Texture2D or ID3D11Texture2D
static bool LockD3DTexture(T* aTexture)
{
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  // Textures created by the DXVA decoders don't have a mutex for synchronization
  if (mutex) {
    HRESULT hr = mutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
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

template<typename T>
static bool HasKeyedMutex(T* aTexture)
{
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  return !!mutex;
}

template<typename T> // ID3D10Texture2D or ID3D11Texture2D
static void UnlockD3DTexture(T* aTexture)
{
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
  if (mutex) {
    HRESULT hr = mutex->ReleaseSync(0);
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }
}

DXGITextureData::DXGITextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                 bool aNeedsClear, bool aNeedsClearWhite,
                                 bool aIsForOutOfBandContent)
: mSize(aSize)
, mFormat(aFormat)
, mNeedsClear(aNeedsClear)
, mNeedsClearWhite(aNeedsClearWhite)
, mHasSynchronization(false)
, mIsForOutOfBandContent(aIsForOutOfBandContent)
{}

D3D11TextureData::D3D11TextureData(ID3D11Texture2D* aTexture,
                                   gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                   bool aNeedsClear, bool aNeedsClearWhite,
                                   bool aIsForOutOfBandContent)
: DXGITextureData(aSize, aFormat, aNeedsClear, aNeedsClearWhite, aIsForOutOfBandContent)
, mTexture(aTexture)
{
  MOZ_ASSERT(aTexture);
  mHasSynchronization = HasKeyedMutex(aTexture);
}

D3D11TextureData::~D3D11TextureData()
{
#ifdef DEBUG
  // An Azure DrawTarget needs to be locked when it gets nullptr'ed as this is
  // when it calls EndDraw. This EndDraw should not execute anything so it
  // shouldn't -really- need the lock but the debug layer chokes on this.
  if (mDrawTarget) {
    Lock(OpenMode::OPEN_NONE, nullptr);
    mDrawTarget = nullptr;
    Unlock();
  }
#endif
}

bool
D3D11TextureData::Lock(OpenMode aMode, FenceHandle*)
{
  if (!LockD3DTexture(mTexture.get())) {
    return false;
  }

  if (NS_IsMainThread() && !mIsForOutOfBandContent) {
    if (!PrepareDrawTargetInLock(aMode)) {
      Unlock();
      return false;
    }
  }

  return true;
}

bool
DXGITextureData::PrepareDrawTargetInLock(OpenMode aMode)
{
  // Make sure that successful write-lock means we will have a DrawTarget to
  // write into.
  if (!mDrawTarget && (aMode & OpenMode::OPEN_WRITE || mNeedsClear || mNeedsClearWhite)) {
    mDrawTarget = BorrowDrawTarget();
    if (!mDrawTarget) {
      return false;
    }
  }

  if (mNeedsClear) {
    mDrawTarget->ClearRect(Rect(0, 0, mSize.width, mSize.height));
    mNeedsClear = false;
  }
  if (mNeedsClearWhite) {
    mDrawTarget->FillRect(Rect(0, 0, mSize.width, mSize.height), ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
    mNeedsClearWhite = false;
  }

  return true;
}

void
D3D11TextureData::Unlock()
{
  UnlockD3DTexture(mTexture.get());
}


void
DXGITextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = true;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = mHasSynchronization;
}

void
D3D11TextureData::SyncWithObject(SyncObject* aSyncObject)
{
  if (!aSyncObject || !NS_IsMainThread() || mIsForOutOfBandContent) {
    // When off the main thread we sync using a keyed mutex per texture.
    return;
  }

  MOZ_ASSERT(aSyncObject->GetSyncType() == SyncObject::SyncType::D3D11);
  SyncObjectD3D11* sync = static_cast<SyncObjectD3D11*>(aSyncObject);
  sync->RegisterTexture(mTexture);
}

bool
DXGITextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
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

  aOutDescriptor = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle, mFormat, mSize);
  return true;
}

DXGITextureData*
DXGITextureData::Create(IntSize aSize, SurfaceFormat aFormat, TextureAllocationFlags aFlags)
{
  if (aFormat == SurfaceFormat::A8) {
    // Currently we don't support A8 surfaces. Fallback.
    return nullptr;
  }

  return D3D11TextureData::Create(aSize, aFormat, aFlags);
}

DXGITextureData*
D3D11TextureData::Create(IntSize aSize, SurfaceFormat aFormat, SourceSurface* aSurface,
                         TextureAllocationFlags aFlags, ID3D11Device* aDevice)
{
  // Just grab any device. We never use the immediate context, so the devices are fine
  // to use from any thread.
  RefPtr<ID3D11Device> device = aDevice;
  if (!device) {
    device = DeviceManagerD3D11::Get()->GetContentDevice();
    if (!device) {
      return nullptr;
    }
  }

  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_B8G8R8A8_UNORM,
                                aSize.width, aSize.height, 1, 1,
                                D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  if (!NS_IsMainThread() || !!(aFlags & ALLOC_FOR_OUT_OF_BAND_CONTENT)) {
    // On the main thread we use the syncobject to handle synchronization.
    if (!(aFlags & ALLOC_MANUAL_SYNCHRONIZATION)) {
      newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    }
  }

  D3D11_SUBRESOURCE_DATA uploadData;
  D3D11_SUBRESOURCE_DATA* uploadDataPtr = nullptr;
  RefPtr<DataSourceSurface> srcSurf;
  if (aSurface) {
    srcSurf = aSurface->GetDataSurface();

    if (!srcSurf) {
      gfxCriticalError() << "Failed to GetDataSurface in D3D11TextureData::Create";
      return nullptr;
    }

    DataSourceSurface::MappedSurface sourceMap;
    if (!srcSurf->Map(DataSourceSurface::READ, &sourceMap)) {
      gfxCriticalError() << "Failed to map source surface for D3D11TextureData::Create";
      return nullptr;
    }

    uploadData.pSysMem = sourceMap.mData;
    uploadData.SysMemPitch = sourceMap.mStride;
    uploadData.SysMemSlicePitch = 0; // unused

    uploadDataPtr = &uploadData;
  }

  RefPtr<ID3D11Texture2D> texture11;
  HRESULT hr = device->CreateTexture2D(&newDesc, uploadDataPtr, getter_AddRefs(texture11));
  if (srcSurf) {
    srcSurf->Unmap();
  }
  if (FAILED(hr)) {
    gfxCriticalError(CriticalLog::DefaultOptions(Factory::ReasonableSurfaceSize(aSize)))
      << "[D3D11] 2 CreateTexture2D failure " << aSize << " Code: " << gfx::hexa(hr);
    return nullptr;
  }

  // If we created the texture with a keyed mutex, then we expect all operations
  // on it to be synchronized using it. If we did an initial upload using aSurface
  // then bizarely this isn't covered, so we insert a manual lock/unlock pair
  // to force this.
  if (aSurface && newDesc.MiscFlags == D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
    if (!LockD3DTexture(texture11.get())) {
      return nullptr;
    }
    UnlockD3DTexture(texture11.get());
  }
  texture11->SetPrivateDataInterface(sD3D11TextureUsage,
                                     new TextureMemoryMeasurer(newDesc.Width * newDesc.Height * 4));
  return new D3D11TextureData(texture11, aSize, aFormat,
                              aFlags & ALLOC_CLEAR_BUFFER,
                              aFlags & ALLOC_CLEAR_BUFFER_WHITE,
                              aFlags & ALLOC_FOR_OUT_OF_BAND_CONTENT);
}

DXGITextureData*
D3D11TextureData::Create(IntSize aSize, SurfaceFormat aFormat,
                         TextureAllocationFlags aFlags, ID3D11Device* aDevice)
{
  return D3D11TextureData::Create(aSize, aFormat, nullptr, aFlags, aDevice);
}

DXGITextureData*
D3D11TextureData::Create(SourceSurface* aSurface,
                         TextureAllocationFlags aFlags, ID3D11Device* aDevice)
{
  if (aSurface->GetFormat() == SurfaceFormat::A8) {
    // Currently we don't support A8 surfaces. Fallback.
    return nullptr;
  }

  return D3D11TextureData::Create(aSurface->GetSize(), aSurface->GetFormat(),
                                  aSurface, aFlags, aDevice);
}

void
D3D11TextureData::Deallocate(ClientIPCAllocator* aAllocator)
{
  mDrawTarget = nullptr;
  mTexture = nullptr;
}

already_AddRefed<TextureClient>
CreateD3D11TextureClientWithDevice(IntSize aSize, SurfaceFormat aFormat,
                                   TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags,
                                   ID3D11Device* aDevice,
                                   ClientIPCAllocator* aAllocator)
{
  TextureData* data = D3D11TextureData::Create(aSize, aFormat, aAllocFlags, aDevice);
  if (!data) {
    return nullptr;
  }
  return MakeAndAddRef<TextureClient>(data, aTextureFlags, aAllocator);
}

TextureData*
D3D11TextureData::CreateSimilar(ClientIPCAllocator* aAllocator,
                                TextureFlags aFlags,
                                TextureAllocationFlags aAllocFlags) const
{
  return D3D11TextureData::Create(mSize, mFormat, aAllocFlags);
}

void
D3D11TextureData::GetDXGIResource(IDXGIResource** aOutResource)
{
  mTexture->QueryInterface(aOutResource);
}

DXGIYCbCrTextureData*
DXGIYCbCrTextureData::Create(ClientIPCAllocator* aAllocator,
                             TextureFlags aFlags,
                             IUnknown* aTextureY,
                             IUnknown* aTextureCb,
                             IUnknown* aTextureCr,
                             HANDLE aHandleY,
                             HANDLE aHandleCb,
                             HANDLE aHandleCr,
                             const gfx::IntSize& aSize,
                             const gfx::IntSize& aSizeY,
                             const gfx::IntSize& aSizeCbCr)
{
  if (!aHandleY || !aHandleCb || !aHandleCr ||
      !aTextureY || !aTextureCb || !aTextureCr) {
    return nullptr;
  }

  DXGIYCbCrTextureData* texture = new DXGIYCbCrTextureData();
  texture->mHandles[0] = aHandleY;
  texture->mHandles[1] = aHandleCb;
  texture->mHandles[2] = aHandleCr;
  texture->mHoldRefs[0] = aTextureY;
  texture->mHoldRefs[1] = aTextureCb;
  texture->mHoldRefs[2] = aTextureCr;
  texture->mSize = aSize;
  texture->mSizeY = aSizeY;
  texture->mSizeCbCr = aSizeCbCr;

  return texture;
}

DXGIYCbCrTextureData*
DXGIYCbCrTextureData::Create(ClientIPCAllocator* aAllocator,
                             TextureFlags aFlags,
                             ID3D11Texture2D* aTextureY,
                             ID3D11Texture2D* aTextureCb,
                             ID3D11Texture2D* aTextureCr,
                             const gfx::IntSize& aSize,
                             const gfx::IntSize& aSizeY,
                             const gfx::IntSize& aSizeCbCr)
{
  if (!aTextureY || !aTextureCb || !aTextureCr) {
    return nullptr;
  }

  aTextureY->SetPrivateDataInterface(sD3D11TextureUsage,
    new TextureMemoryMeasurer(aSize.width * aSize.height));
  aTextureCb->SetPrivateDataInterface(sD3D11TextureUsage,
    new TextureMemoryMeasurer(aSizeCbCr.width * aSizeCbCr.height));
  aTextureCr->SetPrivateDataInterface(sD3D11TextureUsage,
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

  return DXGIYCbCrTextureData::Create(aAllocator, aFlags,
                                      aTextureY, aTextureCb, aTextureCr,
                                      handleY, handleCb, handleCr,
                                      aSize, aSizeY, aSizeCbCr);
}

void
DXGIYCbCrTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSize;
  aInfo.format = gfx::SurfaceFormat::YUV;
  aInfo.supportsMoz2D = false;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
}

bool
DXGIYCbCrTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceDescriptorDXGIYCbCr(
    (WindowsHandle)mHandles[0], (WindowsHandle)mHandles[1], (WindowsHandle)mHandles[2],
    mSize, mSizeY, mSizeCbCr
  );
  return true;
}

void
DXGIYCbCrTextureData::Deallocate(ClientIPCAllocator*)
{
  mHoldRefs[0] = nullptr;
  mHoldRefs[1] = nullptr;
  mHoldRefs[2] = nullptr;
}

already_AddRefed<TextureHost>
CreateTextureHostD3D11(const SurfaceDescriptor& aDesc,
                       ISurfaceAllocator* aDeallocator,
                       TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorBuffer: {
      result = CreateBackendIndependentTextureHost(aDesc, aDeallocator, aFlags);
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      result = new DXGITextureHostD3D11(aFlags,
                                        aDesc.get_SurfaceDescriptorD3D10());
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr: {
      result = new DXGIYCbCrTextureHostD3D11(aFlags,
                                             aDesc.get_SurfaceDescriptorDXGIYCbCr());
      break;
    }
    default: {
      NS_WARNING("Unsupported SurfaceDescriptor type");
    }
  }
  return result.forget();
}


already_AddRefed<DrawTarget>
D3D11TextureData::BorrowDrawTarget()
{
  MOZ_ASSERT(NS_IsMainThread());

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

bool
D3D11TextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  // Supporting texture updates after creation requires an ID3D11DeviceContext and those
  // aren't threadsafe. We'd need to either lock, or have a device for whatever thread
  // this runs on and we're trying to avoid extra devices (bug 1284672).
  MOZ_ASSERT(false, "UpdateFromSurface not supported for D3D11! Use CreateFromSurface instead");
  return false;
}

DXGITextureHostD3D11::DXGITextureHostD3D11(TextureFlags aFlags,
                                           const SurfaceDescriptorD3D10& aDescriptor)
  : TextureHost(aFlags)
  , mSize(aDescriptor.size())
  , mHandle(aDescriptor.handle())
  , mFormat(aDescriptor.format())
  , mIsLocked(false)
{
}

bool
DXGITextureHostD3D11::OpenSharedHandle()
{
  if (!GetDevice()) {
    return false;
  }

  HRESULT hr = GetDevice()->OpenSharedResource((HANDLE)mHandle,
                                               __uuidof(ID3D11Texture2D),
                                               (void**)(ID3D11Texture2D**)getter_AddRefs(mTexture));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture");
    return false;
  }

  D3D11_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);
  mSize = IntSize(desc.Width, desc.Height);
  return true;
}

RefPtr<ID3D11Device>
DXGITextureHostD3D11::GetDevice()
{
  if (mFlags & TextureFlags::INVALID_COMPOSITOR) {
    return nullptr;
  }

  return DeviceManagerD3D11::Get()->GetCompositorDevice();
}

static CompositorD3D11* AssertD3D11Compositor(Compositor* aCompositor)
{
  CompositorD3D11* compositor = aCompositor ? aCompositor->AsCompositorD3D11()
                                            : nullptr;
  if (!compositor) {
    gfxCriticalNote << "[D3D11] Attempt to set an incompatible compositor";
  }
  return compositor;
}

void
DXGITextureHostD3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = AssertD3D11Compositor(aCompositor);
  if (!d3dCompositor) {
    mCompositor = nullptr;
    mTextureSource = nullptr;
    return;
  }
  mCompositor = d3dCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(aCompositor);
  }
}

Compositor*
DXGITextureHostD3D11::GetCompositor()
{
  return mCompositor;
}

bool
DXGITextureHostD3D11::Lock()
{
  if (!mCompositor) {
    NS_WARNING("no suitable compositor");
    return false;
  }

  if (!GetDevice()) {
    NS_WARNING("trying to lock a TextureHost without a D3D device");
    return false;
  }

  if (!mTextureSource) {
    if (!mTexture && !OpenSharedHandle()) {
      gfxWindowsPlatform::GetPlatform()->ForceDeviceReset(ForcedDeviceResetReason::OPENSHAREDHANDLE);
      return false;
    }

    mTextureSource = new DataTextureSourceD3D11(mFormat, mCompositor, mTexture);
  }

  mIsLocked = LockD3DTexture(mTextureSource->GetD3D11Texture());

  return mIsLocked;
}

void
DXGITextureHostD3D11::Unlock()
{
  UnlockD3DTexture(mTextureSource->GetD3D11Texture());
}

bool
DXGITextureHostD3D11::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  MOZ_ASSERT(mIsLocked);
  // If Lock was successful we must have a valid TextureSource.
  MOZ_ASSERT(mTextureSource);
  aTexture = mTextureSource;
  return !!aTexture;
}

DXGIYCbCrTextureHostD3D11::DXGIYCbCrTextureHostD3D11(TextureFlags aFlags,
  const SurfaceDescriptorDXGIYCbCr& aDescriptor)
  : TextureHost(aFlags)
  , mSize(aDescriptor.size())
  , mIsLocked(false)
{
  mHandles[0] = aDescriptor.handleY();
  mHandles[1] = aDescriptor.handleCb();
  mHandles[2] = aDescriptor.handleCr();
}

bool
DXGIYCbCrTextureHostD3D11::OpenSharedHandle()
{
  RefPtr<ID3D11Device> device = GetDevice();
  if (!device) {
    return false;
  }

  RefPtr<ID3D11Texture2D> textures[3];

  HRESULT hr = device->OpenSharedResource((HANDLE)mHandles[0],
                                          __uuidof(ID3D11Texture2D),
                                          (void**)(ID3D11Texture2D**)getter_AddRefs(textures[0]));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture for Y Plane");
    return false;
  }

  hr = device->OpenSharedResource((HANDLE)mHandles[1],
                                  __uuidof(ID3D11Texture2D),
                                  (void**)(ID3D11Texture2D**)getter_AddRefs(textures[1]));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture for Cb Plane");
    return false;
  }

  hr = device->OpenSharedResource((HANDLE)mHandles[2],
                                  __uuidof(ID3D11Texture2D),
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

RefPtr<ID3D11Device>
DXGIYCbCrTextureHostD3D11::GetDevice()
{
  if (mFlags & TextureFlags::INVALID_COMPOSITOR) {
    return nullptr;
  }

  return DeviceManagerD3D11::Get()->GetCompositorDevice();
}

void
DXGIYCbCrTextureHostD3D11::SetCompositor(Compositor* aCompositor)
{
  mCompositor = AssertD3D11Compositor(aCompositor);
  if (!mCompositor) {
    mTextureSources[0] = nullptr;
    mTextureSources[1] = nullptr;
    mTextureSources[2] = nullptr;
    return;
  }

  if (mTextureSources[0]) {
    mTextureSources[0]->SetCompositor(aCompositor);
  }
}

Compositor*
DXGIYCbCrTextureHostD3D11::GetCompositor()
{
  return mCompositor;
}

bool
DXGIYCbCrTextureHostD3D11::Lock()
{
  if (!mCompositor) {
    NS_WARNING("no suitable compositor");
    return false;
  }

  if (!GetDevice()) {
    NS_WARNING("trying to lock a TextureHost without a D3D device");
    return false;
  }
  if (!mTextureSources[0]) {
    if (!mTextures[0] && !OpenSharedHandle()) {
      return false;
    }

    MOZ_ASSERT(mTextures[1] && mTextures[2]);

    mTextureSources[0] = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor, mTextures[0]);
    mTextureSources[1] = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor, mTextures[1]);
    mTextureSources[2] = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor, mTextures[2]);
    mTextureSources[0]->SetNextSibling(mTextureSources[1]);
    mTextureSources[1]->SetNextSibling(mTextureSources[2]);
  }

  mIsLocked = LockD3DTexture(mTextureSources[0]->GetD3D11Texture()) &&
              LockD3DTexture(mTextureSources[1]->GetD3D11Texture()) &&
              LockD3DTexture(mTextureSources[2]->GetD3D11Texture());

  return mIsLocked;
}

void
DXGIYCbCrTextureHostD3D11::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  UnlockD3DTexture(mTextureSources[0]->GetD3D11Texture());
  UnlockD3DTexture(mTextureSources[1]->GetD3D11Texture());
  UnlockD3DTexture(mTextureSources[2]->GetD3D11Texture());
  mIsLocked = false;
}

bool
DXGIYCbCrTextureHostD3D11::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  MOZ_ASSERT(mIsLocked);
  // If Lock was successful we must have a valid TextureSource.
  MOZ_ASSERT(mTextureSources[0] && mTextureSources[1] && mTextureSources[2]);
  aTexture = mTextureSources[0].get();
  return !!aTexture;
}

bool
DataTextureSourceD3D11::Update(DataSourceSurface* aSurface,
                               nsIntRegion* aDestRegion,
                               IntPoint* aSrcOffset)
{
  // Incremental update with a source offset is only used on Mac so it is not
  // clear that we ever will need to support it for D3D.
  MOZ_ASSERT(!aSrcOffset);
  MOZ_ASSERT(aSurface);

  HRESULT hr;

  if (!mCompositor || !mCompositor->GetDevice()) {
    return false;
  }

  uint32_t bpp = BytesPerPixel(aSurface->GetFormat());
  DXGI_FORMAT dxgiFormat = SurfaceFormatToDXGIFormat(aSurface->GetFormat());

  mSize = aSurface->GetSize();
  mFormat = aSurface->GetFormat();

  CD3D11_TEXTURE2D_DESC desc(dxgiFormat, mSize.width, mSize.height, 1, 1);

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if ((mSize.width <= maxSize && mSize.height <= maxSize) ||
      (mFlags & TextureFlags::DISALLOW_BIGIMAGE)) {

    if (mTexture) {
      D3D11_TEXTURE2D_DESC currentDesc;
      mTexture->GetDesc(&currentDesc);

      // Make sure there's no size mismatch, if there is, recreate.
      if (currentDesc.Width != mSize.width || currentDesc.Height != mSize.height ||
          currentDesc.Format != dxgiFormat) {
        mTexture = nullptr;
        // Make sure we upload the whole surface.
        aDestRegion = nullptr;
      }
    }

    nsIntRegion *regionToUpdate = aDestRegion;
    if (!mTexture) {
      hr = mCompositor->GetDevice()->CreateTexture2D(&desc, nullptr, getter_AddRefs(mTexture));
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

    if (regionToUpdate) {
      for (auto iter = regionToUpdate->RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();
        D3D11_BOX box;
        box.front = 0;
        box.back = 1;
        box.left = rect.x;
        box.top = rect.y;
        box.right = rect.XMost();
        box.bottom = rect.YMost();

        void* data = map.mData + map.mStride * rect.y + BytesPerPixel(aSurface->GetFormat()) * rect.x;

        mCompositor->GetDC()->UpdateSubresource(mTexture, 0, &box, data, map.mStride, map.mStride * rect.height);
      }
    } else {
      mCompositor->GetDC()->UpdateSubresource(mTexture, 0, nullptr, aSurface->GetData(),
                                              aSurface->Stride(), aSurface->Stride() * mSize.height);
    }

    aSurface->Unmap();
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D11(mSize.width, maxSize) *
                         GetRequiredTilesD3D11(mSize.height, maxSize);

    mTileTextures.resize(tileCount);
    mTileSRVs.resize(tileCount);
    mTexture = nullptr;

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);

      desc.Width = tileRect.width;
      desc.Height = tileRect.height;
      desc.Usage = D3D11_USAGE_IMMUTABLE;

      D3D11_SUBRESOURCE_DATA initData;
      initData.pSysMem = aSurface->GetData() +
                         tileRect.y * aSurface->Stride() +
                         tileRect.x * bpp;
      initData.SysMemPitch = aSurface->Stride();

      hr = mCompositor->GetDevice()->CreateTexture2D(&desc, &initData, getter_AddRefs(mTileTextures[i]));
      if (FAILED(hr) || !mTileTextures[i]) {
        Reset();
        return false;
      }
    }
  }
  return true;
}

ID3D11Texture2D*
DataTextureSourceD3D11::GetD3D11Texture() const
{
  return mIterating ? mTileTextures[mCurrentTile]
                    : mTexture;
}

ID3D11ShaderResourceView*
DataTextureSourceD3D11::GetShaderResourceView()
{
  if (mIterating) {
    if (!mTileSRVs[mCurrentTile]) {
      if (!mTileTextures[mCurrentTile]) {
        return nullptr;
      }

      RefPtr<ID3D11Device> device;
      mTileTextures[mCurrentTile]->GetDevice(getter_AddRefs(device));
      HRESULT hr = device->CreateShaderResourceView(mTileTextures[mCurrentTile], nullptr, getter_AddRefs(mTileSRVs[mCurrentTile]));
      if (FAILED(hr)) {
        gfxCriticalNote << "[D3D11] DataTextureSourceD3D11:GetShaderResourceView CreateSRV failure " << gfx::hexa(hr);
        return nullptr;
      }
    }
    return mTileSRVs[mCurrentTile];
  }

  return TextureSourceD3D11::GetShaderResourceView();
}

void
DataTextureSourceD3D11::Reset()
{
  mTexture = nullptr;
  mTileSRVs.resize(0);
  mTileTextures.resize(0);
  mIsTiled = false;
  mSize.width = 0;
  mSize.height = 0;
}

IntRect
DataTextureSourceD3D11::GetTileRect(uint32_t aIndex) const
{
  return GetTileRectD3D11(aIndex, mSize, mCompositor->GetMaxTextureSize());
}

IntRect
DataTextureSourceD3D11::GetTileRect()
{
  IntRect rect = GetTileRect(mCurrentTile);
  return IntRect(rect.x, rect.y, rect.width, rect.height);
}

void
DataTextureSourceD3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = AssertD3D11Compositor(aCompositor);
  if (!d3dCompositor) {
    return;
  }
  mCompositor = d3dCompositor;
  if (mNextSibling) {
    mNextSibling->SetCompositor(aCompositor);
  }
}

CompositingRenderTargetD3D11::CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture,
                                                           const gfx::IntPoint& aOrigin,
                                                           DXGI_FORMAT aFormatOverride)
  : CompositingRenderTarget(aOrigin)
{
  MOZ_ASSERT(aTexture);

  mTexture = aTexture;

  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(getter_AddRefs(device));

  mFormatOverride = aFormatOverride;

  // If we happen to have a typeless underlying DXGI surface, we need to be explicit
  // about the format here. (Such a surface could come from an external source, such
  // as the Oculus compositor)
  CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(D3D11_RTV_DIMENSION_TEXTURE2D, mFormatOverride);
  D3D11_RENDER_TARGET_VIEW_DESC *desc = aFormatOverride == DXGI_FORMAT_UNKNOWN ? nullptr : &rtvDesc;

  HRESULT hr = device->CreateRenderTargetView(mTexture, desc, getter_AddRefs(mRTView));

  if (FAILED(hr)) {
    LOGD3D11("Failed to create RenderTargetView.");
  }
}

void
CompositingRenderTargetD3D11::BindRenderTarget(ID3D11DeviceContext* aContext)
{
  if (mClearOnBind) {
    FLOAT clear[] = { 0, 0, 0, 0 };
    aContext->ClearRenderTargetView(mRTView, clear);
    mClearOnBind = false;
  }
  ID3D11RenderTargetView* view = mRTView;
  aContext->OMSetRenderTargets(1, &view, nullptr);
}

IntSize
CompositingRenderTargetD3D11::GetSize() const
{
  return TextureSourceD3D11::GetSize();
}

SyncObjectD3D11::SyncObjectD3D11(SyncHandle aHandle)
{
  MOZ_ASSERT(aHandle);

  mHandle = aHandle;
}

void
SyncObjectD3D11::RegisterTexture(ID3D11Texture2D* aTexture)
{
  mD3D11SyncedTextures.push_back(aTexture);
}

void
SyncObjectD3D11::FinalizeFrame()
{
  HRESULT hr;

  if (!mD3D11Texture && mD3D11SyncedTextures.size()) {
    RefPtr<ID3D11Device> device = DeviceManagerD3D11::Get()->GetContentDevice();

    hr = device->OpenSharedResource(mHandle, __uuidof(ID3D11Texture2D), (void**)(ID3D11Texture2D**)getter_AddRefs(mD3D11Texture));

    if (FAILED(hr) || !mD3D11Texture) {
      gfxCriticalError() << "Failed to D3D11 OpenSharedResource for frame finalization: " << hexa(hr);

      if (gfxWindowsPlatform::GetPlatform()->DidRenderingDeviceReset()) {
        return;
      }

      gfxDevCrash(LogReason::D3D11FinalizeFrame) << "Without device reset: " << hexa(hr);
    }

    // test QI
    RefPtr<IDXGIKeyedMutex> mutex;
    hr = mD3D11Texture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

    if (FAILED(hr) || !mutex) {
      // Leave both the critical error and MOZ_CRASH for now; the critical error lets
      // us "save" the hr value.  We will probably eventuall replace this with gfxDevCrash.
      gfxCriticalError() << "Failed to get KeyedMutex (2): " << hexa(hr);
      MOZ_CRASH("GFX: Cannot get D3D11 KeyedMutex");
    }
  }

  if (mD3D11SyncedTextures.size()) {
    RefPtr<IDXGIKeyedMutex> mutex;
    hr = mD3D11Texture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
    hr = mutex->AcquireSync(0, 20000);

    if (hr == WAIT_TIMEOUT) {
      if (gfxWindowsPlatform::GetPlatform()->DidRenderingDeviceReset()) {
        gfxWarning() << "AcquireSync timed out because of device reset.";
        return;
      }
      gfxDevCrash(LogReason::D3D11SyncLock) << "Timeout on the D3D11 sync lock";
    }

    D3D11_BOX box;
    box.front = box.top = box.left = 0;
    box.back = box.bottom = box.right = 1;

    RefPtr<ID3D11Device> dev = DeviceManagerD3D11::Get()->GetContentDevice();
    if (!dev) {
      if (gfxWindowsPlatform::GetPlatform()->DidRenderingDeviceReset()) {
        return;
      }
      MOZ_CRASH("GFX: Invalid D3D11 content device");
    }

    RefPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(getter_AddRefs(ctx));

    for (auto iter = mD3D11SyncedTextures.begin(); iter != mD3D11SyncedTextures.end(); iter++) {
      ctx->CopySubresourceRegion(mD3D11Texture, 0, 0, 0, 0, *iter, 0, &box);
    }

    mutex->ReleaseSync(0);

    mD3D11SyncedTextures.clear();
  }
}

}
}
