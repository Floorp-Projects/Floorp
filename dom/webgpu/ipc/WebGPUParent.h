/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_PARENT_H_
#define WEBGPU_PARENT_H_

#include <unordered_map>

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
class ExternalTexture;
class PresentationData;

// Destroy/Drop messages:
// - Messages with "Destroy" in their name request deallocation of resources
// owned by the
//   object and put the object in a destroyed state without deleting the object.
//   It is still safe to reffer to these objects.
// - Messages with "Drop" in their name can be thought of as C++ destructors.
// They completely
//   delete the object, so future attempts at accessing to these objects will
//   crash. The child process should *never* send a Drop message if it still
//   holds references to the object. An object that has been destroyed still
//   needs to be dropped when the last reference to it dies on the child
//   process.

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
  ipc::IPCResult RecvAdapterDrop(RawId aAdapterId);
  ipc::IPCResult RecvDeviceDestroy(RawId aDeviceId);
  ipc::IPCResult RecvDeviceDrop(RawId aDeviceId);
  ipc::IPCResult RecvCreateBuffer(RawId aDeviceId, RawId aBufferId,
                                  dom::GPUBufferDescriptor&& aDesc,
                                  ipc::UnsafeSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBufferMap(RawId aBufferId, uint32_t aMode,
                               uint64_t aOffset, uint64_t size,
                               BufferMapResolver&& aResolver);
  ipc::IPCResult RecvBufferUnmap(RawId aDeviceId, RawId aBufferId, bool aFlush);
  ipc::IPCResult RecvBufferDestroy(RawId aBufferId);
  ipc::IPCResult RecvBufferDrop(RawId aBufferId);
  ipc::IPCResult RecvTextureDrop(RawId aTextureId);
  ipc::IPCResult RecvTextureViewDrop(RawId aTextureViewId);
  ipc::IPCResult RecvSamplerDrop(RawId aSamplerId);
  ipc::IPCResult RecvCommandEncoderFinish(
      RawId aEncoderId, RawId aDeviceId,
      const dom::GPUCommandBufferDescriptor& aDesc);
  ipc::IPCResult RecvCommandEncoderDrop(RawId aEncoderId);
  ipc::IPCResult RecvCommandBufferDrop(RawId aCommandBufferId);
  ipc::IPCResult RecvRenderBundleDrop(RawId aBundleId);
  ipc::IPCResult RecvQueueSubmit(RawId aQueueId, RawId aDeviceId,
                                 const nsTArray<RawId>& aCommandBuffers);
  ipc::IPCResult RecvQueueOnSubmittedWorkDone(
      RawId aQueueId, std::function<void(mozilla::void_t)>&& aResolver);
  ipc::IPCResult RecvQueueWriteAction(RawId aQueueId, RawId aDeviceId,
                                      const ipc::ByteBuf& aByteBuf,
                                      ipc::UnsafeSharedMemoryHandle&& aShmem);
  ipc::IPCResult RecvBindGroupLayoutDrop(RawId aBindGroupLayoutId);
  ipc::IPCResult RecvPipelineLayoutDrop(RawId aPipelineLayoutId);
  ipc::IPCResult RecvBindGroupDrop(RawId aBindGroupId);
  ipc::IPCResult RecvShaderModuleDrop(RawId aModuleId);
  ipc::IPCResult RecvComputePipelineDrop(RawId aPipelineId);
  ipc::IPCResult RecvRenderPipelineDrop(RawId aPipelineId);
  ipc::IPCResult RecvImplicitLayoutDrop(RawId aImplicitPlId,
                                        const nsTArray<RawId>& aImplicitBglIds);
  ipc::IPCResult RecvDeviceCreateSwapChain(
      RawId aDeviceId, RawId aQueueId, const layers::RGBDescriptor& aDesc,
      const nsTArray<RawId>& aBufferIds,
      const layers::RemoteTextureOwnerId& aOwnerId,
      bool aUseExternalTextureInSwapChain);
  ipc::IPCResult RecvDeviceCreateShaderModule(
      RawId aDeviceId, RawId aModuleId, const nsString& aLabel,
      const nsCString& aCode, DeviceCreateShaderModuleResolver&& aOutMessage);

  ipc::IPCResult RecvSwapChainPresent(
      RawId aTextureId, RawId aCommandEncoderId,
      const layers::RemoteTextureId& aRemoteTextureId,
      const layers::RemoteTextureOwnerId& aOwnerId);
  ipc::IPCResult RecvSwapChainDrop(
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
    RawId mDeviceId;
  };

  BufferMapData* GetBufferMapData(RawId aBufferId);

  bool UseExternalTextureForSwapChain(ffi::WGPUSwapChainId aSwapChainId);

  bool EnsureExternalTextureForSwapChain(ffi::WGPUSwapChainId aSwapChainId,
                                         ffi::WGPUDeviceId aDeviceId,
                                         ffi::WGPUTextureId aTextureId,
                                         uint32_t aWidth, uint32_t aHeight,
                                         struct ffi::WGPUTextureFormat aFormat,
                                         ffi::WGPUTextureUsages aUsage);

  std::shared_ptr<ExternalTexture> CreateExternalTexture(
      ffi::WGPUDeviceId aDeviceId, ffi::WGPUTextureId aTextureId,
      uint32_t aWidth, uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      ffi::WGPUTextureUsages aUsage);

  std::shared_ptr<ExternalTexture> GetExternalTexture(ffi::WGPUTextureId aId);

  void PostExternalTexture(
      const std::shared_ptr<ExternalTexture>&& aExternalTexture,
      const layers::RemoteTextureId aRemoteTextureId,
      const layers::RemoteTextureOwnerId aOwnerId);

 private:
  static void MapCallback(ffi::WGPUBufferMapAsyncStatus aStatus,
                          uint8_t* aUserData);
  void DeallocBufferShmem(RawId aBufferId);

  virtual ~WebGPUParent();
  void MaintainDevices();
  void LoseDevice(const RawId aDeviceId, Maybe<uint8_t> aReason,
                  const nsACString& aMessage);

  bool ForwardError(const RawId aDeviceId, ErrorBuffer& aError) {
    return ForwardError(Some(aDeviceId), aError);
  }
  bool ForwardError(Maybe<RawId> aDeviceId, ErrorBuffer& aError);

  void ReportError(Maybe<RawId> aDeviceId, GPUErrorFilter,
                   const nsCString& message);

  static Maybe<ffi::WGPUFfiLUID> GetCompositorDeviceLuid();

  UniquePtr<ffi::WGPUGlobal> mContext;
  base::RepeatingTimer<WebGPUParent> mTimer;

  /// A map from wgpu buffer ids to data about their shared memory segments.
  /// Includes entries about mappedAtCreation, MAP_READ and MAP_WRITE buffers,
  /// regardless of their state.
  std::unordered_map<uint64_t, BufferMapData> mSharedMemoryMap;
  /// Associated presentation data for each swapchain.
  std::unordered_map<layers::RemoteTextureOwnerId, RefPtr<PresentationData>,
                     layers::RemoteTextureOwnerId::HashFn>
      mPresentationDataMap;

  RefPtr<layers::RemoteTextureOwnerClient> mRemoteTextureOwner;

  /// Associated stack of error scopes for each device.
  std::unordered_map<uint64_t, std::vector<ErrorScope>>
      mErrorScopeStackByDevice;

  std::unordered_map<ffi::WGPUTextureId, std::shared_ptr<ExternalTexture>>
      mExternalTextures;

  // Store a set of DeviceIds that have been SendDeviceLost. We use this to
  // limit each Device to one DeviceLost message.
  nsTHashSet<RawId> mLostDeviceIds;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_PARENT_H_
