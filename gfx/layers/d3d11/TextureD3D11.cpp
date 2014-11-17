/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureD3D11.h"
#include "CompositorD3D11.h"
#include "gfxContext.h"
#include "Effects.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "ReadbackManagerD3D11.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {

using namespace gfx;

namespace layers {

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
      return DXGI_FORMAT_A8_UNORM;
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
  aTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));
  // Textures created by the DXVA decoders don't have a mutex for synchronization
  if (mutex) {
    HRESULT hr = mutex->AcquireSync(0, INFINITE);
    if (FAILED(hr)) {
      NS_WARNING("Failed to lock the texture");
      return false;
    }
  }
  return true;
}

template<typename T> // ID3D10Texture2D or ID3D11Texture2D
static void UnlockD3DTexture(T* aTexture)
{
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));
  if (mutex) {
    HRESULT hr = mutex->ReleaseSync(0);
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }
}

TemporaryRef<TextureHost>
CreateTextureHostD3D11(const SurfaceDescriptor& aDesc,
                       ISurfaceAllocator* aDeallocator,
                       TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem:
    case SurfaceDescriptor::TSurfaceDescriptorMemory: {
      result = CreateBackendIndependentTextureHost(aDesc, aDeallocator, aFlags);
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      result = new DXGITextureHostD3D11(aFlags,
                                        aDesc.get_SurfaceDescriptorD3D10());
      break;
    }
    default: {
      NS_WARNING("Unsupported SurfaceDescriptor type");
    }
  }
  return result;
}

TextureClientD3D11::TextureClientD3D11(gfx::SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags)
  , mFormat(aFormat)
  , mIsLocked(false)
  , mNeedsClear(false)
  , mNeedsClearWhite(false)
{}

TextureClientD3D11::~TextureClientD3D11()
{
  if (mActor) {
    if (mTexture) {
      KeepUntilFullDeallocation(new TKeepAlive<ID3D10Texture2D>(mTexture10));
    } else if (mTexture10) {
      KeepUntilFullDeallocation(new TKeepAlive<ID3D11Texture2D>(mTexture));
    }
  }
#ifdef DEBUG
  // An Azure DrawTarget needs to be locked when it gets nullptr'ed as this is
  // when it calls EndDraw. This EndDraw should not execute anything so it
  // shouldn't -really- need the lock but the debug layer chokes on this.
  if (mDrawTarget) {
    MOZ_ASSERT(!mIsLocked);
    MOZ_ASSERT(mTexture || mTexture10);
    MOZ_ASSERT(mDrawTarget->refCount() == 1);
    if (mTexture) {
      LockD3DTexture(mTexture.get());
    } else {
      LockD3DTexture(mTexture10.get());
    }
    mDrawTarget = nullptr;
    if (mTexture) {
      UnlockD3DTexture(mTexture.get());
    } else {
      UnlockD3DTexture(mTexture10.get());
    }
  }
#endif
}

TemporaryRef<TextureClient>
TextureClientD3D11::CreateSimilar(TextureFlags aFlags,
                                  TextureAllocationFlags aAllocFlags) const
{
  RefPtr<TextureClient> tex = new TextureClientD3D11(mFormat, mFlags | aFlags);

  if (!tex->AllocateForSurface(mSize, aAllocFlags)) {
    return nullptr;
  }

  return tex;
}

