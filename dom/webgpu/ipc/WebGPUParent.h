/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PARENT_H_
#define WEBGPU_PARENT_H_

#include "mozilla/webgpu/PWebGPUParent.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "WebGPUTypes.h"
#include "base/timer.h"

namespace mozilla {
namespace webgpu {
class PresentationData;

class WebGPUParent final : public PWebGPUParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGPUParent)

 public:
  explicit WebGPUParent();

  ipc::IPCResult RecvInstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions,
      const nsTArray<RawId>& aTargetIds,
      InstanceRequestAdapterResolver&& resolver);
  ipc::IPCResult RecvAdapterRequestDevice(RawId aSelfId,
                                          const dom::GPUDeviceDescriptor& aDesc,
                                          RawId aNewId);
  ipc::IPCResult RecvAdapterDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateBuffer(RawId aSelfId,
                                        const dom::GPUBufferDescriptor& aDesc,
                                        RawId aNewId);
  ipc::IPCResult RecvDeviceUnmapBuffer(RawId aSelfId, RawId aBufferId,
                                       Shmem&& aShmem, bool aFlush);
  ipc::IPCResult RecvBufferMapRead(RawId aSelfId, Shmem&& aShmem,
                                   BufferMapReadResolver&& aResolver);
  ipc::IPCResult RecvBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateTexture(RawId aSelfId,
                                         const SerialTextureDescriptor& aDesc,
                                         RawId aNewId);
  ipc::IPCResult RecvTextureCreateView(
      RawId aSelfId, const ffi::WGPUTextureViewDescriptor& aDesc, RawId aNewId);
  ipc::IPCResult RecvTextureDestroy(RawId aSelfId);
  ipc::IPCResult RecvTextureViewDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateSampler(
      RawId aSelfId, const ffi::WGPUSamplerDescriptor& aDesc, RawId aNewId);
  ipc::IPCResult RecvSamplerDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateCommandEncoder(
      RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc,
      RawId aNewId);
  ipc::IPCResult RecvCommandEncoderCopyBufferToBuffer(
      RawId aSelfId, RawId aSourceId, BufferAddress aSourceOffset,
      RawId aDestinationId, BufferAddress aDestinationOffset,
      BufferAddress aSize);
  ipc::IPCResult RecvCommandEncoderCopyBufferToTexture(
      RawId aSelfId, WGPUBufferCopyView aSource,
      WGPUTextureCopyView aDestination, WGPUExtent3d aCopySize);
  ipc::IPCResult RecvCommandEncoderCopyTextureToBuffer(
      RawId aSelfId, WGPUTextureCopyView aSource,
      WGPUBufferCopyView aDestination, WGPUExtent3d aCopySize);
  ipc::IPCResult RecvCommandEncoderCopyTextureToTexture(
      RawId aSelfId, WGPUTextureCopyView aSource,
      WGPUTextureCopyView aDestination, WGPUExtent3d aCopySize);
  ipc::IPCResult RecvCommandEncoderRunComputePass(RawId aSelfId, Shmem&& shmem);
  ipc::IPCResult RecvCommandEncoderRunRenderPass(RawId aSelfId, Shmem&& shmem);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aSelfId, const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDestroy(RawId aSelfId);
  ipc::IPCResult RecvCommandBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvQueueSubmit(RawId aSelfId,
                                 const nsTArray<RawId>& aCommandBuffers);
  ipc::IPCResult RecvDeviceCreateBindGroupLayout(
      RawId aSelfId, const SerialBindGroupLayoutDescriptor& aDesc,
      RawId aNewId);
  ipc::IPCResult RecvBindGroupLayoutDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreatePipelineLayout(
      RawId aSelfId, const SerialPipelineLayoutDescriptor& aDesc, RawId aNewId);
  ipc::IPCResult RecvPipelineLayoutDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateBindGroup(
      RawId aSelfId, const SerialBindGroupDescriptor& aDesc, RawId aNewId);
  ipc::IPCResult RecvBindGroupDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateShaderModule(RawId aSelfId,
                                              const nsTArray<uint32_t>& aData,
                                              RawId aNewId);
  ipc::IPCResult RecvShaderModuleDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateComputePipeline(
      RawId aSelfId, const SerialComputePipelineDescriptor& aDesc,
      RawId aNewId);
  ipc::IPCResult RecvComputePipelineDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateRenderPipeline(
      RawId aSelfId, const SerialRenderPipelineDescriptor& aDesc, RawId aNewId);
  ipc::IPCResult RecvRenderPipelineDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceCreateSwapChain(RawId aSelfId, RawId aQueueId,
                                           const layers::RGBDescriptor& aDesc,
                                           const nsTArray<RawId>& aBufferIds,
                                           ExternalImageId aExternalId);
  ipc::IPCResult RecvSwapChainPresent(wr::ExternalImageId aExternalId,
                                      RawId aTextureId,
                                      RawId aCommandEncoderId);
  ipc::IPCResult RecvSwapChainDestroy(wr::ExternalImageId aExternalId);
  ipc::IPCResult RecvShutdown();

 private:
  virtual ~WebGPUParent();
  void MaintainDevices();

  const ffi::WGPUGlobal_IdentityRecyclerFactory* const mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;
  std::unordered_map<uint64_t, RefPtr<PresentationData>> mCanvasMap;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PARENT_H_
