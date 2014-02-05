/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureD3D9.h"
#include "CompositorD3D9.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "Effects.h"
#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "gfxWindowsPlatform.h"
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

TemporaryRef<DeprecatedTextureHost>
CreateDeprecatedTextureHostD3D9(SurfaceDescriptorType aDescriptorType,
                      uint32_t aDeprecatedTextureHostFlags,
                      uint32_t aTextureFlags)
{
  RefPtr<DeprecatedTextureHost> result;
  if (aDescriptorType == SurfaceDescriptor::TYCbCrImage) {
    result = new DeprecatedTextureHostYCbCrD3D9();
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceDescriptorD3D9) {
    result = new DeprecatedTextureHostSystemMemD3D9();
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceDescriptorDIB) {
    result = new DeprecatedTextureHostDIB();
  } else {
    result = new DeprecatedTextureHostShmemD3D9();
  }

  result->SetFlags(aTextureFlags);
  return result;
}

TextureSourceD3D9::~TextureSourceD3D9()
{
  MOZ_ASSERT(!mCreatingDeviceManager ||
             mCreatingDeviceManager->IsInTextureHostList(this),
             "Inconsistency in list of texture hosts.");
  // Remove ourselves from the list of d3d9 texture hosts.
  if (mPreviousHost) {
    MOZ_ASSERT(mPreviousHost->mNextHost == this);
    mPreviousHost->mNextHost = mNextHost;
  } else if (mCreatingDeviceManager) {
    mCreatingDeviceManager->RemoveTextureListHead(this);
  }
  if (mNextHost) {
    MOZ_ASSERT(mNextHost->mPreviousHost == this);
    mNextHost->mPreviousHost = mPreviousHost;
  }
}

TemporaryRef<TextureHost>
CreateTextureHostD3D9(const SurfaceDescriptor& aDesc,
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
    case SurfaceDescriptor::TSurfaceDescriptorD3D9: {
      result = new TextureHostD3D9(aFlags, aDesc);
      break;
    }
    case SurfaceDescriptor::TSurfaceDescriptorDIB: {
      result = new DIBTextureHostD3D9(aFlags, aDesc);
      break;
    }
    default: {
      NS_WARNING("Unsupported SurfaceDescriptor type");
    }
  }
  return result.forget();
}

static SurfaceFormat
D3D9FormatToSurfaceFormat(_D3DFORMAT format)
{
  switch (format) {
  case D3DFMT_X8R8G8B8:
    return SurfaceFormat::B8G8R8X8;
  case D3DFMT_A8R8G8B8:
    return SurfaceFormat::B8G8R8A8;
  case D3DFMT_A8:
    return SurfaceFormat::A8;
  default:
    NS_ERROR("Bad texture format");
  }
  return SurfaceFormat::UNKNOWN;
}

static _D3DFORMAT
SurfaceFormatToD3D9Format(SurfaceFormat format)
{
  switch (format) {
  case SurfaceFormat::B8G8R8X8:
    return D3DFMT_X8R8G8B8;
  case SurfaceFormat::B8G8R8A8:
    return D3DFMT_A8R8G8B8;
  case SurfaceFormat::A8:
    return D3DFMT_A8;
  default:
    NS_ERROR("Bad texture format");
  }
  return D3DFMT_A8R8G8B8;
}

CompositingRenderTargetD3D9::CompositingRenderTargetD3D9(IDirect3DTexture9* aTexture,
                                                         SurfaceInitMode aInit,
                                                         const gfx::IntRect& aRect)
  : CompositingRenderTarget(aRect.TopLeft())
  , mInitMode(aInit)
  , mInitialized(false)
{
  MOZ_COUNT_CTOR(CompositingRenderTargetD3D9);
  MOZ_ASSERT(aTexture);

  mTexture = aTexture;
  HRESULT hr = mTexture->GetSurfaceLevel(0, getter_AddRefs(mSurface));
  NS_ASSERTION(mSurface, "Couldn't create surface for texture");
  TextureSourceD3D9::SetSize(aRect.Size());
}

CompositingRenderTargetD3D9::CompositingRenderTargetD3D9(IDirect3DSurface9* aSurface,
                                                         SurfaceInitMode aInit,
                                                         const gfx::IntRect& aRect)
  : CompositingRenderTarget(aRect.TopLeft())
  , mSurface(aSurface)
  , mInitMode(aInit)
  , mInitialized(false)
{
  MOZ_COUNT_CTOR(CompositingRenderTargetD3D9);
  MOZ_ASSERT(mSurface);
  TextureSourceD3D9::SetSize(aRect.Size());
}

CompositingRenderTargetD3D9::~CompositingRenderTargetD3D9()
{
  MOZ_COUNT_DTOR(CompositingRenderTargetD3D9);
}

void
CompositingRenderTargetD3D9::BindRenderTarget(IDirect3DDevice9* aDevice)
{
  aDevice->SetRenderTarget(0, mSurface);
  if (!mInitialized &&
      mInitMode == INIT_MODE_CLEAR) {
    mInitialized = true;
    aDevice->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 0, 0);
  }
}

IntSize
CompositingRenderTargetD3D9::GetSize() const
{
  return TextureSourceD3D9::GetSize();
}

DeprecatedTextureHostD3D9::DeprecatedTextureHostD3D9()
  : mIsTiled(false)
  , mCurrentTile(0)
  , mIterating(false)
{
  MOZ_COUNT_CTOR(DeprecatedTextureHostD3D9);
}

DeprecatedTextureHostD3D9::~DeprecatedTextureHostD3D9()
{
  MOZ_COUNT_DTOR(DeprecatedTextureHostD3D9);
}

IntSize
DeprecatedTextureHostD3D9::GetSize() const
{
  if (mIterating) {
    IntRect rect = GetTileRect(mCurrentTile);
    return rect.Size();
  }
  return TextureSourceD3D9::GetSize();
}

