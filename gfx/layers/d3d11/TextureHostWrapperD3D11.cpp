/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostWrapperD3D11.h"

#include <d3d11.h>

#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/GpuProcessD3D11TextureMap.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/SharedThreadPool.h"

namespace mozilla {
namespace layers {

TextureWrapperD3D11Allocator::TextureWrapperD3D11Allocator()
    : mThread(SharedThreadPool::Get("TextureUpdate"_ns, 1)),
      mMutex("TextureWrapperD3D11Allocator::mMutex") {}
TextureWrapperD3D11Allocator::~TextureWrapperD3D11Allocator() = default;

RefPtr<ID3D11Texture2D> TextureWrapperD3D11Allocator::CreateOrRecycle(
    gfx::SurfaceFormat aSurfaceFormat, gfx::IntSize aSize) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
  {
    MutexAutoLock lock(mMutex);
    if (!!mDevice && mDevice != device) {
      // Device reset might happen
      ClearAllTextures(lock);
      mDevice = nullptr;
    }

    if (!mDevice) {
      mDevice = device;
      MOZ_ASSERT(mDevice == gfx::DeviceManagerDx::Get()->GetCompositorDevice());
    }

    if (!mDevice) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    if (aSurfaceFormat != gfx::SurfaceFormat::NV12) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return nullptr;
    }

    if (mSize != aSize) {
      ClearAllTextures(lock);
      mSize = aSize;
    }

    if (!mRecycledTextures.empty()) {
      RefPtr<ID3D11Texture2D> texture2D = mRecycledTextures.front();
      mRecycledTextures.pop_front();
      return texture2D;
    }
  }

  CD3D11_TEXTURE2D_DESC desc(
      DXGI_FORMAT_NV12, mSize.width, mSize.height, 1, 1,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

  RefPtr<ID3D11Texture2D> texture2D;
  HRESULT hr =
      device->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture2D));
  if (FAILED(hr) || !texture2D) {
    return nullptr;
  }

  EnsureStagingTextureNV12(device);
  if (!mStagingTexture) {
    return nullptr;
  }

  return texture2D;
}

void TextureWrapperD3D11Allocator::EnsureStagingTextureNV12(
    RefPtr<ID3D11Device> aDevice) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  MOZ_ASSERT(aDevice);

  if (mStagingTexture) {
    return;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = mSize.width;
  desc.Height = mSize.height;
  desc.Format = DXGI_FORMAT_NV12;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;
  desc.SampleDesc.Count = 1;

  RefPtr<ID3D11Texture2D> stagingTexture;
  HRESULT hr =
      aDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(stagingTexture));
  if (FAILED(hr) || !stagingTexture) {
    return;
  }
  mStagingTexture = stagingTexture;
}

RefPtr<ID3D11Texture2D> TextureWrapperD3D11Allocator::GetStagingTextureNV12() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  return mStagingTexture;
}

RefPtr<ID3D11Device> TextureWrapperD3D11Allocator::GetDevice() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  MutexAutoLock lock(mMutex);
  return mDevice;
};

void TextureWrapperD3D11Allocator::ClearAllTextures(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  mStagingTexture = nullptr;
  mRecycledTextures.clear();
}

void TextureWrapperD3D11Allocator::RecycleTexture(
    RefPtr<ID3D11Texture2D>& aTexture) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aTexture);

  RefPtr<ID3D11Device> device;
  aTexture->GetDevice(getter_AddRefs(device));

  D3D11_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  {
    MutexAutoLock lock(mMutex);
    if (device != mDevice || desc.Format != DXGI_FORMAT_NV12 ||
        desc.Width != static_cast<UINT>(mSize.width) ||
        desc.Height != static_cast<UINT>(mSize.height)) {
      return;
    }

    const auto kMaxPooledSized = 5;
    if (mRecycledTextures.size() > kMaxPooledSized) {
      return;
    }
    mRecycledTextures.emplace_back(aTexture);
  }
}