bool
TextureClientD3D11::Lock(OpenMode aMode)
{
  if (!IsAllocated()) {
    return false;
  }
  MOZ_ASSERT(!mIsLocked, "The Texture is already locked!");

  if (mTexture) {
    MOZ_ASSERT(!mTexture10);
    mIsLocked = LockD3DTexture(mTexture.get());
  } else {
    MOZ_ASSERT(!mTexture);
    mIsLocked = LockD3DTexture(mTexture10.get());
  }
  if (!mIsLocked) {
    return false;
  }

  // Make sure that successful write-lock means we will have a DrawTarget to
  // write into.
  if (aMode & OpenMode::OPEN_WRITE) {
    mDrawTarget = BorrowDrawTarget();
    if (!mDrawTarget) {
      Unlock();
      return false;
    }
  }

  if (mNeedsClear) {
    mDrawTarget = BorrowDrawTarget();
    mDrawTarget->ClearRect(Rect(0, 0, GetSize().width, GetSize().height));
    mNeedsClear = false;
  }
  if (mNeedsClearWhite) {
    mDrawTarget = BorrowDrawTarget();
    mDrawTarget->FillRect(Rect(0, 0, GetSize().width, GetSize().height), ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
    mNeedsClearWhite = false;
  }

  return true;
}

void
TextureClientD3D11::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (!mIsLocked) {
    return;
  }

  if (mDrawTarget) {
    // see the comment on TextureClient::BorrowDrawTarget.
    // This DrawTarget is internal to the TextureClient and is only exposed to the
    // outside world between Lock() and Unlock(). This assertion checks that no outside
    // reference remains by the time Unlock() is called.
    MOZ_ASSERT(mDrawTarget->refCount() == 1);
    mDrawTarget->Flush();
  }

  if (mReadbackSink && mTexture10) {
    ID3D10Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();

    D3D10_TEXTURE2D_DESC desc;
    mTexture10->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.Usage = D3D10_USAGE_STAGING;
    desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    RefPtr<ID3D10Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, byRef(tex));

    if (FAILED(hr)) {
      gfx::gfxCriticalError() << "[D3D11] CreateTexture2D failure " << mSize << " Code: " << gfx::hexa(hr);
      return;
    }

    if (SUCCEEDED(hr)) {
      device->CopyResource(tex, mTexture10);

      gfxWindowsPlatform::GetPlatform()->GetReadbackManager()->PostTask(tex, mReadbackSink);
    } else {
      mReadbackSink->ProcessReadback(nullptr);
    }
  }

  // The DrawTarget is created only once, and is only usable between calls
  // to Lock and Unlock.
  if (mTexture) {
    UnlockD3DTexture(mTexture.get());
  } else {
    UnlockD3DTexture(mTexture10.get());
  }
  mIsLocked = false;
}

DrawTarget*
TextureClientD3D11::BorrowDrawTarget()
{
  MOZ_ASSERT(mIsLocked, "Calling TextureClient::BorrowDrawTarget without locking :(");

  if (!mIsLocked || (!mTexture && !mTexture10)) {
    return nullptr;
  }

  if (mDrawTarget) {
    return mDrawTarget;
  }

  // This may return a null DrawTarget
#if USE_D2D1_1
  if (mTexture) {
    mDrawTarget = Factory::CreateDrawTargetForD3D11Texture(mTexture, mFormat);
  } else
#endif
  {
    MOZ_ASSERT(mTexture10);
    mDrawTarget = Factory::CreateDrawTargetForD3D10Texture(mTexture10, mFormat);
  }
  return mDrawTarget;
}

bool
TextureClientD3D11::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  mSize = aSize;
  HRESULT hr;

  if (mFormat == SurfaceFormat::A8) {
    // Currently TextureClientD3D11 does not support A8 surfaces. Fallback.
    return false;
  }

#ifdef USE_D2D1_1
  ID3D11Device* d3d11device = gfxWindowsPlatform::GetPlatform()->GetD3D11ContentDevice();

  if (gfxPrefs::Direct2DUse1_1() && d3d11device) {

    CD3D11_TEXTURE2D_DESC newDesc(mFormat == SurfaceFormat::A8 ? DXGI_FORMAT_A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM,
                                  aSize.width, aSize.height, 1, 1,
                                  D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

    newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    hr = d3d11device->CreateTexture2D(&newDesc, nullptr, byRef(mTexture));
  } else
#endif
  {
    ID3D10Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();

    CD3D10_TEXTURE2D_DESC newDesc(DXGI_FORMAT_B8G8R8A8_UNORM,
      aSize.width, aSize.height, 1, 1,
      D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE);

    newDesc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    hr = device->CreateTexture2D(&newDesc, nullptr, byRef(mTexture10));
  }

  if (FAILED(hr)) {
    gfx::gfxCriticalError() << "[D3D11] CreateTexture2D failure " << aSize << " Code: " << gfx::hexa(hr);
    return false;
  }

  // Defer clearing to the next time we lock to avoid an extra (expensive) lock.
  mNeedsClear = aFlags & ALLOC_CLEAR_BUFFER;
  mNeedsClearWhite = aFlags & ALLOC_CLEAR_BUFFER_WHITE;

  return true;
}