nsIntRect
DeprecatedTextureHostD3D9::GetTileRect()
{
  IntRect rect = GetTileRect(mCurrentTile);
  return nsIntRect(rect.x, rect.y, rect.width, rect.height);
}

static uint32_t GetRequiredTilesD3D9(uint32_t aSize, uint32_t aMaxSize)
{
  uint32_t requiredTiles = aSize / aMaxSize;
  if (aSize % aMaxSize) {
    requiredTiles++;
  }
  return requiredTiles;
}

void
DeprecatedTextureHostD3D9::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D9*>(aCompositor);
}

/**
 * Helper method for DataToTexture and SurfaceToTexture.
 * The last three params are out params.
 * Returns the created texture, or null if we fail.
 */
TemporaryRef<IDirect3DTexture9>
TextureSourceD3D9::InitTextures(DeviceManagerD3D9* aDeviceManager,
                                const IntSize &aSize,
                                _D3DFORMAT aFormat,
                                RefPtr<IDirect3DSurface9>& aSurface,
                                D3DLOCKED_RECT& aLockedRect)
{
  if (!aDeviceManager) {
    return nullptr;
  }
  RefPtr<IDirect3DTexture9> result;
  // D3D9Ex doesn't support managed textures and we don't want the hassle even
  // if we don't have Ex. We could use dynamic textures
  // here but since Images are immutable that probably isn't such a great
  // idea.
  result = aDeviceManager->CreateTexture(aSize, aFormat, D3DPOOL_DEFAULT, this);
  if (!result) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> tmpTexture =
    aDeviceManager->CreateTexture(aSize, aFormat, D3DPOOL_SYSTEMMEM, this);
  if (!tmpTexture) {
    return nullptr;
  }

  tmpTexture->GetSurfaceLevel(0, byRef(aSurface));
  aSurface->LockRect(&aLockedRect, nullptr, 0);
  if (!aLockedRect.pBits) {
    NS_WARNING("Could not lock surface");
    return nullptr;
  }

  return result;
}

/**
 * Helper method for DataToTexture and SurfaceToTexture.
 */
static void
FinishTextures(DeviceManagerD3D9* aDeviceManager,
               IDirect3DTexture9* aTexture,
               IDirect3DSurface9* aSurface)
{
  if (!aDeviceManager) {
    return;
  }

  aSurface->UnlockRect();
  nsRefPtr<IDirect3DSurface9> dstSurface;
  aTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
  aDeviceManager->device()->UpdateSurface(aSurface, nullptr, dstSurface,
                                          nullptr);
}

TemporaryRef<IDirect3DTexture9>
TextureSourceD3D9::DataToTexture(DeviceManagerD3D9* aDeviceManager,
                                 unsigned char *aData,
                                 int aStride,
                                 const IntSize &aSize,
                                 _D3DFORMAT aFormat,
                                 uint32_t aBPP)
{
  RefPtr<IDirect3DSurface9> surface;
  D3DLOCKED_RECT lockedRect;
  RefPtr<IDirect3DTexture9> texture = InitTextures(aDeviceManager, aSize, aFormat,
                                                   surface, lockedRect);
  if (!texture) {
    return nullptr;
  }

  uint32_t width = aSize.width * aBPP;

  for (int y = 0; y < aSize.height; y++) {
    memcpy((char*)lockedRect.pBits + lockedRect.Pitch * y,
            aData + aStride * y,
            width);
  }

  FinishTextures(aDeviceManager, texture, surface);

  return texture;
}

void
DeprecatedTextureHostD3D9::Reset()
{
  mSize.width = 0;
  mSize.height = 0;
  mTexture = nullptr;
  mIsTiled = false;
}

void
DeprecatedTextureHostShmemD3D9::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion *aRegion,
                                 nsIntPoint *aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TShmem ||
             aImage.type() == SurfaceDescriptor::TMemoryImage);
  MOZ_ASSERT(mCompositor, "Must have compositor to update.");

  if (!mCompositor->device()) {
    return;
  }

  AutoOpenSurface openSurf(OPEN_READ_ONLY, aImage);

  nsRefPtr<gfxImageSurface> surf = openSurf.GetAsImage();

  mSize = ToIntSize(surf->GetSize());

  uint32_t bpp = 0;

  _D3DFORMAT format = D3DFMT_A8R8G8B8;
  switch (surf->Format()) {
  case gfxImageFormat::RGB24:
    mFormat = SurfaceFormat::B8G8R8X8;
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormat::ARGB32:
    mFormat = SurfaceFormat::B8G8R8A8;
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormat::A8:
    mFormat = SurfaceFormat::A8;
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mTexture = DataToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                             surf->Data(), surf->Stride(),
                             mSize, format, bpp);
    if (!mTexture) {
      NS_WARNING("Could not upload texture");
      Reset();
      return;
    }
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D9(mSize.width, maxSize) *
                         GetRequiredTilesD3D9(mSize.height, maxSize);
    mTileTextures.resize(tileCount);

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      unsigned char* data = surf->Data() +
                            tileRect.y * surf->Stride() +
                            tileRect.x * bpp;
      mTileTextures[i] = DataToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                       data,
                                       surf->Stride(),
                                       tileRect.Size(),
                                       format,
                                       bpp);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        Reset();
        return;
      }
    }
  }
}

IntRect
DeprecatedTextureHostD3D9::GetTileRect(uint32_t aID) const
{
  uint32_t maxSize = mCompositor->GetMaxTextureSize();
  uint32_t horizontalTiles = GetRequiredTilesD3D9(mSize.width, maxSize);
  uint32_t verticalTiles = GetRequiredTilesD3D9(mSize.height, maxSize);

  uint32_t verticalTile = aID / horizontalTiles;
  uint32_t horizontalTile = aID % horizontalTiles;

  return IntRect(horizontalTile * maxSize,
                 verticalTile * maxSize,
                 horizontalTile < (horizontalTiles - 1) ? maxSize : mSize.width % maxSize,
                 verticalTile < (verticalTiles - 1) ? maxSize : mSize.height % maxSize);
}

