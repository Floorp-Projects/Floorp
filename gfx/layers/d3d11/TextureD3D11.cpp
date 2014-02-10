/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureD3D11.h"
#include "CompositorD3D11.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "Effects.h"
#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#include "gfx2DGlue.h"

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
                                               CompositorD3D11* aCompositor)
  : mCompositor(aCompositor)
  , mFormat(aFormat)
  , mFlags(0)
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
, mFlags(0)
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
static void LockD3DTexture(T* aTexture)
{
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));
  mutex->AcquireSync(0, INFINITE);
}

template<typename T> // ID3D10Texture2D or ID3D11Texture2D
static void UnlockD3DTexture(T* aTexture)
{
  MOZ_ASSERT(aTexture);
  RefPtr<IDXGIKeyedMutex> mutex;
  aTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));
  mutex->ReleaseSync(0);
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
{}

TextureClientD3D11::~TextureClientD3D11()
{
#ifdef DEBUG
  // An Azure DrawTarget needs to be locked when it gets nullptr'ed as this is
  // when it calls EndDraw. This EndDraw should not execute anything so it
  // shouldn't -really- need the lock but the debug layer chokes on this.
  if (mDrawTarget) {
    MOZ_ASSERT(!mIsLocked);
    MOZ_ASSERT(mTexture);
    MOZ_ASSERT(mDrawTarget->refCount() == 1);
    LockD3DTexture(mTexture.get());
    mDrawTarget = nullptr;
    UnlockD3DTexture(mTexture.get());
  }
#endif
}

bool
TextureClientD3D11::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mIsLocked, "The Texture is already locked!");
  LockD3DTexture(mTexture.get());
  mIsLocked = true;

  if (mNeedsClear) {
    mDrawTarget = GetAsDrawTarget();
    mDrawTarget->ClearRect(Rect(0, 0, GetSize().width, GetSize().height));
    mNeedsClear = false;
  }

  return true;
}

void
TextureClientD3D11::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (mDrawTarget) {
    MOZ_ASSERT(mDrawTarget->refCount() == 1);
    mDrawTarget->Flush();
  }

  // The DrawTarget is created only once, and is only usable between calls
  // to Lock and Unlock.
  UnlockD3DTexture(mTexture.get());
  mIsLocked = false;
}

TemporaryRef<DrawTarget>
TextureClientD3D11::GetAsDrawTarget()
{
  MOZ_ASSERT(mIsLocked, "Calling TextureClient::GetAsDrawTarget without locking :(");

  if (mDrawTarget) {
    return mDrawTarget;
  }

  mDrawTarget = Factory::CreateDrawTargetForD3D10Texture(mTexture, mFormat);
  return mDrawTarget;
}

bool
TextureClientD3D11::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  mSize = aSize;
  ID3D10Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();

  CD3D10_TEXTURE2D_DESC newDesc(SurfaceFormatToDXGIFormat(mFormat),
                                aSize.width, aSize.height, 1, 1,
                                D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE);

  newDesc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  HRESULT hr = device->CreateTexture2D(&newDesc, nullptr, byRef(mTexture));

  if (FAILED(hr)) {
    LOGD3D11("Error creating texture for client!");
    return false;
  }

  // Defer clearing to the next time we lock to avoid an extra (expensive) lock.
  mNeedsClear = aFlags & ALLOC_CLEAR_BUFFER;

  return true;
}

bool
TextureClientD3D11::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  if (!IsAllocated()) {
    return false;
  }

  RefPtr<IDXGIResource> resource;
  mTexture->QueryInterface((IDXGIResource**)byRef(resource));
  HANDLE sharedHandle;
  HRESULT hr = resource->GetSharedHandle(&sharedHandle);

  if (FAILED(hr)) {
    LOGD3D11("Error getting shared handle for texture.");
    return false;
  }

  aOutDescriptor = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle, mFormat);
  return true;
}

DXGITextureHostD3D11::DXGITextureHostD3D11(TextureFlags aFlags,
                                           const SurfaceDescriptorD3D10& aDescriptor)
  : TextureHost(aFlags)
  , mHandle(aDescriptor.handle())
  , mFormat(aDescriptor.format())
  , mIsLocked(false)
{}

