/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PARENT_H_
#define WEBGPU_PARENT_H_

#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/PWebGPUParent.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/ipc/RawShmem.h"
#include "WebGPUTypes.h"
#include "base/timer.h"

namespace mozilla {

namespace layers {
class RemoteTextureOwnerClient;
}  // namespace layers

namespace webgpu {

class ErrorBuffer;
class PresentationData;

class WebGPUParent final : public PWebGPUParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGPUParent, override)

 public:
  explicit WebGPUParent();

  ipc::IPCResult RecvInstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions,
      const nsTArray<RawId>& aTargetIds,
      InstanceRequestAdapterResolver&& resolver);
  ipc::IPCResult RecvAdapterRequestDevice(
      RawId aAdapterId, const ipc::ByteBuf& aByteBuf, RawId aDeviceId,
      AdapterRequestDeviceResolver&& resolver);
  ipc::IPCResult RecvAdapterDestroy(RawId aAdapterId);
  ipc::IPCResult RecvDeviceDestroy(RawId aDeviceId);
  ipc::IPCResult RecvCreateBuffer(RawId aDeviceId, RawId aBufferId,
                                  dom::GPUBufferDescriptor&& aDesc,
                                  ipc::UnsafeSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBufferMap(RawId aBufferId, uint32_t aMode,
                               uint64_t aOffset, uint64_t size,
                               BufferMapResolver&& aResolver);
  ipc::IPCResult RecvBufferUnmap(RawId aDeviceId, RawId aBufferId, bool aFlush);
  ipc::IPCResult RecvBufferDestroy(RawId aBufferId);
  ipc::IPCResult RecvBufferDrop(RawId aBufferId);
  ipc::IPCResult RecvTextureDestroy(RawId aTextureId);
  ipc::IPCResult RecvTextureViewDestroy(RawId aTextureViewId);
  ipc::IPCResult RecvSamplerDestroy(RawId aSamplerId);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aEncoderId, RawId aDeviceId,
      const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDestroy(RawId aEncoderId);
  ipc::IPCResult RecvCommandBufferDestroy(RawId aCommandBufferId);
  ipc::IPCResult RecvRenderBundleDestroy(RawId aBundleId);
  ipc::IPCResult RecvQueueSubmit(RawId aQueueId, RawId aDeviceId,
                                 const nsTArray<RawId>& aCommandBuffers);
  ipc::IPCResult RecvQueueWriteAction(RawId aQueueId, RawId aDeviceId,
                                      const ipc::ByteBuf& aByteBuf,
                                      ipc::UnsafeSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBindGroupLayoutDestroy(RawId aBindGroupLayoutId);
  ipc::IPCResult RecvPipelineLayoutDestroy(RawId aPipelineLayoutId);
  ipc::IPCResult RecvBindGroupDestroy(RawId aBindGroupId);
  ipc::IPCResult RecvShaderModuleDestroy(RawId aModuleId);
  ipc::IPCResult RecvComputePipelineDestroy(RawId aPipelineId);
  ipc::IPCResult RecvRenderPipelineDestroy(RawId aPipelineId);
  ipc::IPCResult RecvImplicitLayoutDestroy(
      RawId aImplicitPlId, const nsTArray<RawId>& aImplicitBglIds);
  ipc::IPCResult RecvDeviceCreateSwapChain(
      RawId aDeviceId, RawId aQueueId, const layers::RGBDescriptor& aDesc,
      const nsTArray<RawId>& aBufferIds,
      const layers::RemoteTextureOwnerId& aOwnerId);
  ipc::IPCResult RecvDeviceCreateShaderModule(
      RawId aDeviceId, RawId aModuleId, const nsString& aLabel,
      const nsCString& aCode, DeviceCreateShaderModuleResolver&& aOutMessage);

  ipc::IPCResult RecvSwapChainPresent(
      RawId aTextureId, RawId aCommandEncoderId,
      const layers::RemoteTextureId& aRemoteTextureId,
      const layers::RemoteTextureOwnerId& aOwnerId);
  ipc::IPCResult RecvSwapChainDestroy(
      const layers::RemoteTextureOwnerId& aOwnerId);

  ipc::IPCResult RecvDeviceAction(RawId aDeviceId,
                                  const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvDeviceActionWithAck(
      RawId aDeviceId, const ipc::ByteBuf& aByteBuf,
      DeviceActionWithAckResolver&& aResolver);
  ipc::IPCResult RecvTextureAction(RawId aTextureId, RawId aDevice,
                                   const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvCommandEncoderAction(RawId aEncoderId, RawId aDeviceId,
                                          const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvBumpImplicitBindGroupLayout(RawId aPipelineId,
                                                 bool aIsCompute,
                                                 uint32_t aIndex,
                                                 RawId aAssignId);

  ipc::IPCResult RecvDevicePushErrorScope(RawId aDeviceId, dom::GPUErrorFilter);
  ipc::IPCResult RecvDevicePopErrorScope(
      RawId aDeviceId, DevicePopErrorScopeResolver&& aResolver);
  ipc::IPCResult RecvGenerateError(Maybe<RawId> aDeviceId, dom::GPUErrorFilter,
                                   const nsCString& message);

  ipc::IPCResult GetFrontBufferSnapshot(
      IProtocol* aProtocol, const layers::RemoteTextureOwnerId& aOwnerId,
      Maybe<Shmem>& aShmem, gfx::IntSize& aSize);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  struct BufferMapData {
    ipc::WritableSharedMemoryMapping mShmem;
    // True if buffer's usage has MAP_READ or MAP_WRITE set.
    bool mHasMapFlags;
    uint64_t mMappedOffset;
    uint64_t mMappedSize;
  };

  BufferMapData* GetBufferMapData(RawId aBufferId);

 private:
  void DeallocBufferShmem(RawId aBufferId);

  virtual ~WebGPUParent();
  void MaintainDevices();

  bool ForwardError(const RawId aDeviceId, ErrorBuffer& aError) {
    return ForwardError(Some(aDeviceId), aError);
  }
  bool ForwardError(Maybe<RawId> aDeviceId, ErrorBuffer& aError);

  void ReportError(Maybe<RawId> aDeviceId, GPUErrorFilter,
                   const nsCString& message);

  UniquePtr<ffi::WGPUGlobal> mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;

  /// A map from wgpu buffer ids to data about their shared memory segments.
  /// Includes entries about mappedAtCreation, MAP_READ and MAP_WRITE buffers,
  /// regardless of their state.
  std::unordered_map<uint64_t, BufferMapData> mSharedMemoryMap;
  /// Associated presentation data for each swapchain.
  std::unordered_map<layers::RemoteTextureOwnerId, RefPtr<PresentationData>,
                     layers::RemoteTextureOwnerId::HashFn>
      mCanvasMap;

  RefPtr<layers::RemoteTextureOwnerClient> mRemoteTextureOwner;

  /// Associated stack of error scopes for each device.
  std::unordered_map<uint64_t, std::vector<ErrorScope>>
      mErrorScopeStackByDevice;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PARENT_H_
