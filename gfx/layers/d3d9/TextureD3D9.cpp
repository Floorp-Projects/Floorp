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
  return result.forget();
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

CompositingRenderTargetD3D9::CompositingRenderTargetD3D9(IDirect3DTexture9* aTexture,
                                                         SurfaceInitMode aInit,
                                                         const gfx::IntRect& aRect)
  : CompositingRenderTarget(aRect.TopLeft())
  , mInitMode(aInit)
  , mInitialized(false)
{
  MOZ_COUNT_CTOR(CompositingRenderTargetD3D9);
  MOZ_ASSERT(aTexture);

  mTextures[0] = aTexture;
  HRESULT hr = mTextures[0]->GetSurfaceLevel(0, getter_AddRefs(mSurface));
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
  aSurface->LockRect(&aLockedRect, NULL, 0);
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
  aDeviceManager->device()->UpdateSurface(aSurface, NULL, dstSurface, NULL);
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

  return texture.forget();
}

void
DeprecatedTextureHostD3D9::Reset()
{
  mSize.width = 0;
  mSize.height = 0;
  mTextures[0] = nullptr;
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
  case gfxImageFormatRGB24:
    mFormat = FORMAT_B8G8R8X8;
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormatARGB32:
    mFormat = FORMAT_B8G8R8A8;
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormatA8:
    mFormat = FORMAT_A8;
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mTextures[0] = DataToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                 surf->Data(), surf->Stride(),
                                 mSize, format, bpp);
    if (!mTextures[0]) {
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
{
  mFormat = gfx::FORMAT_YUV;
}

DeprecatedTextureHostYCbCrD3D9::~DeprecatedTextureHostYCbCrD3D9()
{
}

void
DeprecatedTextureHostYCbCrD3D9::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorD3D9*>(aCompositor);
}

IntSize
DeprecatedTextureHostYCbCrD3D9::GetSize() const
{
  return TextureSourceD3D9::GetSize();
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

  mSize = ToIntSize(yuvDeserializer.GetYSize());
  IntSize cbCrSize = ToIntSize(yuvDeserializer.GetCbCrSize());
  mStereoMode = yuvDeserializer.GetStereoMode();

  DeviceManagerD3D9* deviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  mTextures[0] = DataToTexture(deviceManager,
                               yuvDeserializer.GetYData(),
                               yuvDeserializer.GetYStride(),
                               mSize,
                               D3DFMT_A8, 1);
  mTextures[1] = DataToTexture(deviceManager,
                               yuvDeserializer.GetCbData(),
                               yuvDeserializer.GetCbCrStride(),
                               cbCrSize,
                               D3DFMT_A8, 1);
  mTextures[2] = DataToTexture(deviceManager,
                               yuvDeserializer.GetCrData(),
                               yuvDeserializer.GetCbCrStride(),
                               cbCrSize,
                               D3DFMT_A8, 1);
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

  return texture.forget();
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
    mFormat = FORMAT_B8G8R8X8;
    bpp = 4;
    break;
  case D3DFMT_A8R8G8B8:
    mFormat = FORMAT_B8G8R8A8;
    bpp = 4;
    break;
  case D3DFMT_A8:
    mFormat = FORMAT_A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mIsTiled = false;

    mTextures[0] = TextureToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                    texture, mSize, format);
    if (!mTextures[0]) {
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

  return texture.forget();
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
  case gfxImageFormatRGB24:
    mFormat = FORMAT_B8G8R8X8;
    format = D3DFMT_X8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormatARGB32:
    mFormat = FORMAT_B8G8R8A8;
    format = D3DFMT_A8R8G8B8;
    bpp = 4;
    break;
  case gfxImageFormatA8:
    mFormat = FORMAT_A8;
    format = D3DFMT_A8;
    bpp = 1;
    break;
  default:
    NS_ERROR("Bad image format");
  }

  int32_t maxSize = mCompositor->GetMaxTextureSize();
  if (mSize.width <= maxSize && mSize.height <= maxSize) {
    mTextures[0] = SurfaceToTexture(gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager(),
                                    surf, mSize, format);
    if (!mTextures[0]) {
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
  case GFX_CONTENT_COLOR:
    format = D3DFMT_X8R8G8B8;
    break;
  case GFX_CONTENT_COLOR_ALPHA:
    // fallback to DIB texture client
    return false;
  case GFX_CONTENT_ALPHA:
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
    gfxIntSize size = mSurface->GetSize();
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

}
}