ID3D11Device*
DXGITextureHostD3D11::GetDevice()
{
  return mCompositor ? mCompositor->GetDevice() : nullptr;
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
    RefPtr<ID3D11Texture2D> tex;
    HRESULT hr = GetDevice()->OpenSharedResource((HANDLE)mHandle,
                                                 __uuidof(ID3D11Texture2D),
                                                 (void**)(ID3D11Texture2D**)byRef(tex));
    if (FAILED(hr)) {
      NS_WARNING("Failed to open shared texture");
      return false;
    }

    mTextureSource = new DataTextureSourceD3D11(mFormat, mCompositor, tex);
    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);
    mSize = IntSize(desc.Width, desc.Height);
  }

  LockD3DTexture(mTextureSource->GetD3D11Texture());

  mIsLocked = true;
  return true;
}

void
DXGITextureHostD3D11::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  UnlockD3DTexture(mTextureSource->GetD3D11Texture());
  mIsLocked = false;
}

NewTextureSource*
DXGITextureHostD3D11::GetTextureSources()
{
  return mTextureSource.get();
}

bool
DataTextureSourceD3D11::Update(DataSourceSurface* aSurface,
                               nsIntRegion* aDestRegion,
                               IntPoint* aSrcOffset)
{
  // Right now we only support null aDestRegion and aSrcOffset (which means)
  // full surface update. Incremental update is only used on Mac so it is
  // not clear that we ever will need to support it for D3D.
  MOZ_ASSERT(!aDestRegion && !aSrcOffset);
  MOZ_ASSERT(aSurface);

  if (!mCompositor || !mCompositor->GetDevice()) {
    return false;
  }

  uint32_t bpp = BytesPerPixel(aSurface->GetFormat());
  DXGI_FORMAT dxgiFormat = SurfaceFormatToDXGIFormat(aSurface->GetFormat());

  mSize = aSurface->GetSize();
  mFormat = aSurface->GetFormat();

  CD3D11_TEXTURE2D_DESC desc(dxgiFormat, mSize.width, mSize.height,
                             1, 1, D3D11_BIND_SHADER_RESOURCE,
                             D3D11_USAGE_IMMUTABLE);

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if ((mSize.width <= maxSize && mSize.height <= maxSize) ||
      (mFlags & TEXTURE_DISALLOW_BIGIMAGE)) {
    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = aSurface->GetData();
    initData.SysMemPitch = aSurface->Stride();

    mCompositor->GetDevice()->CreateTexture2D(&desc, &initData, byRef(mTexture));
    mIsTiled = false;
    if (!mTexture) {
      Reset();
      return false;
    }
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

      D3D11_SUBRESOURCE_DATA initData;
      initData.pSysMem = aSurface->GetData() +
                         tileRect.y * aSurface->Stride() +
                         tileRect.x * bpp;
      initData.SysMemPitch = aSurface->Stride();

      mCompositor->GetDevice()->CreateTexture2D(&desc, &initData, byRef(mTileTextures[i]));
      if (!mTileTextures[i]) {
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
  if (mCompositor != d3dCompositor) {
    Reset();
  }
  mCompositor = d3dCompositor;
}

TemporaryRef<DeprecatedTextureHost>
CreateDeprecatedTextureHostD3D11(SurfaceDescriptorType aDescriptorType,
                                 uint32_t aDeprecatedTextureHostFlags,
                                 uint32_t aTextureFlags)
{
  RefPtr<DeprecatedTextureHost> result;
  if (aDescriptorType == SurfaceDescriptor::TYCbCrImage) {
    result = new DeprecatedTextureHostYCbCrD3D11();
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceDescriptorD3D10) {
    result = new DeprecatedTextureHostDXGID3D11();
  } else {
    result = new DeprecatedTextureHostShmemD3D11();
  }

  result->SetFlags(aTextureFlags);

  return result;
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

DeprecatedTextureClientD3D11::DeprecatedTextureClientD3D11(
  CompositableForwarder* aCompositableForwarder,
  const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aCompositableForwarder, aTextureInfo)
  , mIsLocked(false)
{
  mTextureInfo = aTextureInfo;
}

DeprecatedTextureClientD3D11::~DeprecatedTextureClientD3D11()
{
  mDescriptor = SurfaceDescriptor();

  ClearDT();
}

bool
DeprecatedTextureClientD3D11::EnsureAllocated(gfx::IntSize aSize,
                                              gfxContentType aType)
{
  D3D10_TEXTURE2D_DESC desc;

  if (mTexture) {
    mTexture->GetDesc(&desc);

    if (desc.Width == aSize.width && desc.Height == aSize.height) {
      return true;
    }

    mTexture = nullptr;
    mSurface = nullptr;
    ClearDT();
  }

  mSize = aSize;

  ID3D10Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();

  CD3D10_TEXTURE2D_DESC newDesc(DXGI_FORMAT_B8G8R8A8_UNORM,
                                aSize.width, aSize.height, 1, 1,
                                D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE);

  newDesc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  HRESULT hr = device->CreateTexture2D(&newDesc, nullptr, byRef(mTexture));

  if (FAILED(hr)) {
    LOGD3D11("Error creating texture for client!");
    return false;
  }

  RefPtr<IDXGIResource> resource;
  mTexture->QueryInterface((IDXGIResource**)byRef(resource));

  HANDLE sharedHandle;
  hr = resource->GetSharedHandle(&sharedHandle);

  if (FAILED(hr)) {
    LOGD3D11("Error getting shared handle for texture.");
    return false;
  }

  mDescriptor = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle,
                  gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType));

  mContentType = aType;
  return true;
}

gfxASurface*
DeprecatedTextureClientD3D11::LockSurface()
{
  EnsureSurface();

  LockTexture();
  return mSurface.get();
}

DrawTarget*
DeprecatedTextureClientD3D11::LockDrawTarget()
{
  EnsureDrawTarget();

  LockTexture();
  return mDrawTarget.get();
}

void
DeprecatedTextureClientD3D11::Unlock()
{
  // TODO - Things seem to believe they can hold on to our surface... well...
  // They shouldn't!!
  ReleaseTexture();
}

void
DeprecatedTextureClientD3D11::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() == SurfaceDescriptor::Tnull_t) {
    EnsureAllocated(mSize, mContentType);
    return;
  }

  mDescriptor = aDescriptor;
  mSurface = nullptr;
  ClearDT();

  if (aDescriptor.type() == SurfaceDescriptor::T__None) {
    return;
  }

  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorD3D10);
  ID3D10Device* device = gfxWindowsPlatform::GetPlatform()->GetD3D10Device();

  HRESULT hr = device->OpenSharedResource((HANDLE)aDescriptor.get_SurfaceDescriptorD3D10().handle(),
                                          __uuidof(ID3D10Texture2D),
                                          (void**)(ID3D10Texture2D**)byRef(mTexture));
  NS_WARN_IF_FALSE(mTexture && SUCCEEDED(hr), "Could not open shared resource");
}

