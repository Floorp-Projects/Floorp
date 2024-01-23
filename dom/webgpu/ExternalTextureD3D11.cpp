/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalTextureD3D11.h"

#include <d3d11.h>

#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"

namespace mozilla::webgpu {

// static
UniquePtr<ExternalTextureD3D11> ExternalTextureD3D11::Create(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  const RefPtr<ID3D11Device> d3d11Device =
      gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  if (!d3d11Device) {
    gfxCriticalNoteOnce << "CompositorDevice does not exist";
    return nullptr;
  }

  if (aFormat.tag != ffi::WGPUTextureFormat_Bgra8Unorm) {
    gfxCriticalNoteOnce << "Non supported format: " << aFormat.tag;
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(
      DXGI_FORMAT_B8G8R8A8_UNORM, aWidth, aHeight, 1, 1,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  if (aUsage & WGPUTextureUsages_STORAGE_BINDING) {
    desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
  }

  desc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr =
      d3d11Device->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "CreateTexture2D failed:  " << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<IDXGIResource1> resource;
  texture->QueryInterface((IDXGIResource1**)getter_AddRefs(resource));
  if (!resource) {
    gfxCriticalNoteOnce << "Failed to get IDXGIResource";
    return 0;
  }

  HANDLE sharedHandle;
  hr = resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &sharedHandle);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "GetSharedHandle failed: " << gfx::hexa(hr);
    return 0;
  }

  RefPtr<gfx::FileHandleWrapper> handle =
      new gfx::FileHandleWrapper(UniqueFileHandle(sharedHandle));

  return MakeUnique<ExternalTextureD3D11>(aWidth, aHeight, aFormat, aUsage,
                                          texture, std::move(handle));
}

ExternalTextureD3D11::ExternalTextureD3D11(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage, const RefPtr<ID3D11Texture2D> aTexture,
    RefPtr<gfx::FileHandleWrapper>&& aSharedHandle)
    : ExternalTexture(aWidth, aHeight, aFormat, aUsage),
      mTexture(aTexture),
      mSharedHandle(std::move(aSharedHandle)) {
  MOZ_ASSERT(mTexture);
}

ExternalTextureD3D11::~ExternalTextureD3D11() {}

void* ExternalTextureD3D11::GetExternalTextureHandle() {
  if (!mSharedHandle) {
    return nullptr;
  }

  return mSharedHandle->GetHandle();
}

Maybe<layers::SurfaceDescriptor> ExternalTextureD3D11::ToSurfaceDescriptor(
    Maybe<gfx::FenceInfo>& aFenceInfo) {
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  return Some(layers::SurfaceDescriptorD3D10(
      mSharedHandle,
      /* gpuProcessTextureId */ Nothing(),
      /* arrayIndex */ 0, format, gfx::IntSize(mWidth, mHeight),
      gfx::ColorSpace2::SRGB, gfx::ColorRange::FULL,
      /* hasKeyedMutex */ false, aFenceInfo));
}

void ExternalTextureD3D11::GetSnapshot(const ipc::Shmem& aDestShmem,
                                       const gfx::IntSize& aSize) {
  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(getter_AddRefs(device));
  if (!device) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "Failed to get ID3D11Device";
    return;
  }

  RefPtr<ID3D11DeviceContext> deviceContext;
  device->GetImmediateContext(getter_AddRefs(deviceContext));
  if (!deviceContext) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "Failed to get ID3D11DeviceContext";
    return;
  }

  D3D11_TEXTURE2D_DESC textureDesc = {0};
  mTexture->GetDesc(&textureDesc);

  textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  textureDesc.Usage = D3D11_USAGE_STAGING;
  textureDesc.BindFlags = 0;
  textureDesc.MiscFlags = 0;
  textureDesc.MipLevels = 1;

  RefPtr<ID3D11Texture2D> cpuTexture;
  HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr,
                                       getter_AddRefs(cpuTexture));
  if (FAILED(hr)) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNote << "Failed to create ID3D11Texture2D: " << gfx::hexa(hr);
    return;
  }

  deviceContext->CopyResource(cpuTexture, mTexture);

  D3D11_MAPPED_SUBRESOURCE map;
  hr = deviceContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &map);
  if (FAILED(hr)) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNote << "Failed to map ID3D11Texture2D: " << gfx::hexa(hr);
    return;
  }

  const uint32_t stride = layers::ImageDataSerializer::ComputeRGBStride(
      gfx::SurfaceFormat::B8G8R8A8, aSize.width);
  uint8_t* src = static_cast<uint8_t*>(map.pData);
  uint8_t* dst = aDestShmem.get<uint8_t>();

  MOZ_ASSERT(stride * aSize.height <= aDestShmem.Size<uint8_t>());

  for (int y = 0; y < aSize.height; y++) {
    memcpy(dst, src, stride);
    src += map.RowPitch;
    dst += stride;
  }
}

}  // namespace mozilla::webgpu