bool
TextureClientD3D11::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  if (!IsAllocated()) {
    return false;
  }
  RefPtr<IDXGIResource> resource;
  if (mTexture) {
    mTexture->QueryInterface((IDXGIResource**)byRef(resource));
  } else {
    mTexture10->QueryInterface((IDXGIResource**)byRef(resource));
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

DXGITextureHostD3D11::DXGITextureHostD3D11(TextureFlags aFlags,
                                           const SurfaceDescriptorD3D10& aDescriptor)
  : TextureHost(aFlags)
  , mHandle(aDescriptor.handle())
  , mFormat(aDescriptor.format())
  , mIsLocked(false)
{
  OpenSharedHandle();
}

bool
DXGITextureHostD3D11::OpenSharedHandle()
{
  if (!GetDevice()) {
    return false;
  }

  HRESULT hr = GetDevice()->OpenSharedResource((HANDLE)mHandle,
                                               __uuidof(ID3D11Texture2D),
                                               (void**)(ID3D11Texture2D**)byRef(mTexture));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open shared texture");
    return false;
  }

  D3D11_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);
  mSize = IntSize(desc.Width, desc.Height);
  return true;
}

ID3D11Device*
DXGITextureHostD3D11::GetDevice()
{
  return gfxWindowsPlatform::GetPlatform()->GetD3D11Device();
}

void
DXGITextureHostD3D11::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D11*>(aCompositor);
}

bool
DXGITextureHostD3D11::Lock()
{
  if (!GetDevice()) {
    NS_WARNING("trying to lock a TextureHost without a D3D device");
    return false;
  }
  if (!mTextureSource) {
    if (!mTexture && !OpenSharedHandle()) {
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
  MOZ_ASSERT(mIsLocked);
  UnlockD3DTexture(mTextureSource->GetD3D11Texture());
  mIsLocked = false;
}

TextureSource*
DXGITextureHostD3D11::GetTextureSources()
{
  MOZ_ASSERT(mIsLocked);
  // If Lock was successful we must have a valid TextureSource.
  MOZ_ASSERT(mTextureSource);
  return mTextureSource.get();
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

    if (!mTexture) {
      hr = mCompositor->GetDevice()->CreateTexture2D(&desc, nullptr, byRef(mTexture));
      mIsTiled = false;
      if (FAILED(hr) || !mTexture) {
        Reset();
        return false;
      }
    }

    DataSourceSurface::MappedSurface map;
    aSurface->Map(DataSourceSurface::MapType::READ, &map);

    if (aDestRegion) {
      nsIntRegionRectIterator iter(*aDestRegion);
      const nsIntRect *iterRect;
      while ((iterRect = iter.Next())) {
        D3D11_BOX box;
        box.front = 0;
        box.back = 1;
        box.left = iterRect->x;
        box.top = iterRect->y;
        box.right = iterRect->XMost();
        box.bottom = iterRect->YMost();

        void* data = map.mData + map.mStride * iterRect->y + BytesPerPixel(aSurface->GetFormat()) * iterRect->x;

        mCompositor->GetDC()->UpdateSubresource(mTexture, 0, &box, data, map.mStride, map.mStride * mSize.height);
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

      hr = mCompositor->GetDevice()->CreateTexture2D(&desc, &initData, byRef(mTileTextures[i]));
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

void
DataTextureSourceD3D11::Reset()
{
  mTexture = nullptr;
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

nsIntRect
DataTextureSourceD3D11::GetTileRect()
{
  IntRect rect = GetTileRect(mCurrentTile);
  return nsIntRect(rect.x, rect.y, rect.width, rect.height);
}

void
DataTextureSourceD3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = static_cast<CompositorD3D11*>(aCompositor);
  if (mCompositor && mCompositor != d3dCompositor) {
    Reset();
  }
  mCompositor = d3dCompositor;
}

CompositingRenderTargetD3D11::CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture,
                                                           const gfx::IntPoint& aOrigin)
  : CompositingRenderTarget(aOrigin)
{
  MOZ_ASSERT(aTexture);

  mTexture = aTexture;

  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(byRef(device));

  HRESULT hr = device->CreateRenderTargetView(mTexture, nullptr, byRef(mRTView));

  if (FAILED(hr)) {
    LOGD3D11("Failed to create RenderTargetView.");
  }
}

IntSize
CompositingRenderTargetD3D11::GetSize() const
{
  return TextureSourceD3D11::GetSize();
}

}
}
