/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11YCbCrImage.h"

#include "YCbCrUtils.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureD3D11.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

D3D11YCbCrImage::D3D11YCbCrImage()
    : Image(NULL, ImageFormat::D3D11_YCBCR_IMAGE) {}

D3D11YCbCrImage::~D3D11YCbCrImage() {}

bool D3D11YCbCrImage::SetData(KnowsCompositor* aAllocator,
                              ImageContainer* aContainer,
                              const PlanarYCbCrData& aData) {
  mPictureRect = aData.mPictureRect;
  mColorDepth = aData.mColorDepth;
  mColorSpace = aData.mYUVColorSpace;
  mColorRange = aData.mColorRange;
  mChromaSubsampling = aData.mChromaSubsampling;

  RefPtr<D3D11YCbCrRecycleAllocator> allocator =
      aContainer->GetD3D11YCbCrRecycleAllocator(aAllocator);
  if (!allocator) {
    return false;
  }

  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
  if (!device) {
    return false;
  }

  {
    DXGIYCbCrTextureAllocationHelper helper(aData, TextureFlags::DEFAULT,
                                            device);
    mTextureClient = allocator->CreateOrRecycle(helper);
  }

  if (!mTextureClient) {
    return false;
  }

  DXGIYCbCrTextureData* data =
      mTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();

  ID3D11Texture2D* textureY = data->GetD3D11Texture(0);
  ID3D11Texture2D* textureCb = data->GetD3D11Texture(1);
  ID3D11Texture2D* textureCr = data->GetD3D11Texture(2);

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = device->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

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
  device->GetImmediateContext(getter_AddRefs(ctx));
  if (!ctx) {
    gfxCriticalError() << "Failed to get immediate context.";
    return false;
  }

  AutoLockD3D11Texture lockY(textureY);
  AutoLockD3D11Texture lockCb(textureCb);
  AutoLockD3D11Texture lockCr(textureCr);

  ctx->UpdateSubresource(textureY, 0, nullptr, aData.mYChannel, aData.mYStride,
                         aData.mYStride * aData.YDataSize().height);
  ctx->UpdateSubresource(textureCb, 0, nullptr, aData.mCbChannel,
                         aData.mCbCrStride,
                         aData.mCbCrStride * aData.CbCrDataSize().height);
  ctx->UpdateSubresource(textureCr, 0, nullptr, aData.mCrChannel,
                         aData.mCbCrStride,
                         aData.mCbCrStride * aData.CbCrDataSize().height);

  return true;
}

IntSize D3D11YCbCrImage::GetSize() const { return mPictureRect.Size(); }

TextureClient* D3D11YCbCrImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient;
}

const DXGIYCbCrTextureData* D3D11YCbCrImage::GetData() const {
  if (!mTextureClient) return nullptr;

  return mTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();
}

