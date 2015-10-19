/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D9SurfaceImage.h"
#include "gfx2DGlue.h"
#include "mozilla/layers/TextureD3D9.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {


D3D9SurfaceImage::D3D9SurfaceImage(bool aIsFirstFrame)
  : Image(nullptr, ImageFormat::D3D9_RGB32_TEXTURE)
  , mSize(0, 0)
  , mValid(false)
  , mIsFirstFrame(aIsFirstFrame)
{}

D3D9SurfaceImage::~D3D9SurfaceImage()
{
}

HRESULT
D3D9SurfaceImage::AllocateAndCopy(D3D9RecycleAllocator* aAllocator,
                                  IDirect3DSurface9* aSurface,
                                  const gfx::IntRect& aRegion)
{
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
  hr = d3d9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                         D3DDEVTYPE_HAL,
                                         desc.Format,
                                         D3DFMT_X8R8G8B8);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // DXVA surfaces aren't created sharable, so we need to copy the surface
  // to a sharable texture to that it's accessible to the layer manager's
  // device.
  RefPtr<TextureClient> textureClient =
    aAllocator->CreateOrRecycleClient(gfx::SurfaceFormat::B8G8R8X8, aRegion.Size());
  if (!textureClient) {
    return E_FAIL;
  }

  // Copy the image onto the texture, preforming YUV -> RGB conversion if necessary.
  RefPtr<IDirect3DSurface9> textureSurface = static_cast<DXGID3D9TextureData*>(
    textureClient->GetInternalData())->GetD3D9Surface();
  if (!textureSurface) {
    return E_FAIL;
  }

  RECT src = { aRegion.x, aRegion.y, aRegion.x+aRegion.width, aRegion.y+aRegion.height };
  hr = device->StretchRect(surface, &src, textureSurface, nullptr, D3DTEXF_NONE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Flush the draw command now, so that by the time we come to draw this
  // image, we're less likely to need to wait for the draw operation to
  // complete.
  RefPtr<IDirect3DQuery9> query;
  hr = device->CreateQuery(D3DQUERYTYPE_EVENT, getter_AddRefs(query));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  hr = query->Issue(D3DISSUE_END);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mTextureClient = textureClient;
  mSize = aRegion.Size();
  mQuery = query;
  return S_OK;
}

bool
D3D9SurfaceImage::IsValid()
{
  EnsureSynchronized();
  return mValid;
}

void
D3D9SurfaceImage::EnsureSynchronized()
{
  RefPtr<IDirect3DQuery9> query = mQuery;
  if (!query) {
    // Not setup, or already synchronized.
    return;
  }
  int iterations = 0;
  while (iterations < (mIsFirstFrame ? 100 : 10)) {
    HRESULT hr = query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
      Sleep(1);
      iterations++;
      continue;
    }
    if (hr == S_OK) {
      mValid = true;
    }
    break;
  }
  mQuery = nullptr;
}

const D3DSURFACE_DESC&
D3D9SurfaceImage::GetDesc() const
{
  return static_cast<DXGID3D9TextureData*>(mTextureClient->GetInternalData())->GetDesc();
}

gfx::IntSize
D3D9SurfaceImage::GetSize()
{
  return mSize;
}

TextureClient*
D3D9SurfaceImage::GetTextureClient(CompositableClient* aClient)
{
  MOZ_ASSERT(mTextureClient);
  MOZ_ASSERT(mTextureClient->GetAllocator() == aClient->GetForwarder());
  EnsureSynchronized();
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D9SurfaceImage::GetAsSourceSurface()
{
  if (!mTextureClient) {
    return nullptr;
  }

  HRESULT hr;
  RefPtr<gfx::DataSourceSurface> surface = gfx::Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8X8);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  // Ensure that the texture is ready to be used.
  EnsureSynchronized();

  DXGID3D9TextureData* texData = static_cast<DXGID3D9TextureData*>(mTextureClient->GetInternalData());
  // Readback the texture from GPU memory into system memory, so that
  // we can copy it into the Cairo image. This is expensive.
  RefPtr<IDirect3DSurface9> textureSurface = texData->GetD3D9Surface();
  if (!textureSurface) {
    return nullptr;
  }

  RefPtr<IDirect3DDevice9> device = texData->GetD3D9Device();
  if (!device) {
    return nullptr;
  }

  RefPtr<IDirect3DSurface9> systemMemorySurface;
  hr = device->CreateOffscreenPlainSurface(mSize.width,
                                           mSize.height,
                                           D3DFMT_X8R8G8B8,
                                           D3DPOOL_SYSTEMMEM,
                                           getter_AddRefs(systemMemorySurface),
                                           0);
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
           (unsigned char*)(src) + srcPitch * y,
           mSize.width * 4);
  }

  systemMemorySurface->UnlockRect();
  surface->Unmap();

  return surface.forget();
}

already_AddRefed<TextureClient>
D3D9RecycleAllocator::Allocate(gfx::SurfaceFormat aFormat,
                               gfx::IntSize aSize,
                               BackendSelector aSelector,
                               TextureFlags aTextureFlags,
                               TextureAllocationFlags aAllocFlags)
{
  TextureData* data = DXGID3D9TextureData::Create(aSize, aFormat, aTextureFlags, mDevice);
  if (!data) {
    return nullptr;
  }

  return MakeAndAddRef<ClientTexture>(data, aTextureFlags, mSurfaceAllocator);
}

already_AddRefed<TextureClient>
D3D9RecycleAllocator::CreateOrRecycleClient(gfx::SurfaceFormat aFormat,
                                            const gfx::IntSize& aSize)
{
  return CreateOrRecycle(aFormat, aSize, BackendSelector::Content,
                         TextureFlags::DEFAULT);
}

} // namespace layers
} // namespace mozilla
