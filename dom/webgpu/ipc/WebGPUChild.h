/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_CHILD_H_
#define WEBGPU_CHILD_H_

#include "mozilla/webgpu/PWebGPUChild.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {
struct GPURequestAdapterOptions;
}  // namespace dom
namespace layers {
class CompositorBridgeChild;
}  // namespace layers
namespace webgpu {
namespace ffi {
struct WGPUClient;
struct WGPUTextureViewDescriptor;
}  // namespace ffi

struct TextureInfo;
typedef MozPromise<RawId, Maybe<ipc::ResponseRejectReason>, true> RawIdPromise;

class WebGPUChild final : public PWebGPUChild {
 public:
  friend class layers::CompositorBridgeChild;

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(WebGPUChild)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGPUChild)

 public:
  explicit WebGPUChild();

  bool IsOpen() const { return mIPCOpen; }

  RefPtr<RawIdPromise> InstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions);
  Maybe<RawId> AdapterRequestDevice(RawId aSelfId,
                                    const dom::GPUDeviceDescriptor& aDesc);
  RawId DeviceCreateBuffer(RawId aSelfId,
                           const dom::GPUBufferDescriptor& aDesc);
  static UniquePtr<ffi::WGPUTextureViewDescriptor> GetDefaultViewDescriptor(
      const dom::GPUTextureDescriptor& aDesc);
  RawId DeviceCreateTexture(RawId aSelfId,
                            const dom::GPUTextureDescriptor& aDesc);
  RawId TextureCreateView(
      RawId aSelfId, const dom::GPUTextureViewDescriptor& aDesc,
      const ffi::WGPUTextureViewDescriptor& aDefaultViewDesc);
  RawId DeviceCreateSampler(RawId aSelfId,
                            const dom::GPUSamplerDescriptor& aDesc);
  RawId DeviceCreateCommandEncoder(
      RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc);
  RawId CommandEncoderFinish(RawId aSelfId,
                             const dom::GPUCommandBufferDescriptor& aDesc);
  RawId DeviceCreateBindGroupLayout(
      RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc);
  RawId DeviceCreatePipelineLayout(
      RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc);
  RawId DeviceCreateBindGroup(RawId aSelfId,
                              const dom::GPUBindGroupDescriptor& aDesc);
  RawId DeviceCreateShaderModule(RawId aSelfId,
                                 const dom::GPUShaderModuleDescriptor& aDesc);
  RawId DeviceCreateComputePipeline(
      RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc);
  RawId DeviceCreateRenderPipeline(
      RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc);

  void QueueSubmit(RawId aSelfId, const nsTArray<RawId>& aCommandBufferIds);

 private:
  virtual ~WebGPUChild();

  // AddIPDLReference and ReleaseIPDLReference are only to be called by
  // CompositorBridgeChild's AllocPWebGPUChild and DeallocPWebGPUChild methods
  // respectively. We intentionally make them private to prevent misuse.
  // The purpose of these methods is to be aware of when the IPC system around
  // this actor goes down: mIPCOpen is then set to false.
  void AddIPDLReference() {
    MOZ_ASSERT(!mIPCOpen);
    mIPCOpen = true;
    AddRef();
  }
  void ReleaseIPDLReference() {
    MOZ_ASSERT(mIPCOpen);
    mIPCOpen = false;
    Release();
  }

  ffi::WGPUClient* const mClient;
  bool mIPCOpen;

 public:
  ipc::IPCResult RecvFreeAdapter(RawId id);
  ipc::IPCResult RecvFreeDevice(RawId id);
  ipc::IPCResult RecvFreePipelineLayout(RawId id);
  ipc::IPCResult RecvFreeShaderModule(RawId id);
  ipc::IPCResult RecvFreeBindGroupLayout(RawId id);
  ipc::IPCResult RecvFreeBindGroup(RawId id);
  ipc::IPCResult RecvFreeCommandBuffer(RawId id);
  ipc::IPCResult RecvFreeRenderPipeline(RawId id);
  ipc::IPCResult RecvFreeComputePipeline(RawId id);
  ipc::IPCResult RecvFreeBuffer(RawId id);
  ipc::IPCResult RecvFreeTexture(RawId id);
  ipc::IPCResult RecvFreeTextureView(RawId id);
  ipc::IPCResult RecvFreeSampler(RawId id);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_CHILD_H_
