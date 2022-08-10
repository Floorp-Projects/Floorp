/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_DEVICE_H_
#define GPU_DEVICE_H_

#include "ObjectModel.h"
#include "nsTHashSet.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/PWebGPUTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {
struct GPUExtensions;
struct GPUFeatures;
struct GPULimits;
struct GPUExtent3DDict;

struct GPUBufferDescriptor;
struct GPUTextureDescriptor;
struct GPUSamplerDescriptor;
struct GPUBindGroupLayoutDescriptor;
struct GPUPipelineLayoutDescriptor;
struct GPUBindGroupDescriptor;
struct GPUBlendStateDescriptor;
struct GPUDepthStencilStateDescriptor;
struct GPUInputStateDescriptor;
struct GPUShaderModuleDescriptor;
struct GPUAttachmentStateDescriptor;
struct GPUComputePipelineDescriptor;
struct GPURenderBundleEncoderDescriptor;
struct GPURenderPipelineDescriptor;
struct GPUCommandEncoderDescriptor;
struct GPUCanvasConfiguration;

class EventHandlerNonNull;
class Promise;
template <typename T>
class Sequence;
class GPUBufferOrGPUTexture;
enum class GPUErrorFilter : uint8_t;
enum class GPUFeatureName : uint8_t;
class GPULogCallback;
}  // namespace dom
namespace ipc {
enum class ResponseRejectReason;
class Shmem;
}  // namespace ipc
namespace layers {
class CompositableHandle;
}  // namespace layers

namespace webgpu {
namespace ffi {
struct WGPULimits;
}
class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class Fence;
class InputState;
class PipelineLayout;
class Queue;
class RenderBundleEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class SupportedFeatures;
class SupportedLimits;
class Texture;
class WebGPUChild;

using MappingPromise =
    MozPromise<BufferMapResult, ipc::ResponseRejectReason, true>;

class Device final : public DOMEventTargetHelper, public SupportsWeakPtr {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Device, DOMEventTargetHelper)
  GPU_DECL_JS_WRAP(Device)

  const RawId mId;
  RefPtr<SupportedFeatures> mFeatures;
  RefPtr<SupportedLimits> mLimits;

  explicit Device(Adapter* const aParent, RawId aId,
                  UniquePtr<ffi::WGPULimits> aRawLimits);

  RefPtr<WebGPUChild> GetBridge();
  static JSObject* CreateExternalArrayBuffer(JSContext* aCx, size_t aOffset,
                                             size_t aSize,
                                             const ipc::Shmem& aShmem);
  already_AddRefed<Texture> InitSwapChain(
      const dom::GPUCanvasConfiguration& aDesc,
      const layers::CompositableHandle& aHandle, gfx::SurfaceFormat aFormat,
      gfx::IntSize* aDefaultSize);
  bool CheckNewWarning(const nsACString& aMessage);

  void CleanupUnregisteredInParent();

  void GenerateError(const nsCString& aMessage);

  bool IsLost() const;

 private:
  ~Device();
  void Cleanup();

  RefPtr<WebGPUChild> mBridge;
  bool mValid = true;
  nsString mLabel;
  RefPtr<dom::Promise> mLostPromise;
  RefPtr<Queue> mQueue;
  nsTHashSet<nsCString> mKnownWarnings;

 public:
  void GetLabel(nsAString& aValue) const;
  void SetLabel(const nsAString& aLabel);
  dom::Promise* GetLost(ErrorResult& aRv);
  dom::Promise* MaybeGetLost() const { return mLostPromise; }

  const RefPtr<SupportedFeatures>& Features() const { return mFeatures; }
  const RefPtr<SupportedLimits>& Limits() const { return mLimits; }
  const RefPtr<Queue>& GetQueue() const { return mQueue; }

  already_AddRefed<Buffer> CreateBuffer(const dom::GPUBufferDescriptor& aDesc,
                                        ErrorResult& aRv);

  already_AddRefed<Texture> CreateTexture(
      const dom::GPUTextureDescriptor& aDesc);
  already_AddRefed<Sampler> CreateSampler(
      const dom::GPUSamplerDescriptor& aDesc);

  already_AddRefed<CommandEncoder> CreateCommandEncoder(
      const dom::GPUCommandEncoderDescriptor& aDesc);
  already_AddRefed<RenderBundleEncoder> CreateRenderBundleEncoder(
      const dom::GPURenderBundleEncoderDescriptor& aDesc);

  already_AddRefed<BindGroupLayout> CreateBindGroupLayout(
      const dom::GPUBindGroupLayoutDescriptor& aDesc);
  already_AddRefed<PipelineLayout> CreatePipelineLayout(
      const dom::GPUPipelineLayoutDescriptor& aDesc);
  already_AddRefed<BindGroup> CreateBindGroup(
      const dom::GPUBindGroupDescriptor& aDesc);

  already_AddRefed<ShaderModule> CreateShaderModule(
      JSContext* aCx, const dom::GPUShaderModuleDescriptor& aDesc);
  already_AddRefed<ComputePipeline> CreateComputePipeline(
      const dom::GPUComputePipelineDescriptor& aDesc);
  already_AddRefed<RenderPipeline> CreateRenderPipeline(
      const dom::GPURenderPipelineDescriptor& aDesc);
  already_AddRefed<dom::Promise> CreateComputePipelineAsync(
      const dom::GPUComputePipelineDescriptor& aDesc, ErrorResult& aRv);
  already_AddRefed<dom::Promise> CreateRenderPipelineAsync(
      const dom::GPURenderPipelineDescriptor& aDesc, ErrorResult& aRv);

  void PushErrorScope(const dom::GPUErrorFilter& aFilter);
  already_AddRefed<dom::Promise> PopErrorScope(ErrorResult& aRv);

  void Destroy();

  IMPL_EVENT_HANDLER(uncapturederror)
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_DEVICE_H_
