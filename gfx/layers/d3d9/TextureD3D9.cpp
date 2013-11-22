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
    gfx::IntRect rect = GetTileRect(mCurrentTile);
    return gfx::IntSize(rect.width, rect.height);
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
  mDevice = mCompositor ? mCompositor->device() : nullptr;
}

/**
 * Helper method for DataToTexture and SurfaceToTexture.
 * The last four params are out params.
 * Returns success.
 */
static bool
InitTextures(IDirect3DDevice9* aDevice,
             const gfxIntSize &aSize,
             _D3DFORMAT aFormat,
             RefPtr<IDirect3DTexture9>& aTexture,
             RefPtr<IDirect3DSurface9>& aSurface,
             D3DLOCKED_RECT& aLockedRect,
             bool& aUsingD3D9Ex)
{
  nsRefPtr<IDirect3DDevice9Ex> deviceEx;
  aDevice->QueryInterface(IID_IDirect3DDevice9Ex,
                          (void**)getter_AddRefs(deviceEx));
  aUsingD3D9Ex = !!deviceEx;

  if (aUsingD3D9Ex) {
    // D3D9Ex doesn't support managed textures. We could use dynamic textures
    // here but since Images are immutable that probably isn't such a great
    // idea.
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_DEFAULT,
                             byRef(aTexture), nullptr)))
    {
      return false;
    }

    RefPtr<IDirect3DTexture9> tmpTexture;
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_SYSTEMMEM,
                             byRef(tmpTexture), nullptr)))
    {
      return false;
    }

    tmpTexture->GetSurfaceLevel(0, byRef(aSurface));
    aSurface->LockRect(&aLockedRect, NULL, 0);
    NS_ASSERTION(aLockedRect.pBits, "Could not lock surface");
  } else {
    if (FAILED(aDevice->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, aFormat, D3DPOOL_MANAGED,
                             byRef(aTexture), nullptr))) {
      return false;
    }

    /* lock the entire texture */
    aTexture->LockRect(0, &aLockedRect, nullptr, 0);
  }

  return true;
}

/**
 * Helper method for DataToTexture and SurfaceToTexture.
 */
static void
FinishTextures(IDirect3DDevice9* aDevice,
               RefPtr<IDirect3DTexture9>& aTexture,
               RefPtr<IDirect3DSurface9> aSurface,
               bool aUsingD3D9Ex)
{
  if (aUsingD3D9Ex) {
    aSurface->UnlockRect();
    nsRefPtr<IDirect3DSurface9> dstSurface;
    aTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    aDevice->UpdateSurface(aSurface, NULL, dstSurface, NULL);
  } else {
    aTexture->UnlockRect(0);
  }
}

static TemporaryRef<IDirect3DTexture9>
DataToTexture(IDirect3DDevice9 *aDevice,
              unsigned char *aData,
              int aStride,
              const gfxIntSize &aSize,
              _D3DFORMAT aFormat,
              uint32_t aBPP)
{
  RefPtr<IDirect3DTexture9> texture;
  RefPtr<IDirect3DSurface9> surface;
  D3DLOCKED_RECT lockedRect;
  bool usingD3D9Ex;

  if (!InitTextures(aDevice, aSize, aFormat,
                    texture, surface, lockedRect, usingD3D9Ex)) {
    return nullptr;
  }

  uint32_t width = aSize.width * aBPP;

  for (int y = 0; y < aSize.height; y++) {
    memcpy((char*)lockedRect.pBits + lockedRect.Pitch * y,
            aData + aStride * y,
            width);
  }

  FinishTextures(aDevice, texture, surface, usingD3D9Ex);

  return texture.forget();
}

