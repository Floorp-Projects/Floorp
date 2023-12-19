/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11ShareHandleImage.h"
#include <memory>
#include "DXVA2Manager.h"
#include "WMF.h"
#include "d3d11.h"
#include "gfxImageSurface.h"
#include "gfxWindowsPlatform.h"
#include "libyuv.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureD3D11.h"

namespace mozilla {
namespace layers {

using namespace gfx;

/* static */
RefPtr<D3D11ShareHandleImage>
D3D11ShareHandleImage::MaybeCreateNV12ImageAndSetData(
    KnowsCompositor* aKnowsCompositor, ImageContainer* aContainer,
    const PlanarYCbCrData& aData) {
  MOZ_ASSERT(aKnowsCompositor);
  MOZ_ASSERT(aContainer);

  if (!aKnowsCompositor || !aContainer) {
    return nullptr;
  }

  // Check if data could be used with NV12
  if (aData.YPictureSize().width % 2 != 0 ||
      aData.YPictureSize().height % 2 != 0 || aData.mYSkip != 0 ||
      aData.mCbSkip != 0 || aData.mCrSkip != 0 ||
      aData.mColorDepth != gfx::ColorDepth::COLOR_8 ||
      aData.mColorRange != gfx::ColorRange::LIMITED ||
      aData.mChromaSubsampling !=
          gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT) {
    return nullptr;
  }

  RefPtr<D3D11ShareHandleImage> image = new D3D11ShareHandleImage(
      aData.YPictureSize(), aData.mPictureRect,
      ToColorSpace2(aData.mYUVColorSpace), aData.mColorRange);

  RefPtr<D3D11RecycleAllocator> allocator =
      aContainer->GetD3D11RecycleAllocator(aKnowsCompositor,
                                           gfx::SurfaceFormat::NV12);
  if (!allocator) {
    return nullptr;
  }

  auto syncObject = allocator->GetSyncObject();
  if (!syncObject) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  MOZ_ASSERT(allocator->GetUsableSurfaceFormat() == gfx::SurfaceFormat::NV12);
  if (allocator->GetUsableSurfaceFormat() != gfx::SurfaceFormat::NV12) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  RefPtr<ID3D11Texture2D> stagingTexture =
      allocator->GetStagingTextureNV12(aData.YPictureSize());
  if (!stagingTexture) {
    return nullptr;
  }

  bool ok = image->AllocateTexture(allocator, allocator->mDevice);
  if (!ok) {
    return nullptr;
  }

  RefPtr<TextureClient> client = image->GetTextureClient(nullptr);
  if (!client) {
    return nullptr;
  }

  client->AddFlags(TextureFlags::SOFTWARE_DECODED_VIDEO);

  // The texture does not have keyed mutex. When keyed mutex exists, the texture
  // could not be used for video overlay. Then it needs manual synchronization
  RefPtr<ID3D11Texture2D> texture = image->GetTexture();
  if (!texture) {
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  allocator->mDevice->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    return nullptr;
  }

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = allocator->mDevice->QueryInterface(
      (ID3D10Multithread**)getter_AddRefs(mt));
  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported. " << hr;
    return nullptr;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return nullptr;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  AutoLockD3D11Texture lockSt(stagingTexture);
  if (NS_WARN_IF(!lockSt.Succeeded())) {
    gfxCriticalNoteOnce << "Locking D3D11 staging texture failed";
    return nullptr;
  }

  D3D11_MAP mapType = D3D11_MAP_WRITE;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  hr = context->Map(stagingTexture, 0, mapType, 0, &mappedResource);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Mapping D3D11 staging texture failed: "
                        << gfx::hexa(hr);
    return nullptr;
  }

  const size_t destStride = mappedResource.RowPitch;
  uint8_t* yDestPlaneStart = reinterpret_cast<uint8_t*>(mappedResource.pData);
  uint8_t* uvDestPlaneStart = reinterpret_cast<uint8_t*>(mappedResource.pData) +
                              destStride * aData.YPictureSize().height;
  // Convert I420 to NV12,
  libyuv::I420ToNV12(aData.mYChannel, aData.mYStride, aData.mCbChannel,
                     aData.mCbCrStride, aData.mCrChannel, aData.mCbCrStride,
                     yDestPlaneStart, destStride, uvDestPlaneStart, destStride,
                     aData.YDataSize().width, aData.YDataSize().height);

