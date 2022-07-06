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

namespace mozilla::webgpu {
class ErrorBuffer;
class PresentationData;

struct ErrorScopeStack {
  nsTArray<MaybeScopedError> mStack;
};

class WebGPUParent final : public PWebGPUParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGPUParent, override)

 public:
  explicit WebGPUParent();

  ipc::IPCResult RecvInstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions,
      const nsTArray<RawId>& aTargetIds,
      InstanceRequestAdapterResolver&& resolver);
  ipc::IPCResult RecvAdapterRequestDevice(
      RawId aSelfId, const ipc::ByteBuf& aByteBuf, RawId aNewId,
      AdapterRequestDeviceResolver&& resolver);
  ipc::IPCResult RecvAdapterDestroy(RawId aSelfId);
  ipc::IPCResult RecvDeviceDestroy(RawId aSelfId);
  ipc::IPCResult RecvCreateBuffer(RawId aSelfId, RawId aBufferId,
                                  dom::GPUBufferDescriptor&& aDesc);
  ipc::IPCResult RecvBufferReturnShmem(RawId aSelfId, Shmem&& aShmem);
  ipc::IPCResult RecvBufferMap(RawId aSelfId, ffi::WGPUHostMap aHostMap,
                               uint64_t aOffset, uint64_t size,
                               BufferMapResolver&& aResolver);
  ipc::IPCResult RecvBufferUnmap(RawId aSelfId, Shmem&& aShmem, bool aFlush,
                                 bool aKeepShmem);
  ipc::IPCResult RecvBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvTextureDestroy(RawId aSelfId);
  ipc::IPCResult RecvTextureViewDestroy(RawId aSelfId);
  ipc::IPCResult RecvSamplerDestroy(RawId aSelfId);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aSelfId, RawId aDeviceId,
      const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDestroy(RawId aSelfId);
  ipc::IPCResult RecvCommandBufferDestroy(RawId aSelfId);
  ipc::IPCResult RecvRenderBundleDestroy(RawId aSelfId);
  ipc::IPCResult RecvQueueSubmit(RawId aSelfId, RawId aDeviceId,
                                 const nsTArray<RawId>& aCommandBuffers);
  ipc::IPCResult RecvQueueWriteAction(RawId aSelfId, RawId aDeviceId,
                                      const ipc::ByteBuf& aByteBuf,
                                      Shmem&& aShmem);
  ipc::IPCResult RecvBindGroupLayoutDestroy(RawId aSelfId);
  ipc::IPCResult RecvPipelineLayoutDestroy(RawId aSelfId);
  ipc::IPCResult RecvBindGroupDestroy(RawId aSelfId);
  ipc::IPCResult RecvShaderModuleDestroy(RawId aSelfId);
  ipc::IPCResult RecvComputePipelineDestroy(RawId aSelfId);
  ipc::IPCResult RecvRenderPipelineDestroy(RawId aSelfId);
  ipc::IPCResult RecvImplicitLayoutDestroy(
      RawId aImplicitPlId, const nsTArray<RawId>& aImplicitBglIds);
  ipc::IPCResult RecvDeviceCreateSwapChain(RawId aSelfId, RawId aQueueId,
                                           const layers::RGBDescriptor& aDesc,
                                           const nsTArray<RawId>& aBufferIds,
                                           const CompositableHandle& aHandle);
  ipc::IPCResult RecvSwapChainPresent(const CompositableHandle& aHandle,
                                      RawId aTextureId,
                                      RawId aCommandEncoderId);
  ipc::IPCResult RecvSwapChainDestroy(const CompositableHandle& aHandle);

  ipc::IPCResult RecvDeviceAction(RawId aSelf, const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvDeviceActionWithAck(
      RawId aSelf, const ipc::ByteBuf& aByteBuf,
      DeviceActionWithAckResolver&& aResolver);
  ipc::IPCResult RecvTextureAction(RawId aSelf, RawId aDevice,
                                   const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvCommandEncoderAction(RawId aSelf, RawId aDevice,
                                          const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvBumpImplicitBindGroupLayout(RawId aPipelineId,
                                                 bool aIsCompute,
                                                 uint32_t aIndex,
                                                 RawId aAssignId);

  ipc::IPCResult RecvDevicePushErrorScope(RawId aSelfId);
  ipc::IPCResult RecvDevicePopErrorScope(
      RawId aSelfId, DevicePopErrorScopeResolver&& aResolver);
  ipc::IPCResult RecvGenerateError(RawId aDeviceId, const nsCString& message);

  ipc::IPCResult GetFrontBufferSnapshot(IProtocol* aProtocol,
                                        const CompositableHandle& aHandle,
                                        Maybe<Shmem>& aShmem,
                                        gfx::IntSize& aSize);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~WebGPUParent();
  void MaintainDevices();
  bool ForwardError(RawId aDeviceID, ErrorBuffer& aError);
  void ReportError(RawId aDeviceId, const nsCString& message);

  UniquePtr<ffi::WGPUGlobal> mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;
  /// Shmem associated with a mappable buffer has to be owned by one of the
  /// processes. We keep it here for every mappable buffer while the buffer is
  /// used by GPU.
  std::unordered_map<uint64_t, Shmem> mSharedMemoryMap;
  /// Associated presentation data for each swapchain.
  std::unordered_map<uint64_t, RefPtr<PresentationData>> mCanvasMap;
  /// Associated stack of error scopes for each device.
  std::unordered_map<uint64_t, ErrorScopeStack> mErrorScopeMap;
};

}  // namespace mozilla::webgpu

#endif  // WEBGPU_PARENT_H_
