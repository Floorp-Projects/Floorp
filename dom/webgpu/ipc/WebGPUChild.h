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
}  // namespace ffi

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

  void QueueSubmit(RawId aSelfId, const nsTArray<RawId>& aCommandBufferIds);

  void DestroyAdapter(RawId aId);
  void DestroyBuffer(RawId aId);
  void DestroyCommandEncoder(RawId aId);
  void DestroyCommandBuffer(RawId aId);
  void DestroyBindGroupLayout(RawId aId);
  void DestroyPipelineLayout(RawId aId);
  void DestroyBindGroup(RawId aId);
  void DestroyShaderModule(RawId aId);
  void DestroyComputePipeline(RawId aId);

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