nsresult D3D11YCbCrImage::ReadIntoBuffer(
    const std::function<nsresult(const PlanarYCbCrData&, const IntSize&,
                                 SurfaceFormat)>& aCopy) {
  if (!mTextureClient) {
    gfxWarning()
        << "GetAsSourceSurface() called on uninitialized D3D11YCbCrImage.";
    return NS_ERROR_FAILURE;
  }

  gfx::IntSize size(mPictureRect.Size());
  gfx::SurfaceFormat format =
      gfx::ImageFormatToSurfaceFormat(gfxVars::OffscreenFormat());
  HRESULT hr;

  PlanarYCbCrData data;

  DXGIYCbCrTextureData* dxgiData =
      mTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();

  if (!dxgiData) {
    gfxCriticalError() << "Failed to get texture client internal data.";
    return NS_ERROR_FAILURE;
  }

  RefPtr<ID3D11Texture2D> texY = dxgiData->GetD3D11Texture(0);
  RefPtr<ID3D11Texture2D> texCb = dxgiData->GetD3D11Texture(1);
  RefPtr<ID3D11Texture2D> texCr = dxgiData->GetD3D11Texture(2);
  RefPtr<ID3D11Texture2D> softTexY, softTexCb, softTexCr;
  D3D11_TEXTURE2D_DESC desc;

  RefPtr<ID3D11Device> dev;
  texY->GetDevice(getter_AddRefs(dev));

  if (!dev || dev != gfx::DeviceManagerDx::Get()->GetImageDevice()) {
    gfxCriticalError() << "D3D11Device is obsoleted";
    return NS_ERROR_FAILURE;
  }

  RefPtr<ID3D10Multithread> mt;
  hr = dev->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported.";
    return NS_ERROR_FAILURE;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return NS_ERROR_FAILURE;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  texY->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;

  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexY));
  if (!softTexY) {
    gfxCriticalNote << "Failed to allocate softTexY";
    return NS_ERROR_FAILURE;
  }

  texCb->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;

  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexCb));
  if (!softTexCb) {
    gfxCriticalNote << "Failed to allocate softTexCb";
    return NS_ERROR_FAILURE;
  }

  texCr->GetDesc(&desc);
  desc.BindFlags = 0;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;

  dev->CreateTexture2D(&desc, nullptr, getter_AddRefs(softTexCr));
  if (!softTexCr) {
    gfxCriticalNote << "Failed to allocate softTexCr";
    return NS_ERROR_FAILURE;
  }

  RefPtr<ID3D11DeviceContext> ctx;
  dev->GetImmediateContext(getter_AddRefs(ctx));
  if (!ctx) {
    gfxCriticalError() << "Failed to get immediate context.";
    return NS_ERROR_FAILURE;
  }

  {
    AutoLockD3D11Texture lockY(texY);
    AutoLockD3D11Texture lockCb(texCb);
    AutoLockD3D11Texture lockCr(texCr);
    ctx->CopyResource(softTexY, texY);
    ctx->CopyResource(softTexCb, texCb);
    ctx->CopyResource(softTexCr, texCr);
  }

  D3D11_MAPPED_SUBRESOURCE mapY, mapCb, mapCr;
  mapY.pData = mapCb.pData = mapCr.pData = nullptr;

  hr = ctx->Map(softTexY, 0, D3D11_MAP_READ, 0, &mapY);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return NS_ERROR_FAILURE;
  }
  hr = ctx->Map(softTexCb, 0, D3D11_MAP_READ, 0, &mapCb);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return NS_ERROR_FAILURE;
  }
  hr = ctx->Map(softTexCr, 0, D3D11_MAP_READ, 0, &mapCr);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to map Y plane (" << hr << ")";
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mapCb.RowPitch == mapCr.RowPitch);

  data.mPictureRect = mPictureRect;
  data.mStereoMode = StereoMode::MONO;
  data.mColorDepth = mColorDepth;
  data.mYUVColorSpace = mColorSpace;
  data.mColorRange = mColorRange;
  data.mChromaSubsampling = mChromaSubsampling;
  data.mYSkip = data.mCbSkip = data.mCrSkip = 0;
  data.mYChannel = static_cast<uint8_t*>(mapY.pData);
  data.mYStride = mapY.RowPitch;
  data.mCbChannel = static_cast<uint8_t*>(mapCb.pData);
  data.mCrChannel = static_cast<uint8_t*>(mapCr.pData);
  data.mCbCrStride = mapCb.RowPitch;

  gfx::GetYCbCrToRGBDestFormatAndSize(data, format, size);
  if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
      size.height > PlanarYCbCrImage::MAX_DIMENSION) {
    gfxCriticalError() << "Illegal image dest width or height";
    return NS_ERROR_FAILURE;
  }

  nsresult rv = aCopy(data, size, format);

  ctx->Unmap(softTexY, 0);
  ctx->Unmap(softTexCb, 0);
  ctx->Unmap(softTexCr, 0);

  return rv;
}

already_AddRefed<SourceSurface> D3D11YCbCrImage::GetAsSourceSurface() {
  RefPtr<gfx::DataSourceSurface> surface;

  nsresult rv =
      ReadIntoBuffer([&](const PlanarYCbCrData& aData, const IntSize& aSize,
                         SurfaceFormat aFormat) -> nsresult {
        surface = gfx::Factory::CreateDataSourceSurface(aSize, aFormat);
        if (!surface) {
          gfxCriticalError()
              << "Failed to create DataSourceSurface for image: " << aSize
              << " " << aFormat;
          return NS_ERROR_OUT_OF_MEMORY;
        }

        DataSourceSurface::ScopedMap mapping(surface, DataSourceSurface::WRITE);
        if (!mapping.IsMapped()) {
          gfxCriticalError()
              << "Failed to map DataSourceSurface for D3D11YCbCrImage";
          return NS_ERROR_FAILURE;
        }

        gfx::ConvertYCbCrToRGB(aData, aFormat, aSize, mapping.GetData(),
                               mapping.GetStride());
        return NS_OK;
      });

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  MOZ_ASSERT(surface);
  return surface.forget();
}

nsresult D3D11YCbCrImage::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  return ReadIntoBuffer([&](const PlanarYCbCrData& aData, const IntSize& aSize,
                            SurfaceFormat aFormat) -> nsresult {
    uint8_t* buffer = nullptr;
    int32_t stride = 0;
    nsresult rv = AllocateSurfaceDescriptorBufferRgb(
        aSize, aFormat, buffer, aSdBuffer, stride, aAllocate);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gfx::ConvertYCbCrToRGB(aData, aFormat, aSize, buffer, stride);
    return NS_OK;
  });
}