void TextureWrapperD3D11Allocator::RegisterTextureHostWrapper(
    const wr::ExternalImageId& aExternalImageId,
    RefPtr<TextureHost> aTextureHost) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  auto it = mTextureHostWrappers.find(wr::AsUint64(aExternalImageId));
  if (it != mTextureHostWrappers.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  mTextureHostWrappers.emplace(wr::AsUint64(aExternalImageId), aTextureHost);
}

void TextureWrapperD3D11Allocator::UnregisterTextureHostWrapper(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  auto it = mTextureHostWrappers.find(wr::AsUint64(aExternalImageId));
  if (it == mTextureHostWrappers.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  mTextureHostWrappers.erase(it);
}

RefPtr<TextureHost> TextureWrapperD3D11Allocator::GetTextureHostWrapper(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  auto it = mTextureHostWrappers.find(wr::AsUint64(aExternalImageId));
  if (it == mTextureHostWrappers.end()) {
    return nullptr;
  }
  return it->second;
}

// static
RefPtr<TextureHost> TextureHostWrapperD3D11::CreateFromBufferTexture(
    const RefPtr<TextureWrapperD3D11Allocator>& aAllocator,
    TextureHost* aTextureHost) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aAllocator);
  MOZ_ASSERT(aTextureHost);

  if (!XRE_IsGPUProcess()) {
    return nullptr;
  }

  auto* bufferTexture = aTextureHost->AsBufferTextureHost();
  if (!bufferTexture || bufferTexture->GetFormat() != gfx::SurfaceFormat::YUV) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  auto extId = aTextureHost->GetMaybeExternalImageId();
  MOZ_RELEASE_ASSERT(extId.isSome());

  // Reuse TextureHostWrapperD3D11 if it is still valid.
  RefPtr<TextureHost> textureHost =
      aAllocator->GetTextureHostWrapper(extId.ref());
  if (textureHost) {
    return textureHost;
  }

  auto size = bufferTexture->GetSize();
  auto colorDepth = bufferTexture->GetColorDepth();
  auto colorRange = bufferTexture->GetColorRange();
  auto chromaSubsampling = bufferTexture->GetChromaSubsampling();

  // Check if data could be used with NV12
  // XXX support gfx::ColorRange::FULL
  if (size.width % 2 != 0 || size.height % 2 != 0 ||
      colorDepth != gfx::ColorDepth::COLOR_8 ||
      colorRange != gfx::ColorRange::LIMITED ||
      chromaSubsampling != gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT) {
    return nullptr;
  }

  auto id = GpuProcessD3D11TextureMap::GetNextTextureId();
  auto flags = aTextureHost->GetFlags() | TextureFlags::SOFTWARE_DECODED_VIDEO;

  auto colorSpace = ToColorSpace2(bufferTexture->GetYUVColorSpace());

  auto descD3D10 =
      SurfaceDescriptorD3D10(WindowsHandle(nullptr), Some(id),
                             /* arrayIndex */ 0, gfx::SurfaceFormat::NV12, size,
                             colorSpace, colorRange, /* hasKeyedMutex */ false);

  RefPtr<DXGITextureHostD3D11> textureHostD3D11 =
      new DXGITextureHostD3D11(flags, descD3D10);

  RefPtr<TextureHostWrapperD3D11> textureHostWrapper =
      new TextureHostWrapperD3D11(flags, aAllocator, id, textureHostD3D11,
                                  aTextureHost, extId.ref());
  textureHostWrapper->PostTask();

  auto externalImageId = AsyncImagePipelineManager::GetNextExternalImageId();

  textureHost =
      new WebRenderTextureHost(flags, textureHostWrapper, externalImageId);
  aAllocator->RegisterTextureHostWrapper(extId.ref(), textureHost);

  return textureHost;
}

TextureHostWrapperD3D11::TextureHostWrapperD3D11(
    TextureFlags aFlags, const RefPtr<TextureWrapperD3D11Allocator>& aAllocator,
    const GpuProcessTextureId aTextureId,
    DXGITextureHostD3D11* aTextureHostD3D11, TextureHost* aWrappedTextureHost,
    const wr::ExternalImageId aWrappedExternalImageId)
    : TextureHost(TextureHostType::DXGI, aFlags),
      mAllocator(aAllocator),
      mTextureId(aTextureId),
      mTextureHostD3D11(aTextureHostD3D11),
      mWrappedTextureHost(aWrappedTextureHost),
      mWrappedExternalImageId(aWrappedExternalImageId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(mAllocator);
  MOZ_ASSERT(mTextureHostD3D11);
  MOZ_ASSERT(mWrappedTextureHost);

  MOZ_COUNT_CTOR(TextureHostWrapperD3D11);
}

