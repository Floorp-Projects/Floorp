/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUParent.h"
#include "mozilla/PodOperations.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/layers/CompositableInProcessManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"

namespace mozilla::webgpu {

const uint64_t POLL_TIME_MS = 100;

static mozilla::LazyLogModule sLogger("WebGPU");

// A fixed-capacity buffer for receiving textual error messages from
// `wgpu_bindings`.
//
// The `ToFFI` method returns an `ffi::WGPUErrorBuffer` pointing to our
// buffer, for you to pass to fallible FFI-visible `wgpu_bindings`
// functions. These indicate failure by storing an error message in the
// buffer, which you can retrieve by calling `GetError`.
//
// If you call `ToFFI` on this type, you must also call `GetError` to check for
// an error. Otherwise, the destructor asserts.
//
// TODO: refactor this to avoid stack-allocating the buffer all the time.
class ErrorBuffer {
  // if the message doesn't fit, it will be truncated
  static constexpr unsigned BUFFER_SIZE = 512;
  char mUtf8[BUFFER_SIZE] = {};
  bool mGuard = false;

 public:
  ErrorBuffer() { mUtf8[0] = 0; }
  ErrorBuffer(const ErrorBuffer&) = delete;
  ~ErrorBuffer() { MOZ_ASSERT(!mGuard); }

  ffi::WGPUErrorBuffer ToFFI() {
    mGuard = true;
    ffi::WGPUErrorBuffer errorBuf = {mUtf8, BUFFER_SIZE};
    return errorBuf;
  }

  // If an error message was stored in this buffer, return Some(m)
  // where m is the message as a UTF-8 nsCString. Otherwise, return Nothing.
  //
  // Mark this ErrorBuffer as having been handled, so its destructor
  // won't assert.
  Maybe<nsCString> GetError() {
    mGuard = false;
    if (!mUtf8[0]) {
      return Nothing();
    }
    return Some(nsCString(mUtf8));
  }
};

class PresentationData {
  NS_INLINE_DECL_REFCOUNTING(PresentationData);

 public:
  RawId mDeviceId = 0;
  RawId mQueueId = 0;
  RefPtr<layers::WebRenderImageHost> mImageHost;
  RefPtr<layers::MemoryTextureHost> mTextureHost;
  uint32_t mSourcePitch = 0;
  uint32_t mTargetPitch = 0;
  uint32_t mRowCount = 0;
  int32_t mNextFrameID = 1;
  std::vector<RawId> mUnassignedBufferIds;
  std::vector<RawId> mAvailableBufferIds;
  std::vector<RawId> mQueuedBufferIds;
  Mutex mBuffersLock MOZ_UNANNOTATED;

  PresentationData(RawId aDeviceId, RawId aQueueId,
                   already_AddRefed<layers::WebRenderImageHost> aImageHost,
                   already_AddRefed<layers::MemoryTextureHost> aTextureHost,
                   uint32_t aSourcePitch, uint32_t aTargetPitch, uint32_t aRows,
                   const nsTArray<RawId>& aBufferIds)
      : mDeviceId(aDeviceId),
        mQueueId(aQueueId),
        mImageHost(aImageHost),
        mTextureHost(aTextureHost),
        mSourcePitch(aSourcePitch),
        mTargetPitch(aTargetPitch),
        mRowCount(aRows),
        mBuffersLock("WebGPU presentation buffers") {
    MOZ_COUNT_CTOR(PresentationData);

    for (const RawId id : aBufferIds) {
      mUnassignedBufferIds.push_back(id);
    }
  }

 private:
  ~PresentationData() { MOZ_COUNT_DTOR(PresentationData); }
};

static void FreeAdapter(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_adapter_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeAdapter");
  }
}
static void FreeDevice(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_device_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeDevice");
  }
}
static void FreeShaderModule(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_shader_module_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeShaderModule");
  }
}
static void FreePipelineLayout(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_pipeline_layout_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreePipelineLayout");
  }
}
static void FreeBindGroupLayout(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_bind_group_layout_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeBindGroupLayout");
  }
}
static void FreeBindGroup(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_bind_group_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeBindGroup");
  }
}
static void FreeCommandBuffer(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_command_buffer_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeCommandBuffer");
  }
}
static void FreeRenderBundle(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_render_bundle_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeRenderBundle");
  }
}
static void FreeRenderPipeline(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_render_pipeline_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeRenderPipeline");
  }
}
static void FreeComputePipeline(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_compute_pipeline_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeComputePipeline");
  }
}
static void FreeBuffer(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_buffer_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeBuffer");
  }
}
static void FreeTexture(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_texture_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeTexture");
  }
}
static void FreeTextureView(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_texture_view_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeTextureView");
  }
}
static void FreeSampler(RawId id, void* param) {
  ipc::ByteBuf byteBuf;
  wgpu_server_sampler_free(id, ToFFI(&byteBuf));
  if (!static_cast<WebGPUParent*>(param)->SendDropAction(std::move(byteBuf))) {
    NS_ERROR("Unable FreeSampler");
  }
}
static void FreeSurface(RawId id, void* param) {
  Unused << id;
  Unused << param;
}