DeprecatedTextureHostYCbCrD3D9::DeprecatedTextureHostYCbCrD3D9()
  : mCompositor(nullptr)
{
  mFormat = SurfaceFormat::YUV;

  MOZ_COUNT_CTOR(DeprecatedTextureHostYCbCrD3D9);
}

DeprecatedTextureHostYCbCrD3D9::~DeprecatedTextureHostYCbCrD3D9()
{
  MOZ_COUNT_DTOR(DeprecatedTextureHostYCbCrD3D9);
}

void
DeprecatedTextureHostYCbCrD3D9::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D9*>(aCompositor);
}

IntSize
DeprecatedTextureHostYCbCrD3D9::GetSize() const
{
  return mSize;
}

void
DeprecatedTextureHostYCbCrD3D9::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion *aRegion,
                                 nsIntPoint *aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TYCbCrImage);

  if (!mCompositor->device()) {
    return;
  }

  YCbCrImageDataDeserializer yuvDeserializer(aImage.get_YCbCrImage().data().get<uint8_t>());

  mSize = yuvDeserializer.GetYSize();
  IntSize cbCrSize = yuvDeserializer.GetCbCrSize();
  mStereoMode = yuvDeserializer.GetStereoMode();

  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  RefPtr<DataTextureSource> srcY;
  RefPtr<DataTextureSource> srcCb;
  RefPtr<DataTextureSource> srcCr;
  if (!mFirstSource) {
    srcY  = new DataTextureSourceD3D9(SurfaceFormat::A8, mCompositor,
                                      TEXTURE_DISALLOW_BIGIMAGE, mStereoMode);
    srcCb = new DataTextureSourceD3D9(SurfaceFormat::A8, mCompositor,
                                      TEXTURE_DISALLOW_BIGIMAGE, mStereoMode);
    srcCr = new DataTextureSourceD3D9(SurfaceFormat::A8, mCompositor,
                                      TEXTURE_DISALLOW_BIGIMAGE, mStereoMode);
    mFirstSource = srcY;
    srcY->SetNextSibling(srcCb);
    srcCb->SetNextSibling(srcCr);
  } else {
    MOZ_ASSERT(mFirstSource->GetNextSibling());
    MOZ_ASSERT(mFirstSource->GetNextSibling()->GetNextSibling());
    srcY = mFirstSource;
    srcCb = mFirstSource->GetNextSibling()->AsDataTextureSource();
    srcCr = mFirstSource->GetNextSibling()->GetNextSibling()->AsDataTextureSource();
  }

  RefPtr<DataSourceSurface> wrapperY =
    Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetYData(),
                                             yuvDeserializer.GetYStride(),
                                             yuvDeserializer.GetYSize(),
                                             SurfaceFormat::A8);
  RefPtr<DataSourceSurface> wrapperCb =
    Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCbData(),
                                             yuvDeserializer.GetCbCrStride(),
                                             yuvDeserializer.GetCbCrSize(),
                                             SurfaceFormat::A8);
  RefPtr<DataSourceSurface> wrapperCr =
    Factory::CreateWrappingDataSourceSurface(yuvDeserializer.GetCrData(),
                                             yuvDeserializer.GetCbCrStride(),
                                             yuvDeserializer.GetCbCrSize(),
                                             SurfaceFormat::A8);
  // We don't support partial updates for YCbCr textures
  NS_ASSERTION(!aRegion, "Unsupported partial updates for YCbCr textures");
  if (!srcY->Update(wrapperY) ||
      !srcCb->Update(wrapperCb) ||
      !srcCr->Update(wrapperCr)) {
    NS_WARNING("failed to update the DataTextureSource");
    mSize.width = 0;
    mSize.height = 0;
    mFirstSource = nullptr;
    return;
  }
}

TemporaryRef<IDirect3DTexture9>
TextureSourceD3D9::TextureToTexture(DeviceManagerD3D9* aDeviceManager,
                                    IDirect3DTexture9* aTexture,
                                    const IntSize& aSize,
                                    _D3DFORMAT aFormat)
{
  if (!aDeviceManager) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture =
    aDeviceManager->CreateTexture(aSize, aFormat, D3DPOOL_DEFAULT, this);
  if (!texture) {
    return nullptr;
  }

  HRESULT hr = aDeviceManager->device()->UpdateTexture(aTexture, texture);
  if (FAILED(hr)) {
    return nullptr;
  }

  return texture;
}

void
DeprecatedTextureHostSystemMemD3D9::UpdateImpl(const SurfaceDescriptor& aImage,
                                     nsIntRegion *aRegion,
                                     nsIntPoint *aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceDescriptorD3D9);
  MOZ_ASSERT(mCompositor, "Must have compositor to update.");

  if (!mCompositor->device()) {
    return;
  }

  IDirect3DTexture9* texture =
    reinterpret_cast<IDirect3DTexture9*>(aImage.get_SurfaceDescriptorD3D9().texture());

  if (!texture) {
    Reset();
    return;
  }

  D3DSURFACE_DESC desc;
  texture->GetLevelDesc(0, &desc);
  HRESULT hr = texture->GetLevelDesc(0, &desc);
  if (FAILED(hr)) {
    Reset();
    return;
  }

  mSize.width = desc.Width;
  mSize.height = desc.Height;

  _D3DFORMAT format = desc.Format;
  uint32_t bpp = 0;
  switch (format) {
  case D3DFMT_X8R8G8B8:
    mFormat = SurfaceFormat::B8G8R8X8;
    bpp = 4;
    break;
  case D3DFMT_A8R8G8B8:
    mFormat = SurfaceFormat::B8G8R8A8;
    bpp = 4;
    break;
  case D3DFMT_A8:
    mFormat = SurfaceFormat::A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mIsTiled = false;

    mTexture = TextureToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                texture, mSize, format);
    if (!mTexture) {
      NS_WARNING("Could not upload texture");
      Reset();
      return;
    }
  } else {
    mIsTiled = true;

    uint32_t tileCount = GetRequiredTilesD3D9(mSize.width, maxSize) *
                         GetRequiredTilesD3D9(mSize.height, maxSize);
    mTileTextures.resize(tileCount);

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      RECT d3dTileRect;
      d3dTileRect.left = tileRect.x;
      d3dTileRect.top = tileRect.y;
      d3dTileRect.right = tileRect.XMost();
      d3dTileRect.bottom = tileRect.YMost();
      D3DLOCKED_RECT lockedRect;
      texture->LockRect(0, &lockedRect, &d3dTileRect, 0);
      mTileTextures[i] = DataToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                       reinterpret_cast<unsigned char*>(lockedRect.pBits),
                                       lockedRect.Pitch,
                                       tileRect.Size(),
                                       format,
                                       bpp);
      texture->UnlockRect(0);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        Reset();
        return;
      }
    }
  }
}

