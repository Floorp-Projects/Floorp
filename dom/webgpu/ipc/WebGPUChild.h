/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_CHILD_H_
#define WEBGPU_CHILD_H_

#include "mozilla/webgpu/PWebGPUChild.h"
#include "mozilla/MozPromise.h"
#include "mozilla/WeakPtr.h"

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

typedef MozPromise<ipc::ByteBuf, Maybe<ipc::ResponseRejectReason>, true>
    AdapterPromise;

ffi::WGPUByteBuf* ToFFI(ipc::ByteBuf* x);

class WebGPUChild final : public PWebGPUChild, public SupportsWeakPtr {
 public:
  friend class layers::CompositorBridgeChild;

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(WebGPUChild)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGPUChild)

 public:
  explicit WebGPUChild();

  bool IsOpen() const { return mIPCOpen; }

  RefPtr<AdapterPromise> InstanceRequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions);
  Maybe<RawId> AdapterRequestDevice(RawId aSelfId,
                                    const dom::GPUDeviceDescriptor& aDesc);
  RawId DeviceCreateBuffer(RawId aSelfId,
                           const dom::GPUBufferDescriptor& aDesc);
  RawId DeviceCreateTexture(RawId aSelfId,
                            const dom::GPUTextureDescriptor& aDesc);
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
  RawId DeviceCreateBindGroupLayout(
      RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc);
  RawId DeviceCreatePipelineLayout(
      RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc);
  RawId DeviceCreateBindGroup(RawId aSelfId,
                              const dom::GPUBindGroupDescriptor& aDesc);
  RawId DeviceCreateShaderModule(RawId aSelfId,
                                 const dom::GPUShaderModuleDescriptor& aDesc);
  RawId DeviceCreateComputePipeline(
      RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc,
      RawId* const aImplicitPipelineLayoutId,
      nsTArray<RawId>* const aImplicitBindGroupLayoutIds);
  RawId DeviceCreateRenderPipeline(
      RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc,
      RawId* const aImplicitPipelineLayoutId,
      nsTArray<RawId>* const aImplicitBindGroupLayoutIds);

  void DeviceCreateSwapChain(RawId aSelfId, const RGBDescriptor& aRgbDesc,
                             size_t maxBufferCount,
                             wr::ExternalImageId aExternalImageId);
  void SwapChainPresent(wr::ExternalImageId aExternalImageId, RawId aTextureId);

  void RegisterDevice(RawId aId, Device* aDevice);
  void UnregisterDevice(RawId aId);

  static void ConvertTextureFormatRef(const dom::GPUTextureFormat& aInput,
                                      ffi::WGPUTextureFormat& aOutput);

 private:
  virtual ~WebGPUChild();

  void JsWarning(nsIGlobalObject* aGlobal, const nsACString& aMessage);

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
  std::unordered_map<RawId, Device*> mDeviceMap;

 public:
  ipc::IPCResult RecvError(RawId aDeviceId, const nsACString& aMessage);
  ipc::IPCResult RecvDropAction(const ipc::ByteBuf& aByteBuf);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_CHILD_H_
