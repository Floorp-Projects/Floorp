/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpuProcessD3D11TextureMap.h"

#include "libyuv.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/D3D11TextureIMFSampleImage.h"
#include "mozilla/layers/HelpersD3D11.h"
#include "mozilla/layers/TextureHostWrapperD3D11.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/webrender/RenderThread.h"

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
    : mMonitor("GpuProcessD3D11TextureMap::mMonitor") {}

GpuProcessD3D11TextureMap::~GpuProcessD3D11TextureMap() {}

void GpuProcessD3D11TextureMap::Register(
    GpuProcessTextureId aTextureId, ID3D11Texture2D* aTexture,
    uint32_t aArrayIndex, const gfx::IntSize& aSize,
    RefPtr<IMFSampleUsageInfo> aUsageInfo) {
  MonitorAutoLock lock(mMonitor);
  Register(lock, aTextureId, aTexture, aArrayIndex, aSize, aUsageInfo);
}
void GpuProcessD3D11TextureMap::Register(
    const MonitorAutoLock& aProofOfLock, GpuProcessTextureId aTextureId,
    ID3D11Texture2D* aTexture, uint32_t aArrayIndex, const gfx::IntSize& aSize,
    RefPtr<IMFSampleUsageInfo> aUsageInfo) {
  MOZ_RELEASE_ASSERT(aTexture);

  auto it = mD3D11TexturesById.find(aTextureId);
  if (it != mD3D11TexturesById.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  mD3D11TexturesById.emplace(
      aTextureId, TextureHolder(aTexture, aArrayIndex, aSize, aUsageInfo));
}

void GpuProcessD3D11TextureMap::Unregister(GpuProcessTextureId aTextureId) {
  MonitorAutoLock lock(mMonitor);

  auto it = mD3D11TexturesById.find(aTextureId);
  if (it == mD3D11TexturesById.end()) {
    return;
  }
  mD3D11TexturesById.erase(it);
}

RefPtr<ID3D11Texture2D> GpuProcessD3D11TextureMap::GetTexture(
    GpuProcessTextureId aTextureId) {
  MonitorAutoLock lock(mMonitor);

  auto it = mD3D11TexturesById.find(aTextureId);
  if (it == mD3D11TexturesById.end()) {
    return nullptr;
  }

  return it->second.mTexture;
}

Maybe<HANDLE> GpuProcessD3D11TextureMap::GetSharedHandleOfCopiedTexture(
    GpuProcessTextureId aTextureId) {
  TextureHolder holder;
  {
    MonitorAutoLock lock(mMonitor);

    auto it = mD3D11TexturesById.find(aTextureId);
    if (it == mD3D11TexturesById.end()) {
      return Nothing();
    }

    if (it->second.mCopiedTextureSharedHandle) {
      return Some(it->second.mCopiedTextureSharedHandle->GetHandle());
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
  newDesc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

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

  RefPtr<IDXGIResource1> resource;
  copiedTexture->QueryInterface((IDXGIResource1**)getter_AddRefs(resource));
  if (!resource) {
    return Nothing();
  }

  HANDLE sharedHandle;
  hr = resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &sharedHandle);
  if (FAILED(hr)) {
    return Nothing();
  }

  RefPtr<gfx::FileHandleWrapper> handle =
      new gfx::FileHandleWrapper(UniqueFileHandle(sharedHandle));

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
    MonitorAutoLock lock(mMonitor);

    auto it = mD3D11TexturesById.find(aTextureId);
    if (it == mD3D11TexturesById.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return Nothing();
    }

    // Disable no video copy for future decoded video frames. Since
    // GetSharedHandleOfCopiedTexture() is slow.
    if (it->second.mIMFSampleUsageInfo) {
      it->second.mIMFSampleUsageInfo->DisableZeroCopyNV12Texture();
    }

    it->second.mCopiedTexture = copiedTexture;
    it->second.mCopiedTextureSharedHandle = handle;
  }

  return Some(handle->GetHandle());
}

size_t GpuProcessD3D11TextureMap::GetWaitingTextureCount() const {
  MonitorAutoLock lock(mMonitor);
  return mWaitingTextures.size();
}

bool GpuProcessD3D11TextureMap::WaitTextureReady(
    const GpuProcessTextureId aTextureId) {
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());

  MonitorAutoLock lock(mMonitor);
  auto it = mWaitingTextures.find(aTextureId);
  if (it == mWaitingTextures.end()) {
    return true;
  }

  auto start = TimeStamp::Now();
  const TimeDuration timeout = TimeDuration::FromMilliseconds(1000);
  while (1) {
    CVStatus status = mMonitor.Wait(timeout);
    if (status == CVStatus::Timeout) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      gfxCriticalNoteOnce << "GpuProcessTextureI wait time out id:"
                          << uint64_t(aTextureId);
      return false;
    }

    auto it = mWaitingTextures.find(aTextureId);
    if (it == mWaitingTextures.end()) {
      break;
    }
  }

  auto end = TimeStamp::Now();
  const auto durationMs = static_cast<uint32_t>((end - start).ToMilliseconds());
  nsPrintfCString marker("TextureReadyWait %ums ", durationMs);
  PROFILER_MARKER_TEXT("PresentWait", GRAPHICS, {}, marker);

  return true;
}

