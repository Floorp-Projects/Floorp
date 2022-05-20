/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D9SurfaceImage.h"
#include "gfx2DGlue.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/ProfilerLabels.h"

namespace mozilla {
namespace layers {

DXGID3D9TextureData::DXGID3D9TextureData(gfx::SurfaceFormat aFormat,
                                         IDirect3DTexture9* aTexture,
                                         HANDLE aHandle,
                                         IDirect3DDevice9* aDevice)
    : mDevice(aDevice), mTexture(aTexture), mFormat(aFormat), mHandle(aHandle) {
  MOZ_COUNT_CTOR(DXGID3D9TextureData);
}

DXGID3D9TextureData::~DXGID3D9TextureData() {
  gfxWindowsPlatform::sD3D9SharedTextures -= mDesc.Width * mDesc.Height * 4;
  MOZ_COUNT_DTOR(DXGID3D9TextureData);
}

// static
DXGID3D9TextureData* DXGID3D9TextureData::Create(gfx::IntSize aSize,
                                                 gfx::SurfaceFormat aFormat,
                                                 TextureFlags aFlags,
                                                 IDirect3DDevice9* aDevice) {
  AUTO_PROFILER_LABEL("DXGID3D9TextureData::Create", GRAPHICS);
  MOZ_ASSERT(aFormat == gfx::SurfaceFormat::B8G8R8A8);
  if (aFormat != gfx::SurfaceFormat::B8G8R8A8) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture;
  HANDLE shareHandle = nullptr;
  HRESULT hr = aDevice->CreateTexture(
      aSize.width, aSize.height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
      D3DPOOL_DEFAULT, getter_AddRefs(texture), &shareHandle);
  if (FAILED(hr) || !shareHandle) {
    return nullptr;
  }

  D3DSURFACE_DESC surfaceDesc;
  hr = texture->GetLevelDesc(0, &surfaceDesc);
  if (FAILED(hr)) {
    return nullptr;
  }
  DXGID3D9TextureData* data =
      new DXGID3D9TextureData(aFormat, texture, shareHandle, aDevice);
  data->mDesc = surfaceDesc;

  gfxWindowsPlatform::sD3D9SharedTextures += aSize.width * aSize.height * 4;
  return data;
}

void DXGID3D9TextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = GetSize();
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
  aInfo.hasSynchronization = false;
}

already_AddRefed<IDirect3DSurface9> DXGID3D9TextureData::GetD3D9Surface()
    const {
  RefPtr<IDirect3DSurface9> textureSurface;
  HRESULT hr = mTexture->GetSurfaceLevel(0, getter_AddRefs(textureSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return textureSurface.forget();
}

bool DXGID3D9TextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  SurfaceDescriptorD3D10 desc(
      (WindowsHandle)(mHandle), /* gpuProcessTextureId */ Nothing(),
      /* arrayIndex */ 0, mFormat, GetSize(), gfx::YUVColorSpace::Identity,
      gfx::ColorRange::FULL);
  // In reality, with D3D9 we will only ever deal with RGBA textures.
  bool isYUV = mFormat == gfx::SurfaceFormat::NV12 ||
               mFormat == gfx::SurfaceFormat::P010 ||
               mFormat == gfx::SurfaceFormat::P016;
  if (isYUV) {
    gfxCriticalError() << "Unexpected YUV format for DXGID3D9TextureData: "
                       << mFormat;
    desc.yUVColorSpace() = gfx::YUVColorSpace::BT601;
    desc.colorRange() = gfx::ColorRange::LIMITED;
  }
  aOutDescriptor = desc;
  return true;
}

D3D9SurfaceImage::D3D9SurfaceImage()
    : Image(nullptr, ImageFormat::D3D9_RGB32_TEXTURE),
      mSize(0, 0),
      mShareHandle(0),
      mValid(true) {}

D3D9SurfaceImage::~D3D9SurfaceImage() {}

HRESULT
D3D9SurfaceImage::AllocateAndCopy(D3D9RecycleAllocator* aAllocator,
                                  IDirect3DSurface9* aSurface,
                                  const gfx::IntRect& aRegion) {
  NS_ENSURE_TRUE(aSurface, E_POINTER);
  HRESULT hr;
  RefPtr<IDirect3DSurface9> surface = aSurface;

  RefPtr<IDirect3DDevice9> device;
  hr = surface->GetDevice(getter_AddRefs(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  RefPtr<IDirect3D9> d3d9;
  hr = device->GetDirect3D(getter_AddRefs(d3d9));
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  D3DSURFACE_DESC desc;
  surface->GetDesc(&desc);
  // Ensure we can convert the textures format to RGB conversion
  // in StretchRect. Fail if we can't.
  hr = d3d9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                         desc.Format, D3DFMT_A8R8G8B8);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // DXVA surfaces aren't created sharable, so we need to copy the surface
  // to a sharable texture to that it's accessible to the layer manager's
  // device.
  if (aAllocator) {
    mTextureClient = aAllocator->CreateOrRecycleClient(
        gfx::SurfaceFormat::B8G8R8A8, aRegion.Size());
    if (!mTextureClient) {
      return E_FAIL;
    }

    DXGID3D9TextureData* texData =
        static_cast<DXGID3D9TextureData*>(mTextureClient->GetInternalData());
    mTexture = texData->GetD3D9Texture();
    mShareHandle = texData->GetShareHandle();
    mDesc = texData->GetDesc();
  } else {
    hr = device->CreateTexture(aRegion.Size().width, aRegion.Size().height, 1,
                               D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                               D3DPOOL_DEFAULT, getter_AddRefs(mTexture),
                               &mShareHandle);
    if (FAILED(hr) || !mShareHandle) {
      return E_FAIL;
    }

    hr = mTexture->GetLevelDesc(0, &mDesc);
    if (FAILED(hr)) {
      return E_FAIL;
    }
  }

  // Copy the image onto the texture, preforming YUV -> RGB conversion if
  // necessary.
  RefPtr<IDirect3DSurface9> textureSurface = GetD3D9Surface();
  if (!textureSurface) {
    return E_FAIL;
  }

  RECT src = {aRegion.X(), aRegion.Y(), aRegion.XMost(), aRegion.YMost()};
  hr =
      device->StretchRect(surface, &src, textureSurface, nullptr, D3DTEXF_NONE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mSize = aRegion.Size();
  return S_OK;
}

already_AddRefed<IDirect3DSurface9> D3D9SurfaceImage::GetD3D9Surface() const {
  RefPtr<IDirect3DSurface9> textureSurface;
  HRESULT hr = mTexture->GetSurfaceLevel(0, getter_AddRefs(textureSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  return textureSurface.forget();
}

HANDLE
D3D9SurfaceImage::GetShareHandle() const { return mShareHandle; }

gfx::IntSize D3D9SurfaceImage::GetSize() const { return mSize; }

TextureClient* D3D9SurfaceImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  MOZ_ASSERT(mTextureClient);
  MOZ_ASSERT(mTextureClient->GetAllocator() ==
             aKnowsCompositor->GetTextureForwarder());
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface> D3D9SurfaceImage::GetAsSourceSurface() {
  if (!mTexture) {
    return nullptr;
  }

  HRESULT hr;
  RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateDataSourceSurface(mSize,
                                            gfx::SurfaceFormat::B8G8R8X8);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  // Readback the texture from GPU memory into system memory, so that
  // we can copy it into the Cairo image. This is expensive.
  RefPtr<IDirect3DSurface9> textureSurface = GetD3D9Surface();
  if (!textureSurface) {
    return nullptr;
  }

  RefPtr<IDirect3DDevice9> device;
  hr = textureSurface->GetDevice(getter_AddRefs(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DSurface9> systemMemorySurface;
  hr = device->CreateOffscreenPlainSurface(
      mSize.width, mSize.height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
      getter_AddRefs(systemMemorySurface), 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = device->GetRenderTargetData(textureSurface, systemMemorySurface);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  D3DLOCKED_RECT rect;
  hr = systemMemorySurface->LockRect(&rect, nullptr, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  gfx::DataSourceSurface::MappedSurface mappedSurface;
  if (!surface->Map(gfx::DataSourceSurface::WRITE, &mappedSurface)) {
    systemMemorySurface->UnlockRect();
    return nullptr;
  }

  const unsigned char* src = (const unsigned char*)(rect.pBits);
  const unsigned srcPitch = rect.Pitch;
  for (int y = 0; y < mSize.height; y++) {
    memcpy(mappedSurface.mData + mappedSurface.mStride * y,
           (unsigned char*)(src) + srcPitch * y, mSize.width * 4);
  }

  systemMemorySurface->UnlockRect();
  surface->Unmap();

  return surface.forget();
}

already_AddRefed<TextureClient> D3D9RecycleAllocator::Allocate(
    gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
    TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) {
  TextureData* data =
      DXGID3D9TextureData::Create(aSize, aFormat, aTextureFlags, mDevice);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<TextureClient>(data, aTextureFlags,
                                      mKnowsCompositor->GetTextureForwarder());
}

already_AddRefed<TextureClient> D3D9RecycleAllocator::CreateOrRecycleClient(
    gfx::SurfaceFormat aFormat, const gfx::IntSize& aSize) {
  return CreateOrRecycle(aFormat, aSize, BackendSelector::Content,
                         TextureFlags::DEFAULT);
}

}  // namespace layers
}  // namespace mozilla