  context->Unmap(stagingTexture, 0);

  context->CopyResource(texture, stagingTexture);

  context->Flush();

  client->SyncWithObject(syncObject);
  if (!syncObject->Synchronize(true)) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  return image;
}

D3D11ShareHandleImage::D3D11ShareHandleImage(const gfx::IntSize& aSize,
                                             const gfx::IntRect& aRect,
                                             gfx::ColorSpace2 aColorSpace,
                                             gfx::ColorRange aColorRange)
    : Image(nullptr, ImageFormat::D3D11_SHARE_HANDLE_TEXTURE),
      mSize(aSize),
      mPictureRect(aRect),
      mColorSpace(aColorSpace),
      mColorRange(aColorRange) {}

bool D3D11ShareHandleImage::AllocateTexture(D3D11RecycleAllocator* aAllocator,
                                            ID3D11Device* aDevice) {
  if (aAllocator) {
    mTextureClient =
        aAllocator->CreateOrRecycleClient(mColorSpace, mColorRange, mSize);
    if (mTextureClient) {
      D3D11TextureData* textureData = GetData();
      MOZ_DIAGNOSTIC_ASSERT(textureData, "Wrong TextureDataType");
      mTexture = textureData->GetD3D11Texture();
      return true;
    }
    return false;
  } else {
    MOZ_ASSERT(aDevice);
    CD3D11_TEXTURE2D_DESC newDesc(
        DXGI_FORMAT_B8G8R8A8_UNORM, mSize.width, mSize.height, 1, 1,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    newDesc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr =
        aDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(mTexture));
    return SUCCEEDED(hr);
  }
}

gfx::IntSize D3D11ShareHandleImage::GetSize() const { return mSize; }

TextureClient* D3D11ShareHandleImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D11ShareHandleImage::GetAsSourceSurface() {
  RefPtr<ID3D11Texture2D> src = GetTexture();
  if (!src) {
    gfxWarning() << "Cannot readback from shared texture because no texture is "
                    "available.";
    return nullptr;
  }

  return gfx::Factory::CreateBGRA8DataSourceSurfaceForD3D11Texture(src);
}

nsresult D3D11ShareHandleImage::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  RefPtr<ID3D11Texture2D> src = GetTexture();
  if (!src) {
    gfxWarning() << "Cannot readback from shared texture because no texture is "
                    "available.";
    return NS_ERROR_FAILURE;
  }

  return gfx::Factory::CreateSdbForD3D11Texture(src, mSize, aSdBuffer,
                                                aAllocate);
}

ID3D11Texture2D* D3D11ShareHandleImage::GetTexture() const { return mTexture; }

class MOZ_RAII D3D11TextureClientAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  D3D11TextureClientAllocationHelper(gfx::SurfaceFormat aFormat,
                                     gfx::ColorSpace2 aColorSpace,
                                     gfx::ColorRange aColorRange,
                                     const gfx::IntSize& aSize,
                                     TextureAllocationFlags aAllocFlags,
                                     ID3D11Device* aDevice,
                                     TextureFlags aTextureFlags)
      : ITextureClientAllocationHelper(aFormat, aSize, BackendSelector::Content,
                                       aTextureFlags, aAllocFlags),
        mColorSpace(aColorSpace),
        mColorRange(aColorRange),
        mDevice(aDevice) {}

  bool IsCompatible(TextureClient* aTextureClient) override {
    D3D11TextureData* textureData =
        aTextureClient->GetInternalData()->AsD3D11TextureData();
    if (!textureData || aTextureClient->GetFormat() != mFormat ||
        aTextureClient->GetSize() != mSize) {
      return false;
    }
    // TODO: Should we also check for change in the allocation flags if RGBA?
    return (aTextureClient->GetFormat() != gfx::SurfaceFormat::NV12 &&
            aTextureClient->GetFormat() != gfx::SurfaceFormat::P010 &&
            aTextureClient->GetFormat() != gfx::SurfaceFormat::P016) ||
           (textureData->mColorSpace == mColorSpace &&
            textureData->GetColorRange() == mColorRange &&
            textureData->GetTextureAllocationFlags() == mAllocationFlags);
  }

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override {
    D3D11TextureData* data =
        D3D11TextureData::Create(mSize, mFormat, mAllocationFlags, mDevice);
    if (!data) {
      return nullptr;
    }
    data->mColorSpace = mColorSpace;
    data->SetColorRange(mColorRange);
    return MakeAndAddRef<TextureClient>(data, mTextureFlags,
                                        aAllocator->GetTextureForwarder());
  }

 private:
  const gfx::ColorSpace2 mColorSpace;
  const gfx::ColorRange mColorRange;
  const RefPtr<ID3D11Device> mDevice;
};