TemporaryRef<IDirect3DTexture9>
TextureSourceD3D9::SurfaceToTexture(DeviceManagerD3D9* aDeviceManager,
                                    gfxWindowsSurface* aSurface,
                                    const IntSize& aSize,
                                    _D3DFORMAT aFormat)
{
  RefPtr<IDirect3DSurface9> surface;
  D3DLOCKED_RECT lockedRect;

  RefPtr<IDirect3DTexture9> texture = InitTextures(aDeviceManager, aSize, aFormat,
                                                   surface, lockedRect);
  if (!texture) {
    return nullptr;
  }

  nsRefPtr<gfxImageSurface> imgSurface =
    new gfxImageSurface(reinterpret_cast<unsigned char*>(lockedRect.pBits),
                        gfxIntSize(aSize.width, aSize.height),
                        lockedRect.Pitch,
                        gfxPlatform::GetPlatform()->OptimalFormatForContent(aSurface->GetContentType()));

  nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
  context->SetSource(aSurface);
  context->SetOperator(gfxContext::OPERATOR_SOURCE);
  context->Paint();

  FinishTextures(aDeviceManager, texture, surface);

  return texture;
}

void
DeprecatedTextureHostDIB::UpdateImpl(const SurfaceDescriptor& aImage,
                                     nsIntRegion *aRegion,
                                     nsIntPoint *aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceDescriptorDIB);
  MOZ_ASSERT(mCompositor, "Must have compositor to update.");

  if (!mCompositor->device()) {
    return;
  }

  // We added an extra ref for transport, so we shouldn't AddRef now.
  nsRefPtr<gfxWindowsSurface> surf =
    dont_AddRef(reinterpret_cast<gfxWindowsSurface*>(aImage.get_SurfaceDescriptorDIB().surface()));

  mSize = ToIntSize(surf->GetSize());

  uint32_t bpp = 0;

  _D3DFORMAT format = D3DFMT_A8R8G8B8;
  switch (gfxPlatform::GetPlatform()->OptimalFormatForContent(surf->GetContentType())) {
  case gfxImageFormat::RGB24:
    mFormat = SurfaceFormat::B8G8R8X8;
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormat::ARGB32:
    mFormat = SurfaceFormat::B8G8R8A8;
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormat::A8:
    mFormat = SurfaceFormat::A8;
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mTexture = SurfaceToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                surf, mSize, format);
    if (!mTexture) {
      NS_WARNING("Could not upload texture");
      Reset();
      return;
    }
    mIsTiled = false;
  } else {
    mIsTiled = true;

    uint32_t tileCount = GetRequiredTilesD3D9(mSize.width, maxSize) *
                         GetRequiredTilesD3D9(mSize.height, maxSize);
    mTileTextures.resize(tileCount);

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      nsRefPtr<gfxImageSurface> imgSurface = surf->GetAsImageSurface();
      unsigned char* data = imgSurface->Data() +
                            tileRect.y * imgSurface->Stride() +
                            tileRect.x * bpp;
      mTileTextures[i] = DataToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                       data,
                                       imgSurface->Stride(),
                                       tileRect.Size(),
                                       format,
                                       bpp);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        Reset();
        return;
      }
    }
  }
}

DeprecatedTextureClientD3D9::DeprecatedTextureClientD3D9(CompositableForwarder* aCompositableForwarder,
                                     const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aCompositableForwarder, aTextureInfo)
{
  MOZ_COUNT_CTOR(DeprecatedTextureClientD3D9);
}

DeprecatedTextureClientD3D9::~DeprecatedTextureClientD3D9()
{
  MOZ_COUNT_DTOR(DeprecatedTextureClientD3D9);
  Unlock();
  mDescriptor = SurfaceDescriptor();

  mDrawTarget = nullptr;
}

bool
DeprecatedTextureClientD3D9::EnsureAllocated(gfx::IntSize aSize,
                                   gfxContentType aType)
{
  if (mTexture) {
    D3DSURFACE_DESC desc;
    mTexture->GetLevelDesc(0, &desc);

    if (desc.Width == aSize.width &&
        desc.Height == aSize.height) {
      return true;
    }

    Unlock();
    mD3D9Surface = nullptr;
    mTexture = nullptr;
  }

  mSize = aSize;

  _D3DFORMAT format = D3DFMT_A8R8G8B8;
  switch (aType) {
  case gfxContentType::COLOR:
    format = D3DFMT_X8R8G8B8;
    break;
  case gfxContentType::COLOR_ALPHA:
    // fallback to DIB texture client
    return false;
  case gfxContentType::ALPHA:
    format = D3DFMT_A8;
    break;
  default:
    NS_ERROR("Bad image type");
  }

  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if (!deviceManager ||
      !(mTexture = deviceManager->CreateTexture(aSize, format, D3DPOOL_SYSTEMMEM, nullptr))) {
    NS_WARNING("Could not create texture");
    return false;
  }

  MOZ_ASSERT(mTexture);
  mDescriptor = SurfaceDescriptorD3D9(reinterpret_cast<uintptr_t>(mTexture.get()));

  mContentType = aType;
  return true;
}