void
DeprecatedTextureClientD3D11::EnsureSurface()
{
  if (mSurface) {
    return;
  }

  LockTexture();
  mSurface = new gfxD2DSurface(mTexture, mContentType);
  ReleaseTexture();
}

void
DeprecatedTextureClientD3D11::EnsureDrawTarget()
{
  if (mDrawTarget) {
    return;
  }

  LockTexture();

  SurfaceFormat format;
  switch (mContentType) {
  case gfxContentType::ALPHA:
    format = SurfaceFormat::A8;
    break;
  case gfxContentType::COLOR:
    format = SurfaceFormat::B8G8R8X8;
    break;
  case gfxContentType::COLOR_ALPHA:
    format = SurfaceFormat::B8G8R8A8;
    break;
  default:
    format = SurfaceFormat::B8G8R8A8;
  }

  mDrawTarget = Factory::CreateDrawTargetForD3D10Texture(mTexture, format);
  ReleaseTexture();
}

void
DeprecatedTextureClientD3D11::LockTexture()
{
  RefPtr<IDXGIKeyedMutex> mutex;
  mTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->AcquireSync(0, INFINITE);
  mIsLocked = true;
}

void
DeprecatedTextureClientD3D11::ReleaseTexture()
{
  // TODO - Bas - We seem to have places that unlock without ever having locked,
  // that's kind of bad.
  if (!mIsLocked) {
    return;
  }

  if (mDrawTarget) {
    mDrawTarget->Flush();
  }

  RefPtr<IDXGIKeyedMutex> mutex;
  mTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->ReleaseSync(0);
  mIsLocked = false;
}

void
DeprecatedTextureClientD3D11::ClearDT()
{
  // An Azure DrawTarget needs to be locked when it gets nullptr'ed as this is
  // when it calls EndDraw. This EndDraw should not execute anything so it
  // shouldn't -really- need the lock but the debug layer chokes on this.
  //
  // Perhaps this should be debug only.
  if (mDrawTarget) {
    LockTexture();
    mDrawTarget = nullptr;
    ReleaseTexture();
  }
}