class AutoCheckLockD3D11Texture final {
 public:
  explicit AutoCheckLockD3D11Texture(ID3D11Texture2D* aTexture)
      : mIsLocked(false) {
    aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
    if (!mMutex) {
      // If D3D11Texture does not have keyed mutex, we think that the
      // D3D11Texture could be locked.
      mIsLocked = true;
      return;
    }

    // Test to see if the keyed mutex has been released
    HRESULT hr = mMutex->AcquireSync(0, 0);
    if (hr == S_OK || hr == WAIT_ABANDONED) {
      mIsLocked = true;
      // According to Microsoft documentation:
      // WAIT_ABANDONED - The shared surface and keyed mutex are no longer in a
      // consistent state. If AcquireSync returns this value, you should release
      // and recreate both the keyed mutex and the shared surface
      // So even if we do get WAIT_ABANDONED, the keyed mutex will have to be
      // released.
      mSyncAcquired = true;
    }
  }

  ~AutoCheckLockD3D11Texture() {
    if (!mSyncAcquired) {
      return;
    }
    HRESULT hr = mMutex->ReleaseSync(0);
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }

  bool IsLocked() const { return mIsLocked; }

 private:
  bool mIsLocked;
  bool mSyncAcquired = false;
  RefPtr<IDXGIKeyedMutex> mMutex;
};

DXGIYCbCrTextureAllocationHelper::DXGIYCbCrTextureAllocationHelper(
    const PlanarYCbCrData& aData, TextureFlags aTextureFlags,
    ID3D11Device* aDevice)
    : ITextureClientAllocationHelper(
          gfx::SurfaceFormat::YUV, aData.mPictureRect.Size(),
          BackendSelector::Content, aTextureFlags, ALLOC_DEFAULT),
      mData(aData),
      mDevice(aDevice) {}

bool DXGIYCbCrTextureAllocationHelper::IsCompatible(
    TextureClient* aTextureClient) {
  MOZ_ASSERT(aTextureClient->GetFormat() == gfx::SurfaceFormat::YUV);

  DXGIYCbCrTextureData* dxgiData =
      aTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();
  if (!dxgiData || aTextureClient->GetSize() != mData.mPictureRect.Size() ||
      dxgiData->GetYSize() != mData.YDataSize() ||
      dxgiData->GetCbCrSize() != mData.CbCrDataSize() ||
      dxgiData->GetColorDepth() != mData.mColorDepth ||
      dxgiData->GetYUVColorSpace() != mData.mYUVColorSpace) {
    return false;
  }

  ID3D11Texture2D* textureY = dxgiData->GetD3D11Texture(0);
  ID3D11Texture2D* textureCb = dxgiData->GetD3D11Texture(1);
  ID3D11Texture2D* textureCr = dxgiData->GetD3D11Texture(2);

  RefPtr<ID3D11Device> device;
  textureY->GetDevice(getter_AddRefs(device));
  if (!device || device != gfx::DeviceManagerDx::Get()->GetImageDevice()) {
    return false;
  }

  // Test to see if the keyed mutex has been released.
  // If D3D11Texture failed to lock, do not recycle the DXGIYCbCrTextureData.

  AutoCheckLockD3D11Texture lockY(textureY);
  AutoCheckLockD3D11Texture lockCr(textureCr);
  AutoCheckLockD3D11Texture lockCb(textureCb);

  if (!lockY.IsLocked() || !lockCr.IsLocked() || !lockCb.IsLocked()) {
    return false;
  }

  return true;
}

already_AddRefed<TextureClient> DXGIYCbCrTextureAllocationHelper::Allocate(
    KnowsCompositor* aAllocator) {
  auto ySize = mData.YDataSize();
  auto cbcrSize = mData.CbCrDataSize();
  CD3D11_TEXTURE2D_DESC newDesc(mData.mColorDepth == gfx::ColorDepth::COLOR_8
                                    ? DXGI_FORMAT_R8_UNORM
                                    : DXGI_FORMAT_R16_UNORM,
                                ySize.width, ySize.height, 1, 1);
  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = mDevice->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported. " << hr;
    return nullptr;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return nullptr;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  RefPtr<ID3D11Texture2D> textureY;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = cbcrSize.width;
  newDesc.Height = cbcrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<ID3D11Texture2D> textureCr;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  TextureForwarder* forwarder =
      aAllocator ? aAllocator->GetTextureForwarder() : nullptr;

  return TextureClient::CreateWithData(
      DXGIYCbCrTextureData::Create(
          textureY, textureCb, textureCr, mData.mPictureRect.Size(), ySize,
          cbcrSize, mData.mColorDepth, mData.mYUVColorSpace, mData.mColorRange),
      mTextureFlags, forwarder);
}

already_AddRefed<TextureClient> D3D11YCbCrRecycleAllocator::Allocate(
    SurfaceFormat aFormat, IntSize aSize, BackendSelector aSelector,
    TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return nullptr;
}

}  // namespace layers
}  // namespace mozilla