gfxASurface*
DeprecatedTextureClientD3D9::LockSurface()
{
  if (!gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
    // If the device has failed then we should not lock the surface,
    // even if we could.
    Unlock();
    mD3D9Surface = nullptr;
    mTexture = nullptr;

    return nullptr;
  }

  if (mSurface) {
    return mSurface.get();
  }

  if (!mTexture) {
    NS_WARNING("No texture to lock");
    return nullptr;
  }

  if (!mD3D9Surface) {
    HRESULT hr = mTexture->GetSurfaceLevel(0, getter_AddRefs(mD3D9Surface));
    if (FAILED(hr)) {
      NS_WARNING("Failed to get texture surface level.");
      return nullptr;
    }
  }

  mSurface = new gfxWindowsSurface(mD3D9Surface);
  if (!mSurface || mSurface->CairoStatus()) {
    NS_WARNING("Could not create surface for d3d9 surface");
    mSurface = nullptr;
    return nullptr;
  }

  return mSurface.get();
}

DrawTarget*
DeprecatedTextureClientD3D9::LockDrawTarget()
{
  if (!mDrawTarget) {
    if (gfxASurface* surface = LockSurface()) {
      mDrawTarget =
        gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(LockSurface(), mSize);
    }
  }

  return mDrawTarget.get();
}

void
DeprecatedTextureClientD3D9::Unlock()
{
  if (mDrawTarget) {
    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }

  if (mSurface) {
    mSurface = nullptr;
  }
}

void
DeprecatedTextureClientD3D9::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() == SurfaceDescriptor::Tnull_t) {
    EnsureAllocated(mSize, mContentType);
    return;
  }

  mDescriptor = aDescriptor;
  mSurface = nullptr;
  mDrawTarget = nullptr;

  if (aDescriptor.type() == SurfaceDescriptor::T__None) {
    return;
  }

  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorD3D9);
  Unlock();
  mD3D9Surface = nullptr;
  mTexture = reinterpret_cast<IDirect3DTexture9*>(
               mDescriptor.get_SurfaceDescriptorD3D9().texture());
}

DeprecatedTextureClientDIB::DeprecatedTextureClientDIB(CompositableForwarder* aCompositableForwarder,
                                                       const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aCompositableForwarder, aTextureInfo)
{
  MOZ_COUNT_CTOR(DeprecatedTextureClientDIB);
}

DeprecatedTextureClientDIB::~DeprecatedTextureClientDIB()
{
  MOZ_COUNT_DTOR(DeprecatedTextureClientDIB);
  Unlock();
  // It is OK not to dealloc the surface descriptor because it is only a pointer
  // to mSurface and we will release our strong reference to that automatically.
  mDescriptor = SurfaceDescriptor();
  mDrawTarget = nullptr;
}

bool
DeprecatedTextureClientDIB::EnsureAllocated(gfx::IntSize aSize,
                                            gfxContentType aType)
{
  if (mSurface) {
    gfx::IntSize size = gfx::ToIntSize(mSurface->GetSize());
    if (size.width == aSize.width &&
        size.height == aSize.height) {
      return true;
    }

    Unlock();
    mSurface = nullptr;
  }

  mSurface = new gfxWindowsSurface(gfxIntSize(aSize.width, aSize.height),
                                   gfxPlatform::GetPlatform()->OptimalFormatForContent(aType));
  if (!mSurface || mSurface->CairoStatus())
  {
    NS_WARNING("Could not create surface");
    mSurface = nullptr;
    mDescriptor = SurfaceDescriptor();
    return false;
  }
  mSize = aSize;
  mContentType = aType;

  mDescriptor = SurfaceDescriptorDIB(reinterpret_cast<uintptr_t>(mSurface.get()));

  return true;
}

SurfaceDescriptor*
DeprecatedTextureClientDIB::LockSurfaceDescriptor()
{
  // The host will release this ref when it receives the surface descriptor.
  // We AddRef in case we die before the host receives the pointer.
  NS_ASSERTION(mSurface == reinterpret_cast<gfxWindowsSurface*>(mDescriptor.get_SurfaceDescriptorDIB().surface()),
                "SurfaceDescriptor is not up to date");
  mSurface->AddRef();
  return GetDescriptor();
}

gfxASurface*
DeprecatedTextureClientDIB::LockSurface()
{
  if (mSurface) {
    return mSurface.get();
  }

  return nullptr;
}

DrawTarget*
DeprecatedTextureClientDIB::LockDrawTarget()
{
  if (!mDrawTarget) {
    if (gfxASurface* surface = LockSurface()) {
      mDrawTarget =
        gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(LockSurface(), mSize);
    }
  }

  return mDrawTarget.get();
}

void
DeprecatedTextureClientDIB::Unlock()
{
  if (mDrawTarget) {
    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }
}

void
DeprecatedTextureClientDIB::SetDescriptor(const SurfaceDescriptor& aDescriptor)
{
  mDescriptor = aDescriptor;
  mDrawTarget = nullptr;

  if (aDescriptor.type() == SurfaceDescriptor::T__None) {
    mSurface = nullptr;
    return;
  }

  MOZ_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorDIB);
  Unlock();
  mSurface = reinterpret_cast<gfxWindowsSurface*>(
               mDescriptor.get_SurfaceDescriptorDIB().surface());
}

class D3D9TextureClientData : public TextureClientData
{
public:
  D3D9TextureClientData(IDirect3DTexture9* aTexture)
    : mTexture(aTexture)
  {}

  D3D9TextureClientData(gfxWindowsSurface* aWindowSurface)
    : mWindowSurface(aWindowSurface)
  {}

  virtual void DeallocateSharedData(ISurfaceAllocator*) MOZ_OVERRIDE
  {
    mWindowSurface = nullptr;
    mTexture = nullptr;
  }

private:
  RefPtr<IDirect3DTexture9> mTexture;
  nsRefPtr<gfxWindowsSurface> mWindowSurface;
};