IntSize
DeprecatedTextureHostShmemD3D11::GetSize() const
{
  if (mIterating) {
    gfx::IntRect rect = GetTileRect(mCurrentTile);
    return gfx::IntSize(rect.width, rect.height);
  }
  return TextureSourceD3D11::GetSize();
}

nsIntRect
DeprecatedTextureHostShmemD3D11::GetTileRect()
{
  IntRect rect = GetTileRect(mCurrentTile);
  return nsIntRect(rect.x, rect.y, rect.width, rect.height);
}

void
DeprecatedTextureHostShmemD3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = static_cast<CompositorD3D11*>(aCompositor);
  mDevice = d3dCompositor ? d3dCompositor->GetDevice() : nullptr;
}

void
DeprecatedTextureHostShmemD3D11::UpdateImpl(const SurfaceDescriptor& aImage,
                                            nsIntRegion* aRegion,
                                            nsIntPoint* aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TShmem ||
             aImage.type() == SurfaceDescriptor::TMemoryImage);

  AutoOpenSurface openSurf(OPEN_READ_ONLY, aImage);

  nsRefPtr<gfxImageSurface> surf = openSurf.GetAsImage();

  gfx::IntSize size = gfx::ToIntSize(surf->GetSize());

  uint32_t bpp = 0;

  DXGI_FORMAT dxgiFormat;
  switch (surf->Format()) {
  case gfxImageFormat::RGB24:
    mFormat = SurfaceFormat::B8G8R8X8;
    dxgiFormat = DXGI_FORMAT_B8G8R8X8_UNORM;
    bpp = 4;
    break;
  case gfxImageFormat::ARGB32:
    mFormat = SurfaceFormat::B8G8R8A8;
    dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
    break;
  case gfxImageFormat::A8:
    mFormat = SurfaceFormat::A8;
    dxgiFormat = DXGI_FORMAT_A8_UNORM;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  mSize = size;

  CD3D11_TEXTURE2D_DESC desc(dxgiFormat, size.width, size.height,
                             1, 1, D3D11_BIND_SHADER_RESOURCE,
                             D3D11_USAGE_IMMUTABLE);

  int32_t maxSize = GetMaxTextureSizeForFeatureLevel(mDevice->GetFeatureLevel());
  if (size.width <= maxSize && size.height <= maxSize) {
    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = surf->Data();
    initData.SysMemPitch = surf->Stride();

    mDevice->CreateTexture2D(&desc, &initData, byRef(mTexture));
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D11(size.width, maxSize) *
                         GetRequiredTilesD3D11(size.height, maxSize);

    mTileTextures.resize(tileCount);

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);

      desc.Width = tileRect.width;
      desc.Height = tileRect.height;

      D3D11_SUBRESOURCE_DATA initData;
      initData.pSysMem = surf->Data() +
                         tileRect.y * surf->Stride() +
                         tileRect.x * bpp;
      initData.SysMemPitch = surf->Stride();

      mDevice->CreateTexture2D(&desc, &initData, byRef(mTileTextures[i]));
    }
  }
}

IntRect
DeprecatedTextureHostShmemD3D11::GetTileRect(uint32_t aID) const
{
  uint32_t maxSize = GetMaxTextureSizeForFeatureLevel(mDevice->GetFeatureLevel());
  return GetTileRectD3D11(aID, mSize, maxSize);
}

void
DeprecatedTextureHostDXGID3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = static_cast<CompositorD3D11*>(aCompositor);
  mDevice = d3dCompositor ? d3dCompositor->GetDevice() : nullptr;
}

IntSize
DeprecatedTextureHostDXGID3D11::GetSize() const
{
  return TextureSourceD3D11::GetSize();
}

bool
DeprecatedTextureHostDXGID3D11::Lock()
{
  LockTexture();
  return true;
}

void
DeprecatedTextureHostDXGID3D11::Unlock()
{
  ReleaseTexture();
}

void
DeprecatedTextureHostDXGID3D11::UpdateImpl(const SurfaceDescriptor& aImage,
                                           nsIntRegion* aRegion,
                                           nsIntPoint* aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceDescriptorD3D10);

  HRESULT hr =mDevice->OpenSharedResource((HANDLE)aImage.get_SurfaceDescriptorD3D10().handle(),
                                          __uuidof(ID3D11Texture2D),
                                          (void**)(ID3D11Texture2D**)byRef(mTexture));
  if (!mTexture || FAILED(hr)) {
    NS_WARNING("Could not open shared resource");
    mSize = IntSize(0, 0);
    return;
  }

  mFormat = aImage.get_SurfaceDescriptorD3D10().format();

  D3D11_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  mSize = IntSize(desc.Width, desc.Height);
}