D3D11RecycleAllocator::D3D11RecycleAllocator(
    KnowsCompositor* aAllocator, ID3D11Device* aDevice,
    gfx::SurfaceFormat aPreferredFormat)
    : TextureClientRecycleAllocator(aAllocator),
      mDevice(aDevice),
      mCanUseNV12(StaticPrefs::media_wmf_use_nv12_format() &&
                  gfx::DeviceManagerDx::Get()->CanUseNV12()),
      mCanUseP010(StaticPrefs::media_wmf_use_nv12_format() &&
                  gfx::DeviceManagerDx::Get()->CanUseP010()),
      mCanUseP016(StaticPrefs::media_wmf_use_nv12_format() &&
                  gfx::DeviceManagerDx::Get()->CanUseP016()) {
  SetPreferredSurfaceFormat(aPreferredFormat);
}

void D3D11RecycleAllocator::SetPreferredSurfaceFormat(
    gfx::SurfaceFormat aPreferredFormat) {
  if ((aPreferredFormat == gfx::SurfaceFormat::NV12 && mCanUseNV12) ||
      (aPreferredFormat == gfx::SurfaceFormat::P010 && mCanUseP010) ||
      (aPreferredFormat == gfx::SurfaceFormat::P016 && mCanUseP016)) {
    mUsableSurfaceFormat = aPreferredFormat;
    return;
  }
  // We can't handle the native source format, set it to BGRA which will
  // force the caller to convert it later.
  mUsableSurfaceFormat = gfx::SurfaceFormat::B8G8R8A8;
}

already_AddRefed<TextureClient> D3D11RecycleAllocator::CreateOrRecycleClient(
    gfx::ColorSpace2 aColorSpace, gfx::ColorRange aColorRange,
    const gfx::IntSize& aSize) {
  // When CompositorDevice or ContentDevice is updated,
  // we could not reuse old D3D11Textures. It could cause video flickering.
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
  if (!!mImageDevice && mImageDevice != device) {
    ShrinkToMinimumSize();
  }
  mImageDevice = device;

  TextureAllocationFlags allocFlags = TextureAllocationFlags::ALLOC_DEFAULT;
  if (StaticPrefs::media_wmf_use_sync_texture_AtStartup() ||
      mDevice == DeviceManagerDx::Get()->GetCompositorDevice()) {
    // If our device is the compositor device, we don't need any synchronization
    // in practice.
    allocFlags = TextureAllocationFlags::ALLOC_MANUAL_SYNCHRONIZATION;
  }

  D3D11TextureClientAllocationHelper helper(
      mUsableSurfaceFormat, aColorSpace, aColorRange, aSize, allocFlags,
      mDevice, layers::TextureFlags::DEFAULT);

  RefPtr<TextureClient> textureClient = CreateOrRecycle(helper);
  return textureClient.forget();
}

RefPtr<ID3D11Texture2D> D3D11RecycleAllocator::GetStagingTextureNV12(
    gfx::IntSize aSize) {
  if (!mStagingTexture || mStagingTextureSize != aSize) {
    mStagingTexture = nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = aSize.width;
    desc.Height = aSize.height;
    desc.Format = DXGI_FORMAT_NV12;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;

    HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr,
                                          getter_AddRefs(mStagingTexture));
    if (FAILED(hr)) {
      gfxCriticalNoteOnce << "allocating D3D11 NV12 staging texture failed: "
                          << gfx::hexa(hr);
      return nullptr;
    }
    MOZ_ASSERT(mStagingTexture);
    mStagingTextureSize = aSize;
  }

  return mStagingTexture;
}

}  // namespace layers
}  // namespace mozilla