DataTextureSourceD3D9::DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                                             CompositorD3D9* aCompositor,
                                             TextureFlags aFlags,
                                             StereoMode aStereoMode)
  : mFormat(aFormat)
  , mCompositor(aCompositor)
  , mCurrentTile(0)
  , mFlags(aFlags)
  , mIsTiled(false)
  , mIterating(false)
{
  mStereoMode = aStereoMode;
  MOZ_COUNT_CTOR(DataTextureSourceD3D9);
}

DataTextureSourceD3D9::DataTextureSourceD3D9(gfx::SurfaceFormat aFormat,
                                             gfx::IntSize aSize,
                                             CompositorD3D9* aCompositor,
                                             IDirect3DTexture9* aTexture,
                                             TextureFlags aFlags)
  : mFormat(aFormat)
  , mCompositor(aCompositor)
  , mCurrentTile(0)
  , mFlags(aFlags)
  , mIsTiled(false)
  , mIterating(false)
{
  mSize = aSize;
  mTexture = aTexture;
  mStereoMode = StereoMode::MONO;
  MOZ_COUNT_CTOR(DataTextureSourceD3D9);
}

DataTextureSourceD3D9::~DataTextureSourceD3D9()
{
  MOZ_COUNT_DTOR(DataTextureSourceD3D9);
}

IDirect3DTexture9*
DataTextureSourceD3D9::GetD3D9Texture()
{
  return mIterating ? mTileTextures[mCurrentTile]
                    : mTexture;
}

bool
DataTextureSourceD3D9::Update(gfx::DataSourceSurface* aSurface,
                              nsIntRegion* aDestRegion,
                              gfx::IntPoint* aSrcOffset)
{
  // Right now we only support null aDestRegion and aSrcOffset (which means
  // full surface update). Incremental update is only used on Mac so it is
  // not clear that we ever will need to support it for D3D.
  MOZ_ASSERT(!aDestRegion && !aSrcOffset);

  if (!mCompositor || !mCompositor->device()) {
    NS_WARNING("No D3D device to update the texture.");
    return false;
  }
  mSize = aSurface->GetSize();

  uint32_t bpp = 0;

  _D3DFORMAT format = D3DFMT_A8R8G8B8;
  mFormat = aSurface->GetFormat();
  switch (mFormat) {
  case SurfaceFormat::B8G8R8X8:
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case SurfaceFormat::B8G8R8A8:
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case SurfaceFormat::A8:
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_WARNING("Bad image format");
    return false;
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if ((mSize.width <= maxSize && mSize.height <= maxSize) ||
      (mFlags & TEXTURE_DISALLOW_BIGIMAGE)) {
    mTexture = DataToTexture(deviceManager,
                             aSurface->GetData(), aSurface->Stride(),
                             IntSize(mSize), format, bpp);
    if (!mTexture) {
      NS_WARNING("Could not upload texture");
      Reset();
      return false;
    }
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D9(mSize.width, maxSize) *
                         GetRequiredTilesD3D9(mSize.height, maxSize);
    mTileTextures.resize(tileCount);
    mTexture = nullptr;

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      unsigned char* data = aSurface->GetData() +
                            tileRect.y * aSurface->Stride() +
                            tileRect.x * bpp;
      mTileTextures[i] = DataToTexture(deviceManager,
                                       data,
                                       aSurface->Stride(),
                                       IntSize(tileRect.width, tileRect.height),
                                       format,
                                       bpp);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        Reset();
        return false;
      }
    }
  }

  return true;
}

bool
DataTextureSourceD3D9::Update(gfxWindowsSurface* aSurface)
{
  MOZ_ASSERT(aSurface);
  if (!mCompositor || !mCompositor->device()) {
    NS_WARNING("No D3D device to update the texture.");
    return false;
  }
  mSize = ToIntSize(aSurface->GetSize());

  uint32_t bpp = 0;

  _D3DFORMAT format = D3DFMT_A8R8G8B8;
  mFormat = ImageFormatToSurfaceFormat(
    gfxPlatform::GetPlatform()->OptimalFormatForContent(aSurface->GetContentType()));
  switch (mFormat) {
  case SurfaceFormat::B8G8R8X8:
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case SurfaceFormat::B8G8R8A8:
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case SurfaceFormat::A8:
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_WARNING("Bad image format");
    return false;
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if ((mSize.width <= maxSize && mSize.height <= maxSize) ||
      (mFlags & TEXTURE_DISALLOW_BIGIMAGE)) {
    mTexture = SurfaceToTexture(deviceManager, aSurface, mSize, format);

    if (!mTexture) {
      NS_WARNING("Could not upload texture");
      Reset();
      return false;
    }
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D9(mSize.width, maxSize) *
                         GetRequiredTilesD3D9(mSize.height, maxSize);
    mTileTextures.resize(tileCount);
    mTexture = nullptr;
    nsRefPtr<gfxImageSurface> imgSurface = aSurface->GetAsImageSurface();

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      unsigned char* data = imgSurface->Data() +
                            tileRect.y * imgSurface->Stride() +
                            tileRect.x * bpp;
      mTileTextures[i] = DataToTexture(deviceManager,
                                       data,
                                       imgSurface->Stride(),
                                       IntSize(tileRect.width, tileRect.height),
                                       format,
                                       bpp);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        Reset();
        return false;
      }
    }
  }

  return true;
}

void
DataTextureSourceD3D9::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D9* d3dCompositor = static_cast<CompositorD3D9*>(aCompositor);

  if (d3dCompositor != mCompositor) {
    Reset();
    mCompositor = d3dCompositor;
  }
}

void
DataTextureSourceD3D9::Reset()
{
  mSize.width = 0;
  mSize.height = 0;
  mIsTiled = false;
  mTexture = nullptr;
  mTileTextures.clear();
}