static ffi::WGPUIdentityRecyclerFactory MakeFactory(void* param) {
  ffi::WGPUIdentityRecyclerFactory factory;
  PodZero(&factory);
  factory.param = param;
  factory.free_adapter = FreeAdapter;
  factory.free_device = FreeDevice;
  factory.free_pipeline_layout = FreePipelineLayout;
  factory.free_shader_module = FreeShaderModule;
  factory.free_bind_group_layout = FreeBindGroupLayout;
  factory.free_bind_group = FreeBindGroup;
  factory.free_command_buffer = FreeCommandBuffer;
  factory.free_render_bundle = FreeRenderBundle;
  factory.free_render_pipeline = FreeRenderPipeline;
  factory.free_compute_pipeline = FreeComputePipeline;
  factory.free_buffer = FreeBuffer;
  factory.free_texture = FreeTexture;
  factory.free_texture_view = FreeTextureView;
  factory.free_sampler = FreeSampler;
  factory.free_surface = FreeSurface;
  return factory;
}

WebGPUParent::WebGPUParent()
    : mContext(ffi::wgpu_server_new(MakeFactory(this))) {
  mTimer.Start(base::TimeDelta::FromMilliseconds(POLL_TIME_MS), this,
               &WebGPUParent::MaintainDevices);
}

WebGPUParent::~WebGPUParent() = default;

void WebGPUParent::MaintainDevices() {
  ffi::wgpu_server_poll_all_devices(mContext.get(), false);
}

bool WebGPUParent::ForwardError(RawId aDeviceId, ErrorBuffer& aError) {
  // don't do anything if the error is empty
  auto cString = aError.GetError();
  if (!cString) {
    return false;
  }

  ReportError(aDeviceId, cString.value());

  return true;
}

// Generate an error on the Device timeline of aDeviceId.
// aMessage is interpreted as UTF-8.
void WebGPUParent::ReportError(RawId aDeviceId, const nsCString& aMessage) {
  // find the appropriate error scope
  const auto& lookup = mErrorScopeMap.find(aDeviceId);
  if (lookup != mErrorScopeMap.end() && !lookup->second.mStack.IsEmpty()) {
    auto& last = lookup->second.mStack.LastElement();
    if (last.isNothing()) {
      last.emplace(ScopedError{false, aMessage});
    }
  } else {
    // fall back to the uncaptured error handler
    if (!SendDeviceUncapturedError(aDeviceId, aMessage)) {
      NS_ERROR("Unable to SendError");
    }
  }
}

