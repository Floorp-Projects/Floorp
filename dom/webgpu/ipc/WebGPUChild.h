/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_CHILD_H_
#define WEBGPU_CHILD_H_

#include "mozilla/webgpu/PWebGPUChild.h"
#include "mozilla/MozPromise.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace ipc {
class UnsafeSharedMemoryHandle;
}  // namespace ipc
namespace dom {
struct GPURequestAdapterOptions;
}  // namespace dom
namespace layers {
class CompositorBridgeChild;
}  // namespace layers
namespace webgpu {
namespace ffi {
struct WGPUClient;
struct WGPULimits;
struct WGPUTextureViewDescriptor;
}  // namespace ffi

using AdapterPromise =
    MozPromise<ipc::ByteBuf, Maybe<ipc::ResponseRejectReason>, true>;
using PipelinePromise = MozPromise<RawId, ipc::ResponseRejectReason, true>;
using DevicePromise = MozPromise<bool, ipc::ResponseRejectReason, true>;

struct PipelineCreationContext {
  RawId mParentId = 0;
  RawId mImplicitPipelineLayoutId = 0;
  nsTArray<RawId> mImplicitBindGroupLayoutIds;
};

struct DeviceRequest {
  RawId mId = 0;
  RefPtr<DevicePromise> mPromise;
  // Note: we could put `ffi::WGPULimits` in here as well,
  //  but we don't want to #include ffi stuff in this header
};

ffi::WGPUByteBuf* ToFFI(ipc::ByteBuf* x);

class WebGPUChild final : public PWebGPUChild, public SupportsWeakPtr {
 public:
  friend class layers::CompositorBridgeChild;

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(WebGPUChild)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING_INHERITED(WebGPUChild)

 public:
  explicit WebGPUChild();

  bool IsOpen() const { return CanSend(); }

  RefPtr<AdapterPromise> InstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions);
  Maybe<DeviceRequest> AdapterRequestDevice(RawId aSelfId,
                                            const ffi::WGPUDeviceDescriptor&);
  RawId DeviceCreateBuffer(RawId aSelfId, const dom::GPUBufferDescriptor& aDesc,
                           ipc::UnsafeSharedMemoryHandle&& aShmem);
  RawId DeviceCreateTexture(RawId aSelfId,
                            const dom::GPUTextureDescriptor& aDesc,
                            Maybe<layers::RemoteTextureOwnerId> aOwnerId);
  RawId TextureCreateView(RawId aSelfId, RawId aDeviceId,
                          const dom::GPUTextureViewDescriptor& aDesc);
  RawId DeviceCreateSampler(RawId aSelfId,
                            const dom::GPUSamplerDescriptor& aDesc);
  RawId DeviceCreateCommandEncoder(
      RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc);
  RawId CommandEncoderFinish(RawId aSelfId, RawId aDeviceId,
                             const dom::GPUCommandBufferDescriptor& aDesc);
  RawId RenderBundleEncoderFinish(ffi::WGPURenderBundleEncoder& aEncoder,
                                  RawId aDeviceId,
                                  const dom::GPURenderBundleDescriptor& aDesc);
  RawId RenderBundleEncoderFinishError(RawId aDeviceId, const nsString& aLabel);
  RawId DeviceCreateBindGroupLayout(
      RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc);
  RawId DeviceCreatePipelineLayout(
      RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc);
  RawId DeviceCreateBindGroup(RawId aSelfId,
                              const dom::GPUBindGroupDescriptor& aDesc);
  RawId DeviceCreateComputePipeline(
      PipelineCreationContext* const aContext,
      const dom::GPUComputePipelineDescriptor& aDesc);
  RefPtr<PipelinePromise> DeviceCreateComputePipelineAsync(
      PipelineCreationContext* const aContext,
      const dom::GPUComputePipelineDescriptor& aDesc);
  RawId DeviceCreateRenderPipeline(
      PipelineCreationContext* const aContext,
      const dom::GPURenderPipelineDescriptor& aDesc);
  RefPtr<PipelinePromise> DeviceCreateRenderPipelineAsync(
      PipelineCreationContext* const aContext,
      const dom::GPURenderPipelineDescriptor& aDesc);
  MOZ_CAN_RUN_SCRIPT already_AddRefed<ShaderModule> DeviceCreateShaderModule(
      const RefPtr<Device>& aDevice,
      const dom::GPUShaderModuleDescriptor& aDesc,
      RefPtr<dom::Promise> aPromise);

  ffi::WGPUClient* GetClient() const { return mClient.get(); }

  void DeviceCreateSwapChain(RawId aSelfId, const RGBDescriptor& aRgbDesc,
                             size_t maxBufferCount,
                             const layers::RemoteTextureOwnerId& aOwnerId,
                             bool aUseExternalTextureInSwapChain);

  void QueueOnSubmittedWorkDone(const RawId aSelfId,
                                const RefPtr<dom::Promise>& aPromise);

  void SwapChainPresent(RawId aTextureId,
                        const RemoteTextureId& aRemoteTextureId,
                        const RemoteTextureOwnerId& aOwnerId);

  void RegisterDevice(Device* const aDevice);
  void UnregisterDevice(RawId aId);
  void FreeUnregisteredInParentDevice(RawId aId);

  static ffi::WGPUTextureFormat ConvertTextureFormat(
      const dom::GPUTextureFormat& aInput);

  static void JsWarning(nsIGlobalObject* aGlobal, const nsACString& aMessage);

 private:
  virtual ~WebGPUChild();

  RawId DeviceCreateComputePipelineImpl(
      PipelineCreationContext* const aContext,
      const dom::GPUComputePipelineDescriptor& aDesc,
      ipc::ByteBuf* const aByteBuf);
  RawId DeviceCreateRenderPipelineImpl(
      PipelineCreationContext* const aContext,
      const dom::GPURenderPipelineDescriptor& aDesc,
      ipc::ByteBuf* const aByteBuf);

  UniquePtr<ffi::WGPUClient> const mClient;
  std::unordered_map<RawId, WeakPtr<Device>> mDeviceMap;

 public:
  ipc::IPCResult RecvUncapturedError(Maybe<RawId> aDeviceId,
                                     const nsACString& aMessage);
  ipc::IPCResult RecvDropAction(const ipc::ByteBuf& aByteBuf);
  ipc::IPCResult RecvDeviceLost(RawId aDeviceId, Maybe<uint8_t> aReason,
                                const nsACString& aMessage);
  void ActorDestroy(ActorDestroyReason) override;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_CHILD_H_