IntRect
DataTextureSourceD3D9::GetTileRect(uint32_t aTileIndex) const
{
  uint32_t maxSize = mCompositor->GetMaxTextureSize();
  uint32_t horizontalTiles = GetRequiredTilesD3D9(mSize.width, maxSize);
  uint32_t verticalTiles = GetRequiredTilesD3D9(mSize.height, maxSize);

  uint32_t verticalTile = aTileIndex / horizontalTiles;
  uint32_t horizontalTile = aTileIndex % horizontalTiles;

  return IntRect(horizontalTile * maxSize,
                 verticalTile * maxSize,
                 horizontalTile < (horizontalTiles - 1) ? maxSize : mSize.width % maxSize,
                 verticalTile < (verticalTiles - 1) ? maxSize : mSize.height % maxSize);
}

nsIntRect
DataTextureSourceD3D9::GetTileRect()
{
  return ThebesIntRect(GetTileRect(mCurrentTile));
}

CairoTextureClientD3D9::CairoTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags)
  , mFormat(aFormat)
  , mIsLocked(false)
  , mNeedsClear(false)
{
  MOZ_COUNT_CTOR(CairoTextureClientD3D9);
}

CairoTextureClientD3D9::~CairoTextureClientD3D9()
{
  MOZ_COUNT_DTOR(CairoTextureClientD3D9);
}

bool
CairoTextureClientD3D9::Lock(OpenMode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid() || !IsAllocated()) {
    return false;
  }
  if (mNeedsClear) {
    mDrawTarget = GetAsDrawTarget();
    mDrawTarget->ClearRect(Rect(0, 0, GetSize().width, GetSize().height));
  }
  mIsLocked = true;
  return true;
}

void
CairoTextureClientD3D9::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (mDrawTarget) {
    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }

  if (mSurface) {
    mSurface = nullptr;
  }
  mIsLocked = false;
}

bool
CairoTextureClientD3D9::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorD3D9(reinterpret_cast<uintptr_t>(mTexture.get()));
  return true;
}

TemporaryRef<gfx::DrawTarget>
CairoTextureClientD3D9::GetAsDrawTarget()
{
  MOZ_ASSERT(mIsLocked && mTexture);
  if (mDrawTarget) {
    return mDrawTarget;
  }

  if (!gfxWindowsPlatform::GetPlatform()->GetD3D9Device()) {
    // If the device has failed then we should not lock the surface,
    // even if we could.
    mD3D9Surface = nullptr;
    mTexture = nullptr;
    return nullptr;
  }

  if (!mD3D9Surface) {
    HRESULT hr = mTexture->GetSurfaceLevel(0, getter_AddRefs(mD3D9Surface));
    if (FAILED(hr)) {
      NS_WARNING("Failed to get texture surface level.");
      return nullptr;
    }
  }

  mSurface = new gfxWindowsSurface(mD3D9Surface);
  if (!mSurface || mSurface->CairoStatus()) {
    NS_WARNING("Could not create surface for d3d9 surface");
    mSurface = nullptr;
    return nullptr;
  }

  mDrawTarget =
    gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mSurface, mSize);

  return mDrawTarget;
}

bool
CairoTextureClientD3D9::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(!IsAllocated());
  mSize = aSize;
  _D3DFORMAT format = SurfaceFormatToD3D9Format(mFormat);

  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if (!deviceManager ||
      !(mTexture = deviceManager->CreateTexture(mSize, format, D3DPOOL_SYSTEMMEM, nullptr))) {
    NS_WARNING("Could not create d3d9 texture");
    return false;
  }

  mNeedsClear = aFlags & ALLOC_CLEAR_BUFFER;

  MOZ_ASSERT(mTexture);
  return true;
}

TextureClientData*
CairoTextureClientD3D9::DropTextureData()
{
  TextureClientData* data = new D3D9TextureClientData(mTexture);
  mTexture = nullptr;
  MarkInvalid();
  return data;
}

DIBTextureClientD3D9::DIBTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags)
  , mFormat(aFormat)
  , mIsLocked(false)
{
  MOZ_COUNT_CTOR(DIBTextureClientD3D9);
}

DIBTextureClientD3D9::~DIBTextureClientD3D9()
{
  MOZ_COUNT_DTOR(DIBTextureClientD3D9);
}

bool
DIBTextureClientD3D9::Lock(OpenMode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
DIBTextureClientD3D9::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (mDrawTarget) {
    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }

  mIsLocked = false;
}

bool
DIBTextureClientD3D9::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  MOZ_ASSERT(mSurface);
  // The host will release this ref when it receives the surface descriptor.
  // We AddRef in case we die before the host receives the pointer.
  aOutDescriptor = SurfaceDescriptorDIB(reinterpret_cast<uintptr_t>(mSurface.get()));
  mSurface->AddRef();
  return true;
}

TemporaryRef<gfx::DrawTarget>
DIBTextureClientD3D9::GetAsDrawTarget()
{
  MOZ_ASSERT(mIsLocked && IsAllocated());

  if (!mDrawTarget) {
    mDrawTarget =
      gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mSurface, mSize);
  }

  return mDrawTarget;
}

bool
DIBTextureClientD3D9::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(!IsAllocated());
  mSize = aSize;

  mSurface = new gfxWindowsSurface(gfxIntSize(aSize.width, aSize.height),
                                   SurfaceFormatToImageFormat(mFormat));
  if (!mSurface || mSurface->CairoStatus())
  {
    NS_WARNING("Could not create surface");
    mSurface = nullptr;
    return false;
  }

  return true;
}

TextureClientData*
DIBTextureClientD3D9::DropTextureData()
{
  TextureClientData* data = new D3D9TextureClientData(mSurface);
  mSurface = nullptr;
  MarkInvalid();
  return data;
}

SharedTextureClientD3D9::SharedTextureClientD3D9(gfx::SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags)
  , mFormat(aFormat)
  , mHandle(0)
  , mIsLocked(false)
{
  MOZ_COUNT_CTOR(SharedTextureClientD3D9);
}