void
DeprecatedTextureHostDXGID3D11::LockTexture()
{
  if (!mTexture) {
    return;
  }
  RefPtr<IDXGIKeyedMutex> mutex;
  mTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->AcquireSync(0, INFINITE);
}

void
DeprecatedTextureHostDXGID3D11::ReleaseTexture()
{
  if (!mTexture) {
    return;
  }
  RefPtr<IDXGIKeyedMutex> mutex;
  mTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->ReleaseSync(0);
}

DeprecatedTextureHostYCbCrD3D11::DeprecatedTextureHostYCbCrD3D11()
  : mCompositor(nullptr)
{
  mFormat = SurfaceFormat::YUV;
}

DeprecatedTextureHostYCbCrD3D11::~DeprecatedTextureHostYCbCrD3D11()
{
}

ID3D11Device*
DeprecatedTextureHostYCbCrD3D11::GetDevice()
{
  return mCompositor ? mCompositor->GetDevice() : nullptr;
  NewTextureSource* source = mFirstSource.get();
  while (source) {
    static_cast<DataTextureSourceD3D11*>(source)->SetCompositor(mCompositor);
    source = source->GetNextSibling();
  }
}

void
DeprecatedTextureHostYCbCrD3D11::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D11*>(aCompositor);
  NewTextureSource* source = mFirstSource.get();
  while (source) {
    static_cast<DataTextureSourceD3D11*>(source)->SetCompositor(mCompositor);
    source = source->GetNextSibling();
  }
}

void
DeprecatedTextureHostYCbCrD3D11::UpdateImpl(const SurfaceDescriptor& aImage,
                                  nsIntRegion* aRegion,
                                  nsIntPoint* aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TYCbCrImage);

  YCbCrImageDataDeserializer yuvDeserializer(aImage.get_YCbCrImage().data().get<uint8_t>());

  gfx::IntSize gfxCbCrSize = yuvDeserializer.GetCbCrSize();

  gfx::IntSize size = yuvDeserializer.GetYSize();

  RefPtr<DataTextureSource> srcY;
  RefPtr<DataTextureSource> srcCb;
  RefPtr<DataTextureSource> srcCr;
  if (!mFirstSource) {
    srcY  = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor);
    srcCb = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor);
    srcCr = new DataTextureSourceD3D11(SurfaceFormat::A8, mCompositor);
    mFirstSource = srcY;
    srcY->SetNextSibling(srcCb);
    srcCb->SetNextSibling(srcCr);
  } else {
    MOZ_ASSERT(mFirstSource->GetNextSibling());
    MOZ_ASSERT(mFirstSource->GetNextSibling()->GetNextSibling());
    srcY  = mFirstSource;
    srcCb = mFirstSource->GetNextSibling()->AsDataTextureSource();
    srcCr = mFirstSource->GetNextSibling()->GetNextSibling()->AsDataTextureSource();
  }
  RefPtr<gfx::DataSourceSurface> wrapperY =
    gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetYData(),
                                                  yuvDeserializer.GetYStride(),
                                                  yuvDeserializer.GetYSize(),
                                                  SurfaceFormat::A8);
  RefPtr<gfx::DataSourceSurface> wrapperCb =
    gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCbData(),
                                                  yuvDeserializer.GetCbCrStride(),
                                                  yuvDeserializer.GetCbCrSize(),
                                                  SurfaceFormat::A8);
  RefPtr<gfx::DataSourceSurface> wrapperCr =
    gfx::Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCrData(),
                                                  yuvDeserializer.GetCbCrStride(),
                                                  yuvDeserializer.GetCbCrSize(),
                                                  SurfaceFormat::A8);
  // We don't support partial updates for YCbCr textures
  NS_ASSERTION(!aRegion, "Unsupported partial updates for YCbCr textures");
  if (!srcY->Update(wrapperY) ||
      !srcCb->Update(wrapperCb) ||
      !srcCr->Update(wrapperCr)) {
    NS_WARNING("failed to update the DataTextureSource");
    mFirstSource = nullptr;
    mSize.width = 0;
    mSize.height = 0;
    return;
  }

  mSize = IntSize(size.width, size.height);
}

}
}
