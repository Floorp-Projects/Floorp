/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11YCbCrImage.h"
#include "YCbCrUtils.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureD3D11.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

D3D11YCbCrImage::D3D11YCbCrImage()
 : Image(NULL, ImageFormat::D3D11_YCBCR_IMAGE)
{
}

D3D11YCbCrImage::~D3D11YCbCrImage() { }

bool
D3D11YCbCrImage::SetData(KnowsCompositor* aAllocator,
                         ImageContainer* aContainer,
                         const PlanarYCbCrData& aData)
{
  mPictureRect = IntRect(
    aData.mPicX, aData.mPicY, aData.mPicSize.width, aData.mPicSize.height);
  mYSize = aData.mYSize;
  mCbCrSize = aData.mCbCrSize;
  mColorSpace = aData.mYUVColorSpace;

  D3D11YCbCrRecycleAllocator* allocator =
    aContainer->GetD3D11YCbCrRecycleAllocator(aAllocator);
  if (!allocator) {
    return false;
  }
  allocator->SetSizes(aData.mYSize, aData.mCbCrSize);

  mTextureClient = allocator->CreateOrRecycle(SurfaceFormat::A8,
                                              mYSize,
                                              BackendSelector::Content,
                                              TextureFlags::DEFAULT);
  if (!mTextureClient) {
    return false;
  }

  DXGIYCbCrTextureData *data =
    static_cast<DXGIYCbCrTextureData*>(mTextureClient->GetInternalData());

  ID3D11Texture2D* textureY = data->GetD3D11Texture(0);
  ID3D11Texture2D* textureCb = data->GetD3D11Texture(1);
  ID3D11Texture2D* textureCr = data->GetD3D11Texture(2);

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = allocator->GetDevice()->QueryInterface(
    (ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported. " << hr;
    return false;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return false;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  RefPtr<ID3D11DeviceContext> ctx;
  allocator->GetDevice()->GetImmediateContext(getter_AddRefs(ctx));

  AutoLockD3D11Texture lockY(textureY);
  AutoLockD3D11Texture lockCb(textureCb);
  AutoLockD3D11Texture lockCr(textureCr);

  ctx->UpdateSubresource(textureY,
                         0,
                         nullptr,
                         aData.mYChannel,
                         aData.mYStride,
                         aData.mYStride * aData.mYSize.height);
  ctx->UpdateSubresource(textureCb,
                         0,
                         nullptr,
                         aData.mCbChannel,
                         aData.mCbCrStride,
                         aData.mCbCrStride * aData.mCbCrSize.height);
  ctx->UpdateSubresource(textureCr,
                         0,
                         nullptr,
                         aData.mCrChannel,
                         aData.mCbCrStride,
                         aData.mCbCrStride * aData.mCbCrSize.height);

  
  return true;
}

IntSize
D3D11YCbCrImage::GetSize()
{
  return mPictureRect.Size();
}

TextureClient*
D3D11YCbCrImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  return mTextureClient;
}

already_AddRefed<SourceSurface>
D3D11YCbCrImage::GetAsSourceSurface()
{
  if (!mTextureClient) {
    gfxWarning()
      << "GetAsSourceSurface() called on uninitialized D3D11YCbCrImage.";
    return nullptr;
  }

  gfx::IntSize size(mPictureRect.Size());
  gfx::SurfaceFormat format =
    gfx::ImageFormatToSurfaceFormat(gfxVars::OffscreenFormat());
  HRESULT hr;

  PlanarYCbCrData data;

  DXGIYCbCrTextureData *dxgiData =
    static_cast<DXGIYCbCrTextureData*>(mTextureClient->GetInternalData());

  if (!dxgiData) {
    gfxCriticalError() << "Failed to get texture client internal data.";
    return nullptr;
  }

  RefPtr<ID3D11Texture2D> texY = dxgiData->GetD3D11Texture(0);
  RefPtr<ID3D11Texture2D> texCb = dxgiData->GetD3D11Texture(1);
  RefPtr<ID3D11Texture2D> texCr = dxgiData->GetD3D11Texture(2);
  RefPtr<ID3D11Texture2D> softTexY, softTexCb, softTexCr;
  D3D11_TEXTURE2D_DESC desc;

  RefPtr<ID3D11Device> dev;
  texY->GetDevice(getter_AddRefs(dev));

  RefPtr<ID3D10Multithread> mt;
  hr = dev->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported.";
    return nullptr;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return nullptr;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  texY->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;

  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexY));

  texCb->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;

  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexCb));
  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexCr));

  RefPtr<ID3D11DeviceContext> ctx;
  dev->GetImmediateContext(getter_AddRefs(ctx));

  {
    AutoLockD3D11Texture lockY(texY);
    AutoLockD3D11Texture lockCb(texCb);
    AutoLockD3D11Texture lockCr(texCr);
    ctx->CopyResource(softTexY, texY);
    ctx->CopyResource(softTexCb, texCb);
    ctx->CopyResource(softTexCr, texCr);
  }

  D3D11_MAPPED_SUBRESOURCE mapY, mapCb, mapCr;
  RefPtr<gfx::DataSourceSurface> surface;
  mapY.pData = mapCb.pData = mapCr.pData = nullptr;

  hr = ctx->Map(softTexY, 0, D3D11_MAP_READ, 0, &mapY);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return nullptr;
  }
  hr = ctx->Map(softTexCb, 0, D3D11_MAP_READ, 0, &mapCb);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return nullptr;
  }
  hr = ctx->Map(softTexCr, 0, D3D11_MAP_READ, 0, &mapCr);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return nullptr;
  }

  MOZ_ASSERT(mapCb.RowPitch == mapCr.RowPitch);

  data.mPicX = mPictureRect.x;
  data.mPicY = mPictureRect.y;
  data.mPicSize = mPictureRect.Size();
  data.mStereoMode = StereoMode::MONO;
  data.mYUVColorSpace = mColorSpace;
  data.mYSkip = data.mCbSkip = data.mCrSkip = 0;
  data.mYSize = mYSize;
  data.mCbCrSize = mCbCrSize;
  data.mYChannel = static_cast<uint8_t*>(mapY.pData);
  data.mYStride = mapY.RowPitch;
  data.mCbChannel = static_cast<uint8_t*>(mapCb.pData);
  data.mCrChannel = static_cast<uint8_t*>(mapCr.pData);
  data.mCbCrStride = mapCb.RowPitch;

  gfx::GetYCbCrToRGBDestFormatAndSize(data, format, size);
  if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
      size.height > PlanarYCbCrImage::MAX_DIMENSION) {
    gfxCriticalError() << "Illegal image dest width or height";
    return nullptr;
  }

  surface = gfx::Factory::CreateDataSourceSurface(size, format);
  if (!surface) {
    gfxCriticalError() << "Failed to create DataSourceSurface for image: "
                       << size << " " << format;
    return nullptr;
  }

  DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
  if (!mapping.IsMapped()) {
    gfxCriticalError() << "Failed to map DataSourceSurface for D3D11YCbCrImage";
    return nullptr;
  }

  gfx::ConvertYCbCrToRGB(
    data, format, size, mapping.GetData(), mapping.GetStride());

  ctx->Unmap(softTexY, 0);
  ctx->Unmap(softTexCb, 0);
  ctx->Unmap(softTexCr, 0);

  return surface.forget();
}