ipc::IPCResult WebGPUParent::RecvInstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions,
    const nsTArray<RawId>& aTargetIds,
    InstanceRequestAdapterResolver&& resolver) {
  ffi::WGPURequestAdapterOptions options = {};
  if (aOptions.mPowerPreference.WasPassed()) {
    options.power_preference = static_cast<ffi::WGPUPowerPreference>(
        aOptions.mPowerPreference.Value());
  }
  options.force_fallback_adapter = aOptions.mForceFallbackAdapter;
  // TODO: make available backends configurable by prefs

  ErrorBuffer error;
  int8_t index = ffi::wgpu_server_instance_request_adapter(
      mContext.get(), &options, aTargetIds.Elements(), aTargetIds.Length(),
      error.ToFFI());

  ByteBuf infoByteBuf;
  // Rust side expects an `Option`, so 0 maps to `None`.
  uint64_t adapterId = 0;
  if (index >= 0) {
    adapterId = aTargetIds[index];
  }
  ffi::wgpu_server_adapter_pack_info(mContext.get(), adapterId,
                                     ToFFI(&infoByteBuf));
  resolver(std::move(infoByteBuf));
  ForwardError(0, error);

  // free the unused IDs
  ipc::ByteBuf dropByteBuf;
  for (size_t i = 0; i < aTargetIds.Length(); ++i) {
    if (static_cast<int8_t>(i) != index) {
      wgpu_server_adapter_free(aTargetIds[i], ToFFI(&dropByteBuf));
    }
  }
  if (dropByteBuf.mData && !SendDropAction(std::move(dropByteBuf))) {
    NS_ERROR("Unable to free free unused adapter IDs");
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterRequestDevice(
    RawId aAdapterId, const ipc::ByteBuf& aByteBuf, RawId aDeviceId,
    AdapterRequestDeviceResolver&& resolver) {
  ErrorBuffer error;
  ffi::wgpu_server_adapter_request_device(
      mContext.get(), aAdapterId, ToFFI(&aByteBuf), aDeviceId, error.ToFFI());
  if (ForwardError(0, error)) {
    resolver(false);
  } else {
    mErrorScopeMap.insert({aAdapterId, ErrorScopeStack()});
    resolver(true);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterDestroy(RawId aAdapterId) {
  ffi::wgpu_server_adapter_drop(mContext.get(), aAdapterId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceDestroy(RawId aDeviceId) {
  ffi::wgpu_server_device_drop(mContext.get(), aDeviceId);
  mErrorScopeMap.erase(aDeviceId);
  return IPC_OK();
}

WebGPUParent::BufferMapData* WebGPUParent::GetBufferMapData(RawId aBufferId) {
  const auto iter = mSharedMemoryMap.find(aBufferId);
  if (iter == mSharedMemoryMap.end()) {
    return nullptr;
  }

  return &iter->second;
}

ipc::IPCResult WebGPUParent::RecvCreateBuffer(RawId aDeviceId, RawId aBufferId,
                                              dom::GPUBufferDescriptor&& aDesc,
                                              MaybeShmem&& aShmem) {
  webgpu::StringHelper label(aDesc.mLabel);

  if (aShmem.type() == MaybeShmem::TShmem) {
    bool hasMapFlags = aDesc.mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                                       dom::GPUBufferUsage_Binding::MAP_READ);
    uint64_t offset = 0;
    uint64_t size = 0;
    if (aDesc.mMappedAtCreation) {
      size = aDesc.mSize;
      MOZ_RELEASE_ASSERT(aShmem.get_Shmem().Size<uint8_t>() >= size);
    }
    mSharedMemoryMap[aBufferId] = {aShmem.get_Shmem(), hasMapFlags, offset,
                                   size};
  }

  ErrorBuffer error;
  ffi::wgpu_server_device_create_buffer(mContext.get(), aDeviceId, aBufferId,
                                        label.Get(), aDesc.mSize, aDesc.mUsage,
                                        aDesc.mMappedAtCreation, error.ToFFI());
  ForwardError(aDeviceId, error);
  return IPC_OK();
}

struct MapRequest {
  RefPtr<WebGPUParent> mParent;
  ffi::WGPUGlobal* mContext;
  ffi::WGPUBufferId mBufferId;
  ffi::WGPUHostMap mHostMap;
  uint64_t mOffset;
  uint64_t mSize;
  WebGPUParent::BufferMapResolver mResolver;
};

static void MapCallback(ffi::WGPUBufferMapAsyncStatus status,
                        uint8_t* userdata) {
  auto* req = reinterpret_cast<MapRequest*>(userdata);

  if (!req->mParent->CanSend()) {
    delete req;
    return;
  }

  BufferMapResult result;

  auto bufferId = req->mBufferId;
  auto* mapData = req->mParent->GetBufferMapData(bufferId);
  MOZ_RELEASE_ASSERT(mapData);

  if (status != ffi::WGPUBufferMapAsyncStatus_Success) {
    // TODO: construct a proper error message from the status code.
    nsCString errorString("mapAsync: Failed to map the buffer");
    result = BufferMapError(errorString);
  } else {
    auto size = req->mSize;
    auto offset = req->mOffset;

    if (req->mHostMap == ffi::WGPUHostMap_Read && size > 0) {
      const auto src = ffi::wgpu_server_buffer_get_mapped_range(
          req->mContext, req->mBufferId, offset, size);

      MOZ_RELEASE_ASSERT(mapData->mShmem.Size<uint8_t>() >= offset + size);
      if (src.ptr != nullptr && src.length >= size) {
        auto dstPtr = mapData->mShmem.get<uint8_t>() + offset;
        memcpy(dstPtr, src.ptr, size);
      }
    }

    result =
        BufferMapSuccess(offset, size, req->mHostMap == ffi::WGPUHostMap_Write);

    mapData->mMappedOffset = offset;
    mapData->mMappedSize = size;
  }

  req->mResolver(std::move(result));
  delete req;
}

ipc::IPCResult WebGPUParent::RecvBufferMap(RawId aBufferId, uint32_t aMode,
                                           uint64_t aOffset, uint64_t aSize,
                                           BufferMapResolver&& aResolver) {
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferMap %" PRIu64 " offset=%" PRIu64 " size=%" PRIu64 "\n",
           aBufferId, aOffset, aSize));

  ffi::WGPUHostMap mode;
  switch (aMode) {
    case dom::GPUMapMode_Binding::READ:
      mode = ffi::WGPUHostMap_Read;
      break;
    case dom::GPUMapMode_Binding::WRITE:
      mode = ffi::WGPUHostMap_Write;
      break;
    default: {
      nsCString errorString(
          "GPUBuffer.mapAsync 'mode' argument must be either GPUMapMode.READ "
          "or GPUMapMode.WRITE");
      aResolver(BufferMapError(errorString));
      return IPC_OK();
    }
  }

  auto* mapData = GetBufferMapData(aBufferId);

  if (!mapData) {
    nsCString errorString("Buffer is not mappable");
    aResolver(BufferMapError(errorString));
    return IPC_OK();
  }

  auto* request =
      new MapRequest{this,    mContext.get(), aBufferId,           mode,
                     aOffset, aSize,          std::move(aResolver)};

  ffi::WGPUBufferMapCallbackC callback = {&MapCallback,
                                          reinterpret_cast<uint8_t*>(request)};
  ffi::wgpu_server_buffer_map(mContext.get(), aBufferId, aOffset, aSize, mode,
                              callback);

  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferUnmap(RawId aDeviceId, RawId aBufferId,
                                             bool aFlush) {
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferUnmap %" PRIu64 " flush=%d\n", aBufferId, aFlush));

  auto* mapData = GetBufferMapData(aBufferId);

  if (mapData && aFlush) {
    uint64_t offset = mapData->mMappedOffset;
    uint64_t size = mapData->mMappedSize;

    uint8_t* srcPtr = mapData->mShmem.get<uint8_t>() + offset;

    const auto mapped = ffi::wgpu_server_buffer_get_mapped_range(
        mContext.get(), aBufferId, offset, size);

    if (mapped.ptr != nullptr && mapped.length >= size) {
      auto shmSize = mapData->mShmem.Size<uint8_t>();
      MOZ_RELEASE_ASSERT(offset <= shmSize);
      MOZ_RELEASE_ASSERT(size <= shmSize - offset);
      memcpy(mapped.ptr, srcPtr, size);
    }

    mapData->mMappedOffset = 0;
    mapData->mMappedSize = 0;
  }

  ErrorBuffer error;
  ffi::wgpu_server_buffer_unmap(mContext.get(), aBufferId, error.ToFFI());
  ForwardError(aDeviceId, error);

  if (mapData && !mapData->mHasMapFlags) {
    // We get here if the buffer was mapped at creation without map flags.
    // We don't need the shared memory anymore.
    DeallocBufferShmem(aBufferId);
  }

  return IPC_OK();
}

void WebGPUParent::DeallocBufferShmem(RawId aBufferId) {
  const auto iter = mSharedMemoryMap.find(aBufferId);
  if (iter != mSharedMemoryMap.end()) {
    DeallocShmem(iter->second.mShmem);
    mSharedMemoryMap.erase(iter);
  }
}

ipc::IPCResult WebGPUParent::RecvBufferDrop(RawId aBufferId) {
  ffi::wgpu_server_buffer_drop(mContext.get(), aBufferId);
  MOZ_LOG(sLogger, LogLevel::Info, ("RecvBufferDrop %" PRIu64 "\n", aBufferId));

  DeallocBufferShmem(aBufferId);

  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferDestroy(RawId aBufferId) {
  ffi::wgpu_server_buffer_destroy(mContext.get(), aBufferId);
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferDestroy %" PRIu64 "\n", aBufferId));

  DeallocBufferShmem(aBufferId);

  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureDestroy(RawId aTextureId) {
  ffi::wgpu_server_texture_drop(mContext.get(), aTextureId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureViewDestroy(RawId aTextureViewId) {
  ffi::wgpu_server_texture_view_drop(mContext.get(), aTextureViewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSamplerDestroy(RawId aSamplerId) {
  ffi::wgpu_server_sampler_drop(mContext.get(), aSamplerId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderFinish(
    RawId aEncoderId, RawId aDeviceId,
    const dom::GPUCommandBufferDescriptor& aDesc) {
  Unused << aDesc;
  ffi::WGPUCommandBufferDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ErrorBuffer error;
  ffi::wgpu_server_encoder_finish(mContext.get(), aEncoderId, &desc,
                                  error.ToFFI());

  ForwardError(aDeviceId, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderDestroy(RawId aEncoderId) {
  ffi::wgpu_server_encoder_drop(mContext.get(), aEncoderId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandBufferDestroy(RawId aCommandBufferId) {
  ffi::wgpu_server_command_buffer_drop(mContext.get(), aCommandBufferId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvRenderBundleDestroy(RawId aBundleId) {
  ffi::wgpu_server_render_bundle_drop(mContext.get(), aBundleId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueSubmit(
    RawId aQueueId, RawId aDeviceId, const nsTArray<RawId>& aCommandBuffers) {
  ErrorBuffer error;
  ffi::wgpu_server_queue_submit(mContext.get(), aQueueId,
                                aCommandBuffers.Elements(),
                                aCommandBuffers.Length(), error.ToFFI());
  ForwardError(aDeviceId, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueWriteAction(RawId aQueueId,
                                                  RawId aDeviceId,
                                                  const ipc::ByteBuf& aByteBuf,
                                                  Shmem&& aShmem) {
  ErrorBuffer error;
  ffi::wgpu_server_queue_write_action(mContext.get(), aQueueId,
                                      ToFFI(&aByteBuf), aShmem.get<uint8_t>(),
                                      aShmem.Size<uint8_t>(), error.ToFFI());
  ForwardError(aDeviceId, error);
  DeallocShmem(aShmem);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupLayoutDestroy(RawId aBindGroupId) {
  ffi::wgpu_server_bind_group_layout_drop(mContext.get(), aBindGroupId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvPipelineLayoutDestroy(RawId aLayoutId) {
  ffi::wgpu_server_pipeline_layout_drop(mContext.get(), aLayoutId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupDestroy(RawId aBindGroupId) {
  ffi::wgpu_server_bind_group_drop(mContext.get(), aBindGroupId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShaderModuleDestroy(RawId aModuleId) {
  ffi::wgpu_server_shader_module_drop(mContext.get(), aModuleId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvComputePipelineDestroy(RawId aPipelineId) {
  ffi::wgpu_server_compute_pipeline_drop(mContext.get(), aPipelineId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvRenderPipelineDestroy(RawId aPipelineId) {
  ffi::wgpu_server_render_pipeline_drop(mContext.get(), aPipelineId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvImplicitLayoutDestroy(
    RawId aImplicitPlId, const nsTArray<RawId>& aImplicitBglIds) {
  ffi::wgpu_server_pipeline_layout_drop(mContext.get(), aImplicitPlId);
  for (const auto& id : aImplicitBglIds) {
    ffi::wgpu_server_bind_group_layout_drop(mContext.get(), id);
  }
  return IPC_OK();
}

// TODO: proper destruction

ipc::IPCResult WebGPUParent::RecvDeviceCreateSwapChain(
    RawId aDeviceId, RawId aQueueId, const RGBDescriptor& aDesc,
    const nsTArray<RawId>& aBufferIds, const CompositableHandle& aHandle) {
  switch (aDesc.format()) {
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid surface format!");
      return IPC_OK();
  }

  constexpr uint32_t kBufferAlignmentMask = 0xff;
  const auto bufferStrideWithMask = CheckedInt<uint32_t>(aDesc.size().width) *
                                        gfx::BytesPerPixel(aDesc.format()) +
                                    kBufferAlignmentMask;
  if (!bufferStrideWithMask.isValid()) {
    MOZ_ASSERT_UNREACHABLE("Invalid width / buffer stride!");
    return IPC_OK();
  }

  const uint32_t bufferStride =
      bufferStrideWithMask.value() & ~kBufferAlignmentMask;
  // GetRGBStride does its own size validation and returns 0 if invalid.
  const auto textureStride = layers::ImageDataSerializer::GetRGBStride(aDesc);
  if (textureStride <= 0) {
    MOZ_ASSERT_UNREACHABLE("Invalid texture stride!");
    return IPC_OK();
  }

  const auto rows = CheckedInt<uint32_t>(aDesc.size().height);
  if (!rows.isValid()) {
    MOZ_ASSERT_UNREACHABLE("Invalid height!");
    return IPC_OK();
  }

  const auto wholeBufferSize = rows * bufferStride;
  const auto wholeTextureSize = rows * textureStride;
  if (!wholeBufferSize.isValid() || !wholeTextureSize.isValid()) {
    MOZ_ASSERT_UNREACHABLE("Invalid total buffer/texture size!");
    return IPC_OK();
  }

  auto* textureHostData = new (fallible) uint8_t[wholeTextureSize.value()];
  if (NS_WARN_IF(!textureHostData)) {
    ReportError(
        aDeviceId,
        "Error in Device::create_swapchain: failed to allocate texture buffer"_ns);
    return IPC_OK();
  }

  layers::TextureInfo texInfo(layers::CompositableType::IMAGE);
  layers::TextureFlags texFlags = layers::TextureFlags::NO_FLAGS;
  wr::ExternalImageId externalId =
      layers::CompositableInProcessManager::GetNextExternalImageId();

  RefPtr<layers::WebRenderImageHost> imageHost =
      layers::CompositableInProcessManager::Add(aHandle, OtherPid(), texInfo);

  auto textureHost =
      MakeRefPtr<layers::MemoryTextureHost>(textureHostData, aDesc, texFlags);
  textureHost->DisableExternalTextures();
  textureHost->EnsureRenderTexture(Some(externalId));

  auto data = MakeRefPtr<PresentationData>(
      aDeviceId, aQueueId, imageHost.forget(), textureHost.forget(),
      bufferStride, textureStride, rows.value(), aBufferIds);
  if (!mCanvasMap.insert({aHandle.Value(), data}).second) {
    NS_ERROR("External image is already registered as WebGPU canvas!");
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateShaderModule(
    RawId aDeviceId, RawId aModuleId, const nsString& aLabel,
    const nsCString& aCode, DeviceCreateShaderModuleResolver&& aOutMessage) {
  // TODO: this should probably be an optional label in the IPC message.
  const nsACString* label = nullptr;
  NS_ConvertUTF16toUTF8 utf8Label(aLabel);
  if (!utf8Label.IsEmpty()) {
    label = &utf8Label;
  }

  ffi::WGPUShaderModuleCompilationMessage message;

  bool ok = ffi::wgpu_server_device_create_shader_module(
      mContext.get(), aDeviceId, aModuleId, label, &aCode, &message);

  nsTArray<WebGPUCompilationMessage> messages;

  if (!ok) {
    WebGPUCompilationMessage msg;
    msg.lineNum = message.line_number;
    msg.linePos = message.line_pos;
    msg.offset = message.utf16_offset;
    msg.length = message.utf16_length;
    msg.message = message.message;
    // wgpu currently only returns errors.
    msg.messageType = WebGPUCompilationMessageType::Error;

    messages.AppendElement(msg);
  }

  aOutMessage(messages);

  return IPC_OK();
}

struct PresentRequest {
  const ffi::WGPUGlobal* mContext;
  RefPtr<PresentationData> mData;
};

static void PresentCallback(ffi::WGPUBufferMapAsyncStatus status,
                            uint8_t* userdata) {
  auto* req = reinterpret_cast<PresentRequest*>(userdata);
  PresentationData* data = req->mData.get();
  // get the buffer ID
  RawId bufferId;
  {
    MutexAutoLock lock(data->mBuffersLock);
    bufferId = data->mQueuedBufferIds.back();
    data->mQueuedBufferIds.pop_back();
    data->mAvailableBufferIds.push_back(bufferId);
  }
  MOZ_LOG(
      sLogger, LogLevel::Info,
      ("PresentCallback for buffer %" PRIu64 " status=%d\n", bufferId, status));
  // copy the data
  if (status == ffi::WGPUBufferMapAsyncStatus_Success) {
    const auto bufferSize = data->mRowCount * data->mSourcePitch;
    const auto mapped = ffi::wgpu_server_buffer_get_mapped_range(
        req->mContext, bufferId, 0, bufferSize);
    MOZ_ASSERT(mapped.length >= bufferSize);
    if (data->mTextureHost) {
      uint8_t* src = mapped.ptr;
      uint8_t* dst = data->mTextureHost->GetBuffer();
      for (uint32_t row = 0; row < data->mRowCount; ++row) {
        memcpy(dst, src, data->mTargetPitch);
        dst += data->mTargetPitch;
        src += data->mSourcePitch;
      }
      layers::CompositorThread()->Dispatch(NS_NewRunnableFunction(
          "webgpu::WebGPUParent::PresentCallback",
          [imageHost = data->mImageHost, texture = data->mTextureHost,
           frameID = data->mNextFrameID++]() {
            AutoTArray<layers::CompositableHost::TimedTexture, 1> textures;

            layers::CompositableHost::TimedTexture* timedTexture =
                textures.AppendElement();

            // TODO(aosmond): We recreate the WebRenderTextureHost object each
            // time so that the pipeline actually updates, as it checks if the
            // texture is the same as before issuing the transaction update to
            // WR. We really ought to be cycling between a front and buffer back
            // here to avoid a race uploading the texture and doing the copy in
            // PresentCallback.
            timedTexture->mTexture = new layers::WebRenderTextureHost(
                layers::TextureFlags::BORROWED_EXTERNAL_ID, texture,
                texture->GetMaybeExternalImageId().ref());
            timedTexture->mTimeStamp = TimeStamp();
            timedTexture->mPictureRect =
                gfx::IntRect(gfx::IntPoint(0, 0), texture->GetSize());
            timedTexture->mFrameID = frameID;
            timedTexture->mProducerID = 0;

            imageHost->UseTextureHost(textures);
          }));
    } else {
      NS_WARNING("WebGPU present skipped: the swapchain is resized!");
    }
    ErrorBuffer error;
    wgpu_server_buffer_unmap(req->mContext, bufferId, error.ToFFI());
    if (auto errorString = error.GetError()) {
      MOZ_LOG(
          sLogger, LogLevel::Info,
          ("WebGPU present: buffer unmap failed: %s\n", errorString->get()));
    }
  } else {
    // TODO: better handle errors
    NS_WARNING("WebGPU frame mapping failed!");
  }
  // free yourself
  delete req;
}

ipc::IPCResult WebGPUParent::GetFrontBufferSnapshot(
    IProtocol* aProtocol, const CompositableHandle& aHandle,
    Maybe<Shmem>& aShmem, gfx::IntSize& aSize) {
  const auto& lookup = mCanvasMap.find(aHandle.Value());
  if (lookup == mCanvasMap.end()) {
    return IPC_OK();
  }

  RefPtr<PresentationData> data = lookup->second.get();
  aSize = data->mTextureHost->GetSize();
  uint32_t stride =
      aSize.width * BytesPerPixel(data->mTextureHost->GetFormat());
  uint32_t len = data->mRowCount * stride;
  Shmem shmem;
  if (!AllocShmem(len, &shmem)) {
    return IPC_OK();
  }

  uint8_t* dst = shmem.get<uint8_t>();
  uint8_t* src = data->mTextureHost->GetBuffer();
  for (uint32_t row = 0; row < data->mRowCount; ++row) {
    memcpy(dst, src, stride);
    src += data->mTargetPitch;
    dst += stride;
  }

  aShmem.emplace(std::move(shmem));
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSwapChainPresent(
    const CompositableHandle& aHandle, RawId aTextureId,
    RawId aCommandEncoderId) {
  // step 0: get the data associated with the swapchain
  const auto& lookup = mCanvasMap.find(aHandle.Value());
  if (lookup == mCanvasMap.end()) {
    NS_WARNING("WebGPU presenting on a destroyed swap chain!");
    return IPC_OK();
  }
  RefPtr<PresentationData> data = lookup->second.get();
  RawId bufferId = 0;
  const auto& size = data->mTextureHost->GetSize();
  const auto bufferSize = data->mRowCount * data->mSourcePitch;

  // step 1: find an available staging buffer, or create one
  {
    MutexAutoLock lock(data->mBuffersLock);
    if (!data->mAvailableBufferIds.empty()) {
      bufferId = data->mAvailableBufferIds.back();
      data->mAvailableBufferIds.pop_back();
    } else if (!data->mUnassignedBufferIds.empty()) {
      bufferId = data->mUnassignedBufferIds.back();
      data->mUnassignedBufferIds.pop_back();

      ffi::WGPUBufferUsages usage =
          WGPUBufferUsages_COPY_DST | WGPUBufferUsages_MAP_READ;

      ErrorBuffer error;
      ffi::wgpu_server_device_create_buffer(mContext.get(), data->mDeviceId,
                                            bufferId, nullptr, bufferSize,
                                            usage, false, error.ToFFI());
      if (ForwardError(data->mDeviceId, error)) {
        return IPC_OK();
      }
    } else {
      bufferId = 0;
    }

    if (bufferId) {
      data->mQueuedBufferIds.insert(data->mQueuedBufferIds.begin(), bufferId);
    }
  }

  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvSwapChainPresent with buffer %" PRIu64 "\n", bufferId));
  if (!bufferId) {
    // TODO: add a warning - no buffer are available!
    return IPC_OK();
  }

  // step 3: submit a copy command for the frame
  ffi::WGPUCommandEncoderDescriptor encoderDesc = {};
  {
    ErrorBuffer error;
    ffi::wgpu_server_device_create_encoder(mContext.get(), data->mDeviceId,
                                           &encoderDesc, aCommandEncoderId,
                                           error.ToFFI());
    if (ForwardError(data->mDeviceId, error)) {
      return IPC_OK();
    }
  }

  const ffi::WGPUImageCopyTexture texView = {
      aTextureId,
  };
  const ffi::WGPUImageDataLayout bufLayout = {
      0,
      data->mSourcePitch,
      0,
  };
  const ffi::WGPUImageCopyBuffer bufView = {
      bufferId,
      bufLayout,
  };
  const ffi::WGPUExtent3d extent = {
      static_cast<uint32_t>(size.width),
      static_cast<uint32_t>(size.height),
      1,
  };
  ffi::wgpu_server_encoder_copy_texture_to_buffer(
      mContext.get(), aCommandEncoderId, &texView, &bufView, &extent);
  ffi::WGPUCommandBufferDescriptor commandDesc = {};
  {
    ErrorBuffer error;
    ffi::wgpu_server_encoder_finish(mContext.get(), aCommandEncoderId,
                                    &commandDesc, error.ToFFI());
    if (ForwardError(data->mDeviceId, error)) {
      return IPC_OK();
    }
  }

  {
    ErrorBuffer error;
    ffi::wgpu_server_queue_submit(mContext.get(), data->mQueueId,
                                  &aCommandEncoderId, 1, error.ToFFI());
    if (ForwardError(data->mDeviceId, error)) {
      return IPC_OK();
    }
  }

  // step 4: request the pixels to be copied into the external texture
  // TODO: this isn't strictly necessary. When WR wants to Lock() the external
  // texture,
  // we can just give it the contents of the last mapped buffer instead of the
  // copy.
  auto* const presentRequest = new PresentRequest{
      mContext.get(),
      data,
  };

  ffi::WGPUBufferMapCallbackC callback = {
      &PresentCallback, reinterpret_cast<uint8_t*>(presentRequest)};
  ffi::wgpu_server_buffer_map(mContext.get(), bufferId, 0, bufferSize,
                              ffi::WGPUHostMap_Read, callback);

  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSwapChainDestroy(
    const CompositableHandle& aHandle) {
  const auto& lookup = mCanvasMap.find(aHandle.Value());
  MOZ_ASSERT(lookup != mCanvasMap.end());
  if (lookup == mCanvasMap.end()) {
    NS_WARNING("WebGPU presenting on a destroyed swap chain!");
    return IPC_OK();
  }

  RefPtr<PresentationData> data = lookup->second.get();
  mCanvasMap.erase(lookup);
  data->mTextureHost = nullptr;
  layers::CompositableInProcessManager::Release(aHandle, OtherPid());

  MutexAutoLock lock(data->mBuffersLock);
  ipc::ByteBuf dropByteBuf;
  for (const auto bid : data->mUnassignedBufferIds) {
    wgpu_server_buffer_free(bid, ToFFI(&dropByteBuf));
  }
  if (dropByteBuf.mData && !SendDropAction(std::move(dropByteBuf))) {
    NS_WARNING("Unable to free an ID for non-assigned buffer");
  }
  for (const auto bid : data->mAvailableBufferIds) {
    ffi::wgpu_server_buffer_drop(mContext.get(), bid);
  }
  for (const auto bid : data->mQueuedBufferIds) {
    ffi::wgpu_server_buffer_drop(mContext.get(), bid);
  }
  return IPC_OK();
}

void WebGPUParent::ActorDestroy(ActorDestroyReason aWhy) {
  mTimer.Stop();
  for (const auto& p : mCanvasMap) {
    const CompositableHandle handle(p.first);
    layers::CompositableInProcessManager::Release(handle, OtherPid());
  }
  mCanvasMap.clear();
  ffi::wgpu_server_poll_all_devices(mContext.get(), true);
  mContext = nullptr;
}

ipc::IPCResult WebGPUParent::RecvDeviceAction(RawId aDeviceId,
                                              const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_device_action(mContext.get(), aDeviceId, ToFFI(&aByteBuf),
                                 error.ToFFI());

  ForwardError(aDeviceId, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceActionWithAck(
    RawId aDeviceId, const ipc::ByteBuf& aByteBuf,
    DeviceActionWithAckResolver&& aResolver) {
  ErrorBuffer error;
  ffi::wgpu_server_device_action(mContext.get(), aDeviceId, ToFFI(&aByteBuf),
                                 error.ToFFI());

  ForwardError(aDeviceId, error);
  aResolver(true);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureAction(RawId aTextureId,
                                               RawId aDeviceId,
                                               const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_texture_action(mContext.get(), aTextureId, ToFFI(&aByteBuf),
                                  error.ToFFI());

  ForwardError(aDeviceId, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderAction(
    RawId aEncoderId, RawId aDeviceId, const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_command_encoder_action(mContext.get(), aEncoderId,
                                          ToFFI(&aByteBuf), error.ToFFI());
  ForwardError(aDeviceId, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBumpImplicitBindGroupLayout(RawId aPipelineId,
                                                             bool aIsCompute,
                                                             uint32_t aIndex,
                                                             RawId aAssignId) {
  ErrorBuffer error;
  if (aIsCompute) {
    ffi::wgpu_server_compute_pipeline_get_bind_group_layout(
        mContext.get(), aPipelineId, aIndex, aAssignId, error.ToFFI());
  } else {
    ffi::wgpu_server_render_pipeline_get_bind_group_layout(
        mContext.get(), aPipelineId, aIndex, aAssignId, error.ToFFI());
  }

  ForwardError(0, error);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDevicePushErrorScope(RawId aDeviceId) {
  const auto& lookup = mErrorScopeMap.find(aDeviceId);
  if (lookup == mErrorScopeMap.end()) {
    NS_WARNING("WebGPU error scopes on a destroyed device!");
    return IPC_OK();
  }

  lookup->second.mStack.EmplaceBack();
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDevicePopErrorScope(
    RawId aDeviceId, DevicePopErrorScopeResolver&& aResolver) {
  const auto& lookup = mErrorScopeMap.find(aDeviceId);
  if (lookup == mErrorScopeMap.end()) {
    NS_WARNING("WebGPU error scopes on a destroyed device!");
    ScopedError error = {true};
    aResolver(Some(error));
    return IPC_OK();
  }

  if (lookup->second.mStack.IsEmpty()) {
    NS_WARNING("WebGPU no error scope to pop!");
    ScopedError error = {true};
    aResolver(Some(error));
    return IPC_OK();
  }

  auto scope = lookup->second.mStack.PopLastElement();
  aResolver(scope);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvGenerateError(RawId aDeviceId,
                                               const nsCString& aMessage) {
  ReportError(aDeviceId, aMessage);
  return IPC_OK();
}

}  // namespace mozilla::webgpu
