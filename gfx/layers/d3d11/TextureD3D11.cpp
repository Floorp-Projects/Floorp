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

namespace mozilla {

using namespace gfx;

namespace layers {

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

  return result.forget();
}


CompositingRenderTargetD3D11::CompositingRenderTargetD3D11(ID3D11Texture2D* aTexture)
{
  MOZ_ASSERT(aTexture);
  
  mTextures[0] = aTexture;

  RefPtr<ID3D11Device> device;
  mTextures[0]->GetDevice(byRef(device));

  HRESULT hr = device->CreateRenderTargetView(mTextures[0], nullptr, byRef(mRTView));

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
                                              gfxASurface::gfxContentType aType)
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
  }

  mDescriptor = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle,
                                       aType == gfxASurface::CONTENT_COLOR_ALPHA);

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

  device->OpenSharedResource((HANDLE)aDescriptor.get_SurfaceDescriptorD3D10().handle(),
                             __uuidof(ID3D10Texture2D),
                             (void**)(ID3D10Texture2D**)byRef(mTexture));
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
  case gfxASurface::CONTENT_ALPHA:
    format = FORMAT_A8;
    break;
  case gfxASurface::CONTENT_COLOR:
    format = FORMAT_B8G8R8X8;
    break;
  case gfxASurface::CONTENT_COLOR_ALPHA:
    format = FORMAT_B8G8R8A8;
    break;
  default:
    format = FORMAT_B8G8R8A8;
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

static uint32_t GetRequiredTiles(uint32_t aSize, uint32_t aMaxSize)
{
  uint32_t requiredTiles = aSize / aMaxSize;
  if (aSize % aMaxSize) {
    requiredTiles++;
  }
  return requiredTiles;
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

  gfxIntSize size = surf->GetSize();

  uint32_t bpp = 0;

  DXGI_FORMAT dxgiFormat;
  switch (surf->Format()) {
  case gfxImageSurface::ImageFormatRGB24:
    mFormat = FORMAT_B8G8R8X8;
    dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
    break;
  case gfxImageSurface::ImageFormatARGB32:
    mFormat = FORMAT_B8G8R8A8;
    dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    bpp = 4;
    break;
  case gfxImageSurface::ImageFormatA8:
    mFormat = FORMAT_A8;
    dxgiFormat = DXGI_FORMAT_A8_UNORM;
    bpp = 1;
    break;
  }

  mSize = IntSize(size.width, size.height);

  CD3D11_TEXTURE2D_DESC desc(dxgiFormat, size.width, size.height,
                             1, 1, D3D11_BIND_SHADER_RESOURCE,
                             D3D11_USAGE_IMMUTABLE);

  int32_t maxSize = GetMaxTextureSizeForFeatureLevel(mDevice->GetFeatureLevel());
  if (size.width <= maxSize && size.height <= maxSize) {
    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = surf->Data();
    initData.SysMemPitch = surf->Stride();

    mDevice->CreateTexture2D(&desc, &initData, byRef(mTextures[0]));
    mIsTiled = false;
  } else {
    mIsTiled = true;
    uint32_t tileCount = GetRequiredTiles(size.width, maxSize) *
                         GetRequiredTiles(size.height, maxSize);

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
  uint32_t horizontalTiles = GetRequiredTiles(mSize.width, maxSize);
  uint32_t verticalTiles = GetRequiredTiles(mSize.height, maxSize);

  uint32_t verticalTile = aID / horizontalTiles;
  uint32_t horizontalTile = aID % horizontalTiles;

  return IntRect(horizontalTile * maxSize,
                 verticalTile * maxSize,
                 horizontalTile < (horizontalTiles - 1) ? maxSize : mSize.width % maxSize,
                 verticalTile < (verticalTiles - 1) ? maxSize : mSize.height % maxSize);
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

  mDevice->OpenSharedResource((HANDLE)aImage.get_SurfaceDescriptorD3D10().handle(),
                              __uuidof(ID3D11Texture2D),
                              (void**)(ID3D11Texture2D**)byRef(mTextures[0]));
  mFormat = aImage.get_SurfaceDescriptorD3D10().hasAlpha() ? FORMAT_B8G8R8A8 : FORMAT_B8G8R8X8;

  D3D11_TEXTURE2D_DESC desc;
  mTextures[0]->GetDesc(&desc);

  mSize = IntSize(desc.Width, desc.Height);
}

void
DeprecatedTextureHostDXGID3D11::LockTexture()
{
  RefPtr<IDXGIKeyedMutex> mutex;
  mTextures[0]->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->AcquireSync(0, INFINITE);
}

void
DeprecatedTextureHostDXGID3D11::ReleaseTexture()
{
  RefPtr<IDXGIKeyedMutex> mutex;
  mTextures[0]->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

  mutex->ReleaseSync(0);
}

void
DeprecatedTextureHostYCbCrD3D11::SetCompositor(Compositor* aCompositor)
{
  CompositorD3D11* d3dCompositor = static_cast<CompositorD3D11*>(aCompositor);
  mDevice = d3dCompositor ? d3dCompositor->GetDevice() : nullptr;
}

IntSize
DeprecatedTextureHostYCbCrD3D11::GetSize() const
{
  return TextureSourceD3D11::GetSize();
}

void
DeprecatedTextureHostYCbCrD3D11::UpdateImpl(const SurfaceDescriptor& aImage,
                                  nsIntRegion* aRegion,
                                  nsIntPoint* aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TYCbCrImage);

  YCbCrImageDataDeserializer yuvDeserializer(aImage.get_YCbCrImage().data().get<uint8_t>());

  gfxIntSize gfxCbCrSize = yuvDeserializer.GetCbCrSize();

  gfxIntSize size = yuvDeserializer.GetYSize();

  D3D11_SUBRESOURCE_DATA initData;
  initData.pSysMem = yuvDeserializer.GetYData();
  initData.SysMemPitch = yuvDeserializer.GetYStride();

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8_UNORM, size.width, size.height,
                             1, 1, D3D11_BIND_SHADER_RESOURCE,
                             D3D11_USAGE_IMMUTABLE);

  mDevice->CreateTexture2D(&desc, &initData, byRef(mTextures[0]));

  initData.pSysMem = yuvDeserializer.GetCbData();
  initData.SysMemPitch = yuvDeserializer.GetCbCrStride();
  desc.Width = yuvDeserializer.GetCbCrSize().width;
  desc.Height = yuvDeserializer.GetCbCrSize().height;

  mDevice->CreateTexture2D(&desc, &initData, byRef(mTextures[1]));

  initData.pSysMem = yuvDeserializer.GetCrData();
  mDevice->CreateTexture2D(&desc, &initData, byRef(mTextures[2]));

  mSize = IntSize(size.width, size.height);
}

}
}