SharedTextureClientD3D9::~SharedTextureClientD3D9()
{
  MOZ_COUNT_DTOR(SharedTextureClientD3D9);
}

TextureClientData*
SharedTextureClientD3D9::DropTextureData()
{
  TextureClientData* data = new D3D9TextureClientData(mTexture);
  mTexture = nullptr;
  MarkInvalid();
  return data;
}

bool
SharedTextureClientD3D9::Lock(OpenMode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
SharedTextureClientD3D9::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlock called while the texture is not locked!");
}

bool
SharedTextureClientD3D9::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorD3D10((WindowsHandle)(mHandle), mFormat);
  return true;
}

TextureHostD3D9::TextureHostD3D9(TextureFlags aFlags,
                                 const SurfaceDescriptorD3D9& aDescriptor)
  : TextureHost(aFlags)
  , mFormat(SurfaceFormat::UNKNOWN)
  , mIsLocked(false)
{
  mTexture = reinterpret_cast<IDirect3DTexture9*>(aDescriptor.texture());
  MOZ_ASSERT(mTexture);
  D3DSURFACE_DESC desc;
  HRESULT hr = mTexture->GetLevelDesc(0, &desc);
  if (!FAILED(hr)) {
    mFormat = D3D9FormatToSurfaceFormat(desc.Format);
    mSize.width = desc.Width;
    mSize.height = desc.Height;
  }
}

bool
DataTextureSourceD3D9::UpdateFromTexture(IDirect3DTexture9* aTexture,
                                         const nsIntRegion* aRegion)
{
  MOZ_ASSERT(aTexture);

  D3DSURFACE_DESC desc;
  HRESULT hr = aTexture->GetLevelDesc(0, &desc);
  if (!FAILED(hr)) {
    MOZ_ASSERT(mFormat == D3D9FormatToSurfaceFormat(desc.Format));
    MOZ_ASSERT(mSize.width == desc.Width);
    MOZ_ASSERT(mSize.height == desc.Height);
  }

  DeviceManagerD3D9* dm = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if (!mTexture) {
    mTexture = dm->CreateTexture(mSize, SurfaceFormatToD3D9Format(mFormat),
                                 D3DPOOL_DEFAULT, this);
    if (!mTexture) {
      NS_WARNING("Failed to create a texture");
      return false;
    }
  }

  RefPtr<IDirect3DSurface9> srcSurface;
  RefPtr<IDirect3DSurface9> dstSurface;

  hr = aTexture->GetSurfaceLevel(0, byRef(srcSurface));
  if (FAILED(hr)) {
    return false;
  }
  hr = mTexture->GetSurfaceLevel(0, byRef(dstSurface));
  if (FAILED(hr)) {
    return false;
  }

  if (aRegion) {
    nsIntRegionRectIterator iter(*aRegion);
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      RECT rect;
      rect.left = iterRect->x;
      rect.top = iterRect->y;
      rect.right = iterRect->XMost();
      rect.bottom = iterRect->YMost();

      POINT point;
      point.x = iterRect->x;
      point.y = iterRect->y;
      dm->device()->UpdateSurface(srcSurface, &rect, dstSurface, &point);
    }
  } else {
    dm->device()->UpdateSurface(srcSurface, nullptr, dstSurface, nullptr);
  }
  mIsTiled = false;
  return true;
}

void
TextureHostD3D9::Updated(const nsIntRegion* aRegion)
{
  if (!mTexture) {
    return;
  }

  if (!mTextureSource) {
    mTextureSource = new DataTextureSourceD3D9(mFormat, mSize, mCompositor,
                                               nullptr, mFlags);
  }

  mTextureSource->UpdateFromTexture(mTexture, aRegion);
}

IDirect3DDevice9*
TextureHostD3D9::GetDevice()
{
  return mCompositor ? mCompositor->device() : nullptr;
}

void
TextureHostD3D9::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D9*>(aCompositor);
  if (mTextureSource) {
    mTextureSource->SetCompositor(aCompositor);
  }
}

NewTextureSource*
TextureHostD3D9::GetTextureSources()
{
  MOZ_ASSERT(mIsLocked);
  return mTextureSource;
}

bool
TextureHostD3D9::Lock()
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return true;
}

void
TextureHostD3D9::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

void
TextureHostD3D9::DeallocateDeviceData()
{
  mTextureSource = nullptr;
}

DIBTextureHostD3D9::DIBTextureHostD3D9(TextureFlags aFlags,
                                       const SurfaceDescriptorDIB& aDescriptor)
  : TextureHost(aFlags)
  , mIsLocked(false)
{
  // We added an extra ref for transport, so we shouldn't AddRef now.
  mSurface =
    dont_AddRef(reinterpret_cast<gfxWindowsSurface*>(aDescriptor.surface()));
  MOZ_ASSERT(mSurface);

  mSize = ToIntSize(mSurface->GetSize());
  mFormat = ImageFormatToSurfaceFormat(
    gfxPlatform::GetPlatform()->OptimalFormatForContent(mSurface->GetContentType()));
}

NewTextureSource*
DIBTextureHostD3D9::GetTextureSources()
{
  if (!mTextureSource) {
    Updated();
  }

  return mTextureSource;
}

void
DIBTextureHostD3D9::Updated(const nsIntRegion*)
{
  if (!mTextureSource) {
    mTextureSource = new DataTextureSourceD3D9(mFormat, mCompositor, mFlags);
  }

  if (!mTextureSource->Update(mSurface)) {
    mTextureSource = nullptr;
  }
}

bool
DIBTextureHostD3D9::Lock()
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return true;
}

void
DIBTextureHostD3D9::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

void
DIBTextureHostD3D9::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D9*>(aCompositor);
}

void
DIBTextureHostD3D9::DeallocateDeviceData()
{
  mTextureSource = nullptr;
}

}
}
