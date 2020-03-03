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
  void DestroyAdapter(RawId aId);
  RawId DeviceCreateBuffer(RawId aSelfId,
                           const dom::GPUBufferDescriptor& aDesc);
  void DestroyBuffer(RawId aId);
  static UniquePtr<ffi::WGPUTextureViewDescriptor> GetDefaultViewDescriptor(
      const dom::GPUTextureDescriptor& aDesc);
  RawId DeviceCreateTexture(RawId aSelfId,
                            const dom::GPUTextureDescriptor& aDesc);
  RawId TextureCreateView(
      RawId aSelfId, const dom::GPUTextureViewDescriptor& aDesc,
      const ffi::WGPUTextureViewDescriptor& aDefaultViewDesc);
  void DestroyTexture(RawId aId);
  void DestroyTextureView(RawId aId);
  RawId DeviceCreateSampler(RawId aSelfId,
                            const dom::GPUSamplerDescriptor& aDesc);
  void DestroySampler(RawId aId);
  RawId DeviceCreateCommandEncoder(
      RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc);
  RawId CommandEncoderFinish(RawId aSelfId,
                             const dom::GPUCommandBufferDescriptor& aDesc);
  void DestroyCommandEncoder(RawId aId);
  void DestroyCommandBuffer(RawId aId);
  RawId DeviceCreateBindGroupLayout(
      RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc);
  void DestroyBindGroupLayout(RawId aId);
  RawId DeviceCreatePipelineLayout(
      RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc);
  void DestroyPipelineLayout(RawId aId);
  RawId DeviceCreateBindGroup(RawId aSelfId,
                              const dom::GPUBindGroupDescriptor& aDesc);
  void DestroyBindGroup(RawId aId);
  RawId DeviceCreateShaderModule(RawId aSelfId,
                                 const dom::GPUShaderModuleDescriptor& aDesc);
  void DestroyShaderModule(RawId aId);
  RawId DeviceCreateComputePipeline(
      RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc);
  void DestroyComputePipeline(RawId aId);

  RawId DeviceCreateRenderPipeline(
      RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc);
  void DestroyRenderPipeline(RawId aId);

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
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_CHILD_H_
