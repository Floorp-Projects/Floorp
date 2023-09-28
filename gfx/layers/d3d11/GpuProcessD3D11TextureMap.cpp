/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpuProcessD3D11TextureMap.h"

#include "mozilla/layers/D3D11TextureIMFSampleImage.h"
#include "mozilla/layers/HelpersD3D11.h"

namespace mozilla {

namespace layers {

StaticAutoPtr<GpuProcessD3D11TextureMap> GpuProcessD3D11TextureMap::sInstance;

/* static */
void GpuProcessD3D11TextureMap::Init() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  sInstance = new GpuProcessD3D11TextureMap();
}

/* static */
void GpuProcessD3D11TextureMap::Shutdown() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  sInstance = nullptr;
}

/* static */
GpuProcessTextureId GpuProcessD3D11TextureMap::GetNextTextureId() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  return GpuProcessTextureId::GetNext();
}

GpuProcessD3D11TextureMap::GpuProcessD3D11TextureMap()
    : mD3D11TexturesById("D3D11TextureMap::mD3D11TexturesById") {}

GpuProcessD3D11TextureMap::~GpuProcessD3D11TextureMap() {}

void GpuProcessD3D11TextureMap::Register(
    GpuProcessTextureId aTextureId, ID3D11Texture2D* aTexture,
    uint32_t aArrayIndex, const gfx::IntSize& aSize,
    RefPtr<IMFSampleUsageInfo> aUsageInfo) {
  MOZ_RELEASE_ASSERT(aTexture);
  MOZ_RELEASE_ASSERT(aUsageInfo);

  auto textures = mD3D11TexturesById.Lock();

  auto it = textures->find(aTextureId);
  if (it != textures->end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  textures->emplace(aTextureId,
                    TextureHolder(aTexture, aArrayIndex, aSize, aUsageInfo));
}

void GpuProcessD3D11TextureMap::Unregister(GpuProcessTextureId aTextureId) {
  auto textures = mD3D11TexturesById.Lock();

  auto it = textures->find(aTextureId);
  if (it == textures->end()) {
    return;
  }
  textures->erase(it);
}

RefPtr<ID3D11Texture2D> GpuProcessD3D11TextureMap::GetTexture(
    GpuProcessTextureId aTextureId) {
  auto textures = mD3D11TexturesById.Lock();

  auto it = textures->find(aTextureId);
  if (it == textures->end()) {
    return nullptr;
  }

  return it->second.mTexture;
}

Maybe<HANDLE> GpuProcessD3D11TextureMap::GetSharedHandleOfCopiedTexture(
    GpuProcessTextureId aTextureId) {
  TextureHolder holder;
  {
    auto textures = mD3D11TexturesById.Lock();

    auto it = textures->find(aTextureId);
    if (it == textures->end()) {
      return Nothing();
    }

    if (it->second.mCopiedTextureSharedHandle.isSome()) {
      return it->second.mCopiedTextureSharedHandle;
    }

    holder = it->second;
  }

  RefPtr<ID3D11Device> device;
  holder.mTexture->GetDevice(getter_AddRefs(device));
  if (!device) {
    return Nothing();
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    return Nothing();
  }

  CD3D11_TEXTURE2D_DESC newDesc(
      DXGI_FORMAT_NV12, holder.mSize.width, holder.mSize.height, 1, 1,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  RefPtr<ID3D11Texture2D> copiedTexture;
  HRESULT hr =
      device->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(copiedTexture));
  if (FAILED(hr)) {
    return Nothing();
  }

  D3D11_TEXTURE2D_DESC inDesc;
  holder.mTexture->GetDesc(&inDesc);

  D3D11_TEXTURE2D_DESC outDesc;
  copiedTexture->GetDesc(&outDesc);

  UINT height = std::min(inDesc.Height, outDesc.Height);
  UINT width = std::min(inDesc.Width, outDesc.Width);
  D3D11_BOX srcBox = {0, 0, 0, width, height, 1};

  context->CopySubresourceRegion(copiedTexture, 0, 0, 0, 0, holder.mTexture,
                                 holder.mArrayIndex, &srcBox);

  RefPtr<IDXGIResource> resource;
  copiedTexture->QueryInterface((IDXGIResource**)getter_AddRefs(resource));
  if (!resource) {
    return Nothing();
  }

  HANDLE sharedHandle;
  hr = resource->GetSharedHandle(&sharedHandle);
  if (FAILED(hr)) {
    return Nothing();
  }

  RefPtr<ID3D11Query> query;
  CD3D11_QUERY_DESC desc(D3D11_QUERY_EVENT);
  hr = device->CreateQuery(&desc, getter_AddRefs(query));
  if (FAILED(hr) || !query) {
    gfxWarning() << "Could not create D3D11_QUERY_EVENT: " << gfx::hexa(hr);
    return Nothing();
  }

  context->End(query);

  BOOL result;
  bool ret = WaitForFrameGPUQuery(device, context, query, &result);
  if (!ret) {
    gfxCriticalNoteOnce << "WaitForFrameGPUQuery() failed";
  }

  {
    auto textures = mD3D11TexturesById.Lock();

    auto it = textures->find(aTextureId);
    if (it == textures->end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return Nothing();
    }

    // Disable no video copy for future decoded video frames. Since
    // GetSharedHandleOfCopiedTexture() is slow.
    it->second.mIMFSampleUsageInfo->DisableZeroCopyNV12Texture();

    it->second.mCopiedTexture = copiedTexture;
    it->second.mCopiedTextureSharedHandle = Some(sharedHandle);
  }

  return Some(sharedHandle);
}

GpuProcessD3D11TextureMap::TextureHolder::TextureHolder(
    ID3D11Texture2D* aTexture, uint32_t aArrayIndex, const gfx::IntSize& aSize,
    RefPtr<IMFSampleUsageInfo> aUsageInfo)
    : mTexture(aTexture),
      mArrayIndex(aArrayIndex),
      mSize(aSize),
      mIMFSampleUsageInfo(aUsageInfo) {}

}  // namespace layers
}  // namespace mozilla
