/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUParent.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace webgpu {

const uint64_t POLL_TIME_MS = 100;

static mozilla::LazyLogModule sLogger("WebGPU");

// A helper class to force error checks coming across FFI.
// It will assert in destructor if unchecked.
// TODO: refactor this to avoid stack-allocating the buffer all the time.
class ErrorBuffer {
  // if the message doesn't fit, it will be truncated
  static constexpr unsigned BUFFER_SIZE = 256;
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

  bool CheckAndForward(PWebGPUParent* aParent, RawId aDeviceId) {
    mGuard = false;
    if (!mUtf8[0]) {
      return false;
    }
    nsAutoCString cString(mUtf8);
    if (!aParent->SendError(aDeviceId, cString)) {
      NS_ERROR("Unable to SendError");
    }
    return true;
  }
};

class PresentationData {
  NS_INLINE_DECL_REFCOUNTING(PresentationData);

 public:
  RawId mDeviceId = 0;
  RawId mQueueId = 0;
  RefPtr<layers::MemoryTextureHost> mTextureHost;
  uint32_t mSourcePitch = 0;
  uint32_t mTargetPitch = 0;
  uint32_t mRowCount = 0;
  std::vector<RawId> mUnassignedBufferIds;
  std::vector<RawId> mAvailableBufferIds;
  std::vector<RawId> mQueuedBufferIds;
  Mutex mBuffersLock;

  PresentationData() : mBuffersLock("WebGPU presentation buffers") {
    MOZ_COUNT_CTOR(PresentationData);
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
static void FreeSwapChain(RawId id, void* param) {
  Unused << id;
  Unused << param;
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
  ffi::WGPUIdentityRecyclerFactory factory = {param};
  factory.free_adapter = FreeAdapter;
  factory.free_device = FreeDevice;
  factory.free_swap_chain = FreeSwapChain;
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
  ffi::wgpu_server_poll_all_devices(mContext, false);
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
  // TODO: make available backends configurable by prefs

  ErrorBuffer error;
  int8_t index = ffi::wgpu_server_instance_request_adapter(
      mContext, &options, aTargetIds.Elements(), aTargetIds.Length(),
      error.ToFFI());

  ByteBuf infoByteBuf;
  // Rust side expects an `Option`, so 0 maps to `None`.
  uint64_t adapterId = 0;
  if (index >= 0) {
    adapterId = aTargetIds[index];
  }
  ffi::wgpu_server_adapter_pack_info(mContext, adapterId, ToFFI(&infoByteBuf));
  resolver(std::move(infoByteBuf));
  error.CheckAndForward(this, 0);

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
    RawId aSelfId, const ipc::ByteBuf& aByteBuf, RawId aNewId) {
  ErrorBuffer error;
  ffi::wgpu_server_adapter_request_device(mContext, aSelfId, ToFFI(&aByteBuf),
                                          aNewId, error.ToFFI());
  error.CheckAndForward(this, 0);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterDestroy(RawId aSelfId) {
  ffi::wgpu_server_adapter_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceDestroy(RawId aSelfId) {
  ffi::wgpu_server_device_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferReturnShmem(RawId aSelfId,
                                                   Shmem&& aShmem) {
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferReturnShmem %" PRIu64 "\n", aSelfId));
  mSharedMemoryMap[aSelfId] = aShmem;
  return IPC_OK();
}

struct MapRequest {
  const ffi::WGPUGlobal* const mContext;
  ffi::WGPUBufferId mBufferId;
  ffi::WGPUHostMap mHostMap;
  uint64_t mOffset;
  ipc::Shmem mShmem;
  WebGPUParent::BufferMapResolver mResolver;
  MapRequest(const ffi::WGPUGlobal* context, ffi::WGPUBufferId bufferId,
             ffi::WGPUHostMap hostMap, uint64_t offset, ipc::Shmem&& shmem,
             WebGPUParent::BufferMapResolver&& resolver)
      : mContext(context),
        mBufferId(bufferId),
        mHostMap(hostMap),
        mOffset(offset),
        mShmem(shmem),
        mResolver(resolver) {}
};

static void MapCallback(ffi::WGPUBufferMapAsyncStatus status,
                        uint8_t* userdata) {
  auto* req = reinterpret_cast<MapRequest*>(userdata);
  // TODO: better handle errors
  MOZ_ASSERT(status == ffi::WGPUBufferMapAsyncStatus_Success);
  if (req->mHostMap == ffi::WGPUHostMap_Read) {
    const uint8_t* ptr = ffi::wgpu_server_buffer_get_mapped_range(
        req->mContext, req->mBufferId, req->mOffset,
        req->mShmem.Size<uint8_t>());
    memcpy(req->mShmem.get<uint8_t>(), ptr, req->mShmem.Size<uint8_t>());
  }
  req->mResolver(std::move(req->mShmem));
  delete req;
}

ipc::IPCResult WebGPUParent::RecvBufferMap(RawId aSelfId,
                                           ffi::WGPUHostMap aHostMap,
                                           uint64_t aOffset, uint64_t aSize,
                                           BufferMapResolver&& aResolver) {
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferMap %" PRIu64 " offset=%" PRIu64 " size=%" PRIu64 "\n",
           aSelfId, aOffset, aSize));
  auto& shmem = mSharedMemoryMap[aSelfId];
  if (!shmem.IsReadable()) {
    MOZ_LOG(sLogger, LogLevel::Error, ("\tshmem is empty\n"));
    return IPC_OK();
  }

  auto* request = new MapRequest(mContext, aSelfId, aHostMap, aOffset,
                                 std::move(shmem), std::move(aResolver));
  ffi::WGPUBufferMapOperation mapOperation = {
      aHostMap, &MapCallback, reinterpret_cast<uint8_t*>(request)};
  ffi::wgpu_server_buffer_map(mContext, aSelfId, aOffset, aSize, mapOperation);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferUnmap(RawId aSelfId, Shmem&& aShmem,
                                             bool aFlush, bool aKeepShmem) {
  if (aFlush) {
    // TODO: flush exact modified sub-range
    uint8_t* ptr = ffi::wgpu_server_buffer_get_mapped_range(
        mContext, aSelfId, 0, aShmem.Size<uint8_t>());
    MOZ_ASSERT(ptr != nullptr);
    memcpy(ptr, aShmem.get<uint8_t>(), aShmem.Size<uint8_t>());
  }

  ffi::wgpu_server_buffer_unmap(mContext, aSelfId);

  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferUnmap %" PRIu64 " flush=%d\n", aSelfId, aFlush));
  const auto iter = mSharedMemoryMap.find(aSelfId);
  if (iter != mSharedMemoryMap.end()) {
    iter->second = aShmem;
  } else if (aKeepShmem) {
    mSharedMemoryMap[aSelfId] = aShmem;
  } else {
    // we are here if the buffer was mapped at creation, but doesn't have any
    // mapping flags
    DeallocShmem(aShmem);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_buffer_drop(mContext, aSelfId);
  MOZ_LOG(sLogger, LogLevel::Info,
          ("RecvBufferDestroy %" PRIu64 "\n", aSelfId));

  const auto iter = mSharedMemoryMap.find(aSelfId);
  if (iter != mSharedMemoryMap.end()) {
    DeallocShmem(iter->second);
    mSharedMemoryMap.erase(iter);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureDestroy(RawId aSelfId) {
  ffi::wgpu_server_texture_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureViewDestroy(RawId aSelfId) {
  ffi::wgpu_server_texture_view_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSamplerDestroy(RawId aSelfId) {
  ffi::wgpu_server_sampler_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderFinish(
    RawId aSelfId, RawId aDeviceId,
    const dom::GPUCommandBufferDescriptor& aDesc) {
  Unused << aDesc;
  ffi::WGPUCommandBufferDescriptor desc = {};
  ErrorBuffer error;
  ffi::wgpu_server_encoder_finish(mContext, aSelfId, &desc, error.ToFFI());

  error.CheckAndForward(this, aDeviceId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderDestroy(RawId aSelfId) {
  ffi::wgpu_server_encoder_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_command_buffer_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueSubmit(
    RawId aSelfId, RawId aDeviceId, const nsTArray<RawId>& aCommandBuffers) {
  ErrorBuffer error;
  ffi::wgpu_server_queue_submit(mContext, aSelfId, aCommandBuffers.Elements(),
                                aCommandBuffers.Length(), error.ToFFI());
  error.CheckAndForward(this, aDeviceId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueWriteAction(RawId aSelfId,
                                                  RawId aDeviceId,
                                                  const ipc::ByteBuf& aByteBuf,
                                                  Shmem&& aShmem) {
  ErrorBuffer error;
  ffi::wgpu_server_queue_write_action(mContext, aSelfId, ToFFI(&aByteBuf),
                                      aShmem.get<uint8_t>(),
                                      aShmem.Size<uint8_t>(), error.ToFFI());
  error.CheckAndForward(this, aDeviceId);
  DeallocShmem(aShmem);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupLayoutDestroy(RawId aSelfId) {
  ffi::wgpu_server_bind_group_layout_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvPipelineLayoutDestroy(RawId aSelfId) {
  ffi::wgpu_server_pipeline_layout_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupDestroy(RawId aSelfId) {
  ffi::wgpu_server_bind_group_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShaderModuleDestroy(RawId aSelfId) {
  ffi::wgpu_server_shader_module_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvComputePipelineDestroy(RawId aSelfId) {
  ffi::wgpu_server_compute_pipeline_drop(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvRenderPipelineDestroy(RawId aSelfId) {
  ffi::wgpu_server_render_pipeline_drop(mContext, aSelfId);
  return IPC_OK();
}

// TODO: proper destruction
static const uint64_t kBufferAlignment = 0x100;

static uint64_t Align(uint64_t value) {
  return (value | (kBufferAlignment - 1)) + 1;
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateSwapChain(
    RawId aSelfId, RawId aQueueId, const RGBDescriptor& aDesc,
    const nsTArray<RawId>& aBufferIds, ExternalImageId aExternalId) {
  const auto rows = aDesc.size().height;
  const auto bufferStride =
      Align(static_cast<uint64_t>(aDesc.size().width) * 4);
  const auto textureStride = layers::ImageDataSerializer::GetRGBStride(aDesc);
  const auto wholeBufferSize = CheckedInt<size_t>(textureStride) * rows;
  if (!wholeBufferSize.isValid()) {
    NS_ERROR("Invalid total buffer size!");
    return IPC_OK();
  }
  auto* textureHostData = new (fallible) uint8_t[wholeBufferSize.value()];
  if (!textureHostData) {
    NS_ERROR("Unable to allocate host data!");
    return IPC_OK();
  }
  RefPtr<layers::MemoryTextureHost> textureHost = new layers::MemoryTextureHost(
      textureHostData, aDesc, layers::TextureFlags::NO_FLAGS);
  textureHost->DisableExternalTextures();
  textureHost->CreateRenderTexture(aExternalId);
  nsTArray<RawId> bufferIds(aBufferIds.Clone());
  RefPtr<PresentationData> data = new PresentationData();
  data->mDeviceId = aSelfId;
  data->mQueueId = aQueueId;
  data->mTextureHost = textureHost;
  data->mSourcePitch = bufferStride;
  data->mTargetPitch = textureStride;
  data->mRowCount = rows;
  for (const RawId id : bufferIds) {
    data->mUnassignedBufferIds.push_back(id);
  }
  if (!mCanvasMap.insert({AsUint64(aExternalId), data}).second) {
    NS_ERROR("External image is already registered as WebGPU canvas!");
  }
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
  data->mBuffersLock.Lock();
  RawId bufferId = data->mQueuedBufferIds.back();
  data->mQueuedBufferIds.pop_back();
  data->mAvailableBufferIds.push_back(bufferId);
  data->mBuffersLock.Unlock();
  // copy the data
  if (status == ffi::WGPUBufferMapAsyncStatus_Success) {
    const auto bufferSize = data->mRowCount * data->mSourcePitch;
    const uint8_t* ptr = ffi::wgpu_server_buffer_get_mapped_range(
        req->mContext, bufferId, 0, bufferSize);
    uint8_t* dst = data->mTextureHost->GetBuffer();
    for (uint32_t row = 0; row < data->mRowCount; ++row) {
      memcpy(dst, ptr, data->mTargetPitch);
      dst += data->mTargetPitch;
      ptr += data->mSourcePitch;
    }
    wgpu_server_buffer_unmap(req->mContext, bufferId);
  } else {
    // TODO: better handle errors
    NS_WARNING("WebGPU frame mapping failed!");
  }
  // free yourself
  delete req;
}

ipc::IPCResult WebGPUParent::RecvSwapChainPresent(
    wr::ExternalImageId aExternalId, RawId aTextureId,
    RawId aCommandEncoderId) {
  // step 0: get the data associated with the swapchain
  const auto& lookup = mCanvasMap.find(AsUint64(aExternalId));
  if (lookup == mCanvasMap.end()) {
    NS_WARNING("WebGPU presenting on a destroyed swap chain!");
    return IPC_OK();
  }
  RefPtr<PresentationData> data = lookup->second.get();
  RawId bufferId = 0;
  const auto& size = data->mTextureHost->GetSize();
  const auto bufferSize = data->mRowCount * data->mSourcePitch;

  // step 1: find an available staging buffer, or create one
  data->mBuffersLock.Lock();
  if (!data->mAvailableBufferIds.empty()) {
    bufferId = data->mAvailableBufferIds.back();
    data->mAvailableBufferIds.pop_back();
  } else if (!data->mUnassignedBufferIds.empty()) {
    bufferId = data->mUnassignedBufferIds.back();
    data->mUnassignedBufferIds.pop_back();

    ffi::WGPUBufferUsage usage =
        WGPUBufferUsage_COPY_DST | WGPUBufferUsage_MAP_READ;
    ffi::WGPUBufferDescriptor desc = {};
    desc.size = bufferSize;
    desc.usage = usage;

    ErrorBuffer error;
    ffi::wgpu_server_device_create_buffer(mContext, data->mDeviceId, &desc,
                                          bufferId, error.ToFFI());
    if (error.CheckAndForward(this, data->mDeviceId)) {
      return IPC_OK();
    }
  } else {
    bufferId = 0;
  }
  if (bufferId) {
    data->mQueuedBufferIds.insert(data->mQueuedBufferIds.begin(), bufferId);
  }
  data->mBuffersLock.Unlock();
  if (!bufferId) {
    // TODO: add a warning - no buffer are available!
    return IPC_OK();
  }

  // step 3: submit a copy command for the frame
  ffi::WGPUCommandEncoderDescriptor encoderDesc = {};
  {
    ErrorBuffer error;
    ffi::wgpu_server_device_create_encoder(mContext, data->mDeviceId,
                                           &encoderDesc, aCommandEncoderId,
                                           error.ToFFI());
    if (error.CheckAndForward(this, data->mDeviceId)) {
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
  ffi::wgpu_server_encoder_copy_texture_to_buffer(mContext, aCommandEncoderId,
                                                  &texView, &bufView, &extent);
  ffi::WGPUCommandBufferDescriptor commandDesc = {};
  {
    ErrorBuffer error;
    ffi::wgpu_server_encoder_finish(mContext, aCommandEncoderId, &commandDesc,
                                    error.ToFFI());
    if (error.CheckAndForward(this, data->mDeviceId)) {
      return IPC_OK();
    }
  }

  {
    ErrorBuffer error;
    ffi::wgpu_server_queue_submit(mContext, data->mQueueId, &aCommandEncoderId,
                                  1, error.ToFFI());
    if (error.CheckAndForward(this, data->mDeviceId)) {
      return IPC_OK();
    }
  }

  // step 4: request the pixels to be copied into the external texture
  // TODO: this isn't strictly necessary. When WR wants to Lock() the external
  // texture,
  // we can just give it the contents of the last mapped buffer instead of the
  // copy.
  auto* const presentRequest = new PresentRequest{
      mContext,
      data,
  };

  ffi::WGPUBufferMapOperation mapOperation = {
      ffi::WGPUHostMap_Read, &PresentCallback,
      reinterpret_cast<uint8_t*>(presentRequest)};
  ffi::wgpu_server_buffer_map(mContext, bufferId, 0, bufferSize, mapOperation);

  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSwapChainDestroy(
    wr::ExternalImageId aExternalId) {
  const auto& lookup = mCanvasMap.find(AsUint64(aExternalId));
  MOZ_ASSERT(lookup != mCanvasMap.end());
  RefPtr<PresentationData> data = lookup->second.get();
  mCanvasMap.erase(AsUint64(aExternalId));
  data->mTextureHost = nullptr;
  layers::TextureHost::DestroyRenderTexture(aExternalId);

  data->mBuffersLock.Lock();
  ipc::ByteBuf dropByteBuf;
  for (const auto bid : data->mUnassignedBufferIds) {
    wgpu_server_buffer_free(bid, ToFFI(&dropByteBuf));
  }
  if (dropByteBuf.mData && !SendDropAction(std::move(dropByteBuf))) {
    NS_WARNING("Unable to free an ID for non-assigned buffer");
  }
  for (const auto bid : data->mAvailableBufferIds) {
    ffi::wgpu_server_buffer_drop(mContext, bid);
  }
  for (const auto bid : data->mQueuedBufferIds) {
    ffi::wgpu_server_buffer_drop(mContext, bid);
  }
  data->mBuffersLock.Unlock();
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShutdown() {
  mTimer.Stop();
  for (const auto& p : mCanvasMap) {
    const wr::ExternalImageId extId = {p.first};
    layers::TextureHost::DestroyRenderTexture(extId);
  }
  mCanvasMap.clear();
  ffi::wgpu_server_poll_all_devices(mContext, true);
  ffi::wgpu_server_delete(const_cast<ffi::WGPUGlobal*>(mContext));
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceAction(RawId aSelf,
                                              const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_device_action(mContext, aSelf, ToFFI(&aByteBuf),
                                 error.ToFFI());

  error.CheckAndForward(this, aSelf);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureAction(RawId aSelf, RawId aDevice,
                                               const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_texture_action(mContext, aSelf, ToFFI(&aByteBuf),
                                  error.ToFFI());

  error.CheckAndForward(this, aDevice);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderAction(
    RawId aSelf, RawId aDevice, const ipc::ByteBuf& aByteBuf) {
  ErrorBuffer error;
  ffi::wgpu_server_command_encoder_action(mContext, aSelf, ToFFI(&aByteBuf),
                                          error.ToFFI());
  error.CheckAndForward(this, aDevice);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBumpImplicitBindGroupLayout(RawId aPipelineId,
                                                             bool aIsCompute,
                                                             uint32_t aIndex,
                                                             RawId aAssignId) {
  ErrorBuffer error;
  if (aIsCompute) {
    ffi::wgpu_server_compute_pipeline_get_bind_group_layout(
        mContext, aPipelineId, aIndex, aAssignId, error.ToFFI());
  } else {
    ffi::wgpu_server_render_pipeline_get_bind_group_layout(
        mContext, aPipelineId, aIndex, aAssignId, error.ToFFI());
  }

  error.CheckAndForward(this, 0);
  return IPC_OK();
}

}  // namespace webgpu
}  // namespace mozilla