void
DeprecatedTextureHostShmemD3D9::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion *aRegion,
                                 nsIntPoint *aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TShmem ||
             aImage.type() == SurfaceDescriptor::TMemoryImage);
  MOZ_ASSERT(mCompositor, "Must have compositor to update.");

  AutoOpenSurface openSurf(OPEN_READ_ONLY, aImage);

  nsRefPtr<gfxImageSurface> surf = openSurf.GetAsImage();

  gfxIntSize size = surf->GetSize();
  mSize = IntSize(size.width, size.height);

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
  if (size.width <= maxSize && size.height <= maxSize) {
    mTextures[0] = DataToTexture(mDevice,
                                 surf->Data(), surf->Stride(),
                                 size, format, bpp);
    NS_ASSERTION(mTextures[0], "Could not upload texture");
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTilesD3D9(size.width, maxSize) *
                         GetRequiredTilesD3D9(size.height, maxSize);
    mTileTextures.resize(tileCount);

    for (uint32_t i = 0; i < tileCount; i++) {
      IntRect tileRect = GetTileRect(i);
      unsigned char* data = surf->Data() +
                            tileRect.y * surf->Stride() +
                            tileRect.x * bpp;
      mTileTextures[i] = DataToTexture(mDevice,
                                       data,
                                       surf->Stride(),
                                       gfxIntSize(tileRect.width, tileRect.height),
                                       format,
                                       bpp);
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

void
DeprecatedTextureHostYCbCrD3D9::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D9 *d3dCompositor = static_cast<CompositorD3D9*>(aCompositor);
  mDevice = d3dCompositor ? d3dCompositor->device() : nullptr;
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

  YCbCrImageDataDeserializer yuvDeserializer(aImage.get_YCbCrImage().data().get<uint8_t>());

  gfxIntSize gfxCbCrSize = yuvDeserializer.GetCbCrSize();
  gfxIntSize size = yuvDeserializer.GetYSize();
  mSize = IntSize(size.width, size.height);
  mStereoMode = yuvDeserializer.GetStereoMode();

  mTextures[0] = DataToTexture(mDevice,
                               yuvDeserializer.GetYData(),
                               yuvDeserializer.GetYStride(),
                               size,
                               D3DFMT_A8, 1);
  mTextures[1] = DataToTexture(mDevice,
                               yuvDeserializer.GetCbData(),
                               yuvDeserializer.GetCbCrStride(),
                               gfxCbCrSize,
                               D3DFMT_A8, 1);
  mTextures[2] = DataToTexture(mDevice,
                               yuvDeserializer.GetCrData(),
                               yuvDeserializer.GetCbCrStride(),
                               gfxCbCrSize,
                               D3DFMT_A8, 1);
}

// aTexture should be in SYSTEMMEM, returns a texture in the default
// pool (that is, in video memory).
static TemporaryRef<IDirect3DTexture9>
TextureToTexture(IDirect3DDevice9* aDevice,
                 IDirect3DTexture9* aTexture,
                 const IntSize& aSize,
                 _D3DFORMAT aFormat)
{
  RefPtr<IDirect3DTexture9> texture;
  if (FAILED(aDevice->
              CreateTexture(aSize.width, aSize.height,
                            1, 0, aFormat, D3DPOOL_DEFAULT,
                            byRef(texture), nullptr))) {
    return nullptr;
  }

  HRESULT hr = aDevice->UpdateTexture(aTexture, texture);
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

  IDirect3DTexture9* texture =
    reinterpret_cast<IDirect3DTexture9*>(aImage.get_SurfaceDescriptorD3D9().texture());

  if (!texture) {
    mTextures[0] = nullptr;
    return;
  }

  D3DSURFACE_DESC desc;
  texture->GetLevelDesc(0, &desc);
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

    mTextures[0] = TextureToTexture(mDevice, texture, mSize, format);
    NS_ASSERTION(mTextures[0], "Could not upload texture");
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
      mTileTextures[i] = DataToTexture(mDevice,
                                       reinterpret_cast<unsigned char*>(lockedRect.pBits),
                                       lockedRect.Pitch,
                                       gfxIntSize(tileRect.width, tileRect.height),
                                       format,
                                       bpp);
      texture->UnlockRect(0);
      if (!mTileTextures[i]) {
        NS_WARNING("Could not upload texture");
        mSize.width = 0;
        mSize.height = 0;
        mIsTiled = false;
        return;
      }
    }
  }
}

static TemporaryRef<IDirect3DTexture9>
SurfaceToTexture(IDirect3DDevice9* aDevice,
                 gfxWindowsSurface* aSurface,
                 const gfxIntSize& aSize,
                 _D3DFORMAT aFormat)
{
  RefPtr<IDirect3DTexture9> texture;
  RefPtr<IDirect3DSurface9> surface;
  D3DLOCKED_RECT lockedRect;
  bool usingD3D9Ex;

  if (!InitTextures(aDevice, aSize, aFormat,
                    texture, surface, lockedRect, usingD3D9Ex)) {
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

  FinishTextures(aDevice, texture, surface, usingD3D9Ex);

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

  gfxIntSize size = surf->GetSize();
  mSize = IntSize(size.width, size.height);

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
  if (size.width <= maxSize && size.height <= maxSize) {
    mTextures[0] = SurfaceToTexture(mDevice, surf, size, format);
    NS_ASSERTION(mTextures[0], "Could not upload texture");
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
      mTileTextures[i] = DataToTexture(mDevice,
                                       data,
                                       imgSurface->Stride(),
                                       gfxIntSize(tileRect.width, tileRect.height),
                                       format,
                                       bpp);
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

  IDirect3DDevice9 *device = gfxWindowsPlatform::GetPlatform()->GetD3D9Device();
  if (!device ||
      FAILED(device->
               CreateTexture(aSize.width, aSize.height,
                             1, 0, format, D3DPOOL_SYSTEMMEM,
                             getter_AddRefs(mTexture), nullptr)))
  {
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
  if (mSurface) {
    return mSurface.get();
  }

  MOZ_ASSERT(mTexture, "Cannot lock surface without a texture to lock");

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