void
D3D11YCbCrRecycleAllocator::SetSizes(const gfx::IntSize& aYSize,
                                     const gfx::IntSize& aCbCrSize)
{
  mYSize = Some(aYSize);
  mCbCrSize = Some(aCbCrSize);
}

already_AddRefed<TextureClient>
D3D11YCbCrRecycleAllocator::Allocate(SurfaceFormat aFormat,
                                     IntSize aSize,
                                     BackendSelector aSelector,
                                     TextureFlags aTextureFlags,
                                     TextureAllocationFlags aAllocFlags)
{
  MOZ_ASSERT(aFormat == SurfaceFormat::A8);

  gfx::IntSize YSize = mYSize.refOr(aSize);
  gfx::IntSize CbCrSize =
    mCbCrSize.refOr(gfx::IntSize(YSize.width, YSize.height));
  CD3D11_TEXTURE2D_DESC newDesc(DXGI_FORMAT_R8_UNORM, YSize.width, YSize.height,
                                1, 1);
  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> textureY;
  HRESULT hr =
    mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = CbCrSize.width;
  newDesc.Height = CbCrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<ID3D11Texture2D> textureCr;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  return TextureClient::CreateWithData(
    DXGIYCbCrTextureData::Create(
      textureY,
      textureCb,
      textureCr,
      aSize,
      YSize,
      CbCrSize),
    TextureFlags::DEFAULT,
    mSurfaceAllocator->GetTextureForwarder());
}

} // namespace layers
} // namespace mozilla