void GpuProcessD3D11TextureMap::PostUpdateTextureDataTask(
    const GpuProcessTextureId aTextureId, TextureHost* aTextureHost,
    TextureHost* aWrappedTextureHost,
    TextureWrapperD3D11Allocator* aAllocator) {
  {
    MonitorAutoLock lock(mMonitor);

    auto it = mWaitingTextures.find(aTextureId);
    if (it != mWaitingTextures.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return;
    }

    auto updatingTexture = MakeUnique<UpdatingTextureHolder>(
        aTextureId, aTextureHost, aWrappedTextureHost, aAllocator);

    mWaitingTextures.emplace(aTextureId);
    mWaitingTextureQueue.push_back(std::move(updatingTexture));
  }

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "GpuProcessD3D11TextureMap::PostUpdateTextureDataTaskRunnable", []() {
        GpuProcessD3D11TextureMap::Get()->HandleInTextureUpdateThread();
      });
  nsCOMPtr<nsIEventTarget> thread = aAllocator->mThread;
  thread->Dispatch(runnable.forget());
}

void GpuProcessD3D11TextureMap::HandleInTextureUpdateThread() {
  UniquePtr<UpdatingTextureHolder> textureHolder;
  {
    MonitorAutoLock lock(mMonitor);

    if (mWaitingTextureQueue.empty()) {
      return;
    }
    textureHolder = std::move(mWaitingTextureQueue.front());
    mWaitingTextureQueue.pop_front();
  }

  RefPtr<ID3D11Texture2D> texture = UpdateTextureData(textureHolder.get());

  {
    MonitorAutoLock lock(mMonitor);
    if (texture) {
      auto size = textureHolder->mWrappedTextureHost->GetSize();
      Register(lock, textureHolder->mTextureId, texture, /* aArrayIndex */ 0,
               size, /* aUsageInfo */ nullptr);
    }
    mWaitingTextures.erase(textureHolder->mTextureId);
    MOZ_ASSERT(mWaitingTextures.size() == mWaitingTextureQueue.size());
    mMonitor.Notify();
  }

  // Release UpdatingTextureHolder in CompositorThread
  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "GpuProcessD3D11TextureMap::HandleInTextureUpdateThread::Runnable",
      [textureHolder = std::move(textureHolder)]() mutable {
        textureHolder = nullptr;
      });
  CompositorThread()->Dispatch(runnable.forget());
}

RefPtr<ID3D11Texture2D> GpuProcessD3D11TextureMap::UpdateTextureData(
    UpdatingTextureHolder* aHolder) {
  MOZ_ASSERT(aHolder);
  MOZ_ASSERT(aHolder->mAllocator->mThread->IsOnCurrentThread());

  auto* bufferTexture = aHolder->mWrappedTextureHost->AsBufferTextureHost();
  MOZ_ASSERT(bufferTexture);
  if (!bufferTexture) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  auto size = bufferTexture->GetSize();

  RefPtr<ID3D11Texture2D> texture2D =
      aHolder->mAllocator->CreateOrRecycle(gfx::SurfaceFormat::NV12, size);
  if (!texture2D) {
    return nullptr;
  }

  RefPtr<ID3D11Texture2D> stagingTexture =
      aHolder->mAllocator->GetStagingTextureNV12();
  if (!stagingTexture) {
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  RefPtr<ID3D11Device> device = aHolder->mAllocator->GetDevice();
  device->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    return nullptr;
  }

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = device->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));
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
    gfxCriticalNote
        << "GpuProcessD3D11TextureMap::UpdateTextureData lock failed";
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
                              destStride * size.height;
  // Convert I420 to NV12,
  const uint8_t* yChannel = bufferTexture->GetYChannel();
  const uint8_t* cbChannel = bufferTexture->GetCbChannel();
  const uint8_t* crChannel = bufferTexture->GetCrChannel();
  int32_t yStride = bufferTexture->GetYStride();
  int32_t cbCrStride = bufferTexture->GetCbCrStride();

  libyuv::I420ToNV12(yChannel, yStride, cbChannel, cbCrStride, crChannel,
                     cbCrStride, yDestPlaneStart, destStride, uvDestPlaneStart,
                     destStride, size.width, size.height);

  context->Unmap(stagingTexture, 0);

  context->CopyResource(texture2D, stagingTexture);

  return texture2D;
}

GpuProcessD3D11TextureMap::TextureHolder::TextureHolder(
    ID3D11Texture2D* aTexture, uint32_t aArrayIndex, const gfx::IntSize& aSize,
    RefPtr<IMFSampleUsageInfo> aUsageInfo)
    : mTexture(aTexture),
      mArrayIndex(aArrayIndex),
      mSize(aSize),
      mIMFSampleUsageInfo(aUsageInfo) {}

GpuProcessD3D11TextureMap::UpdatingTextureHolder::UpdatingTextureHolder(
    const GpuProcessTextureId aTextureId, TextureHost* aTextureHost,
    TextureHost* aWrappedTextureHost, TextureWrapperD3D11Allocator* aAllocator)
    : mTextureId(aTextureId),
      mTextureHost(aTextureHost),
      mWrappedTextureHost(aWrappedTextureHost),
      mAllocator(aAllocator) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
}

GpuProcessD3D11TextureMap::UpdatingTextureHolder::~UpdatingTextureHolder() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
}

}  // namespace layers
}  // namespace mozilla