TextureHostWrapperD3D11::~TextureHostWrapperD3D11() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MOZ_COUNT_DTOR(TextureHostWrapperD3D11);

  auto* textureMap = GpuProcessD3D11TextureMap::Get();
  if (textureMap) {
    RefPtr<ID3D11Texture2D> texture = textureMap->GetTexture(mTextureId);
    if (texture) {
      mAllocator->RecycleTexture(texture);
      textureMap->Unregister(mTextureId);
    }
  } else {
    gfxCriticalNoteOnce << "GpuProcessD3D11TextureMap does not exist";
  }
}

void TextureHostWrapperD3D11::PostTask() {
  GpuProcessD3D11TextureMap::Get()->PostUpdateTextureDataTask(
      mTextureId, this, mWrappedTextureHost, mAllocator);
}

bool TextureHostWrapperD3D11::IsValid() { return true; }

gfx::ColorRange TextureHostWrapperD3D11::GetColorRange() const {
  return mTextureHostD3D11->GetColorRange();
}

gfx::IntSize TextureHostWrapperD3D11::GetSize() const {
  return mTextureHostD3D11->GetSize();
}

gfx::SurfaceFormat TextureHostWrapperD3D11::GetFormat() const {
  return mTextureHostD3D11->GetFormat();
}

void TextureHostWrapperD3D11::CreateRenderTexture(
    const wr::ExternalImageId& aExternalImageId) {
  MOZ_ASSERT(mExternalImageId.isSome());

  mTextureHostD3D11->EnsureRenderTexture(mExternalImageId);
}

void TextureHostWrapperD3D11::MaybeDestroyRenderTexture() {
  // TextureHostWrapperD3D11 does not create RenderTexture, then
  // TextureHostWrapperD3D11 does not need to destroy RenderTexture.
  mExternalImageId = Nothing();
}

uint32_t TextureHostWrapperD3D11::NumSubTextures() {
  return mTextureHostD3D11->NumSubTextures();
}

void TextureHostWrapperD3D11::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  mTextureHostD3D11->PushResourceUpdates(aResources, aOp, aImageKeys, aExtID);
}

void TextureHostWrapperD3D11::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys, PushDisplayItemFlagSet aFlags) {
  MOZ_ASSERT(aImageKeys.length() > 0);

  mTextureHostD3D11->PushDisplayItems(aBuilder, aBounds, aClip, aFilter,
                                      aImageKeys, aFlags);
}

bool TextureHostWrapperD3D11::SupportsExternalCompositing(
    WebRenderBackend aBackend) {
  return mTextureHostD3D11->SupportsExternalCompositing(aBackend);
}

void TextureHostWrapperD3D11::UnbindTextureSource() {
  mTextureHostD3D11->UnbindTextureSource();
  // Handle read unlock
  TextureHost::UnbindTextureSource();
}

void TextureHostWrapperD3D11::NotifyNotUsed() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mAllocator->UnregisterTextureHostWrapper(mWrappedExternalImageId);

  MOZ_ASSERT(mWrappedTextureHost);
  if (mWrappedTextureHost) {
    mWrappedTextureHost = nullptr;
  }
  mTextureHostD3D11->NotifyNotUsed();
  TextureHost::NotifyNotUsed();
}

BufferTextureHost* TextureHostWrapperD3D11::AsBufferTextureHost() {
  return nullptr;
}

bool TextureHostWrapperD3D11::IsWrappingSurfaceTextureHost() { return false; }

TextureHostType TextureHostWrapperD3D11::GetTextureHostType() {
  return mTextureHostD3D11->GetTextureHostType();
}

bool TextureHostWrapperD3D11::NeedsDeferredDeletion() const {
  return mTextureHostD3D11->NeedsDeferredDeletion();
}

}  // namespace layers
}  // namespace mozilla
