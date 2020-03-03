/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_DEVICE_H_
#define GPU_DEVICE_H_

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {
struct GPUExtensions;
struct GPUFeatures;
struct GPULimits;

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

class EventHandlerNonNull;
class Promise;
template <typename T>
class Sequence;
class GPUBufferOrGPUTexture;
enum class GPUErrorFilter : uint8_t;
class GPULogCallback;
}  // namespace dom
namespace ipc {
enum class ResponseRejectReason;
class Shmem;
}  // namespace ipc

namespace webgpu {
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
class Texture;
class WebGPUChild;

typedef MozPromise<ipc::Shmem, ipc::ResponseRejectReason, true> MappingPromise;

class Device final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Device, DOMEventTargetHelper)
  GPU_DECL_JS_WRAP(Device)

  explicit Device(Adapter* const aParent, RawId aId);

  static JSObject* CreateExternalArrayBuffer(JSContext* aCx, size_t aSize,
                                             ipc::Shmem& aShmem);
  RefPtr<MappingPromise> MapBufferForReadAsync(RawId aId, size_t aSize,
                                               ErrorResult& aRv);
  void UnmapBuffer(RawId aId, UniquePtr<ipc::Shmem> aShmem);

  const RefPtr<WebGPUChild> mBridge;

 private:
  ~Device();
  void Cleanup();

  const RawId mId;
  bool mValid = true;
  nsString mLabel;
  const RefPtr<Queue> mQueue;

 public:
  void GetLabel(nsAString& aValue) const;
  void SetLabel(const nsAString& aLabel);

  Queue* DefaultQueue() const;

  already_AddRefed<Buffer> CreateBuffer(const dom::GPUBufferDescriptor& aDesc);
  void CreateBufferMapped(JSContext* aCx, const dom::GPUBufferDescriptor& aDesc,
                          nsTArray<JS::Value>& aSequence, ErrorResult& aRv);

  already_AddRefed<Texture> CreateTexture(
      const dom::GPUTextureDescriptor& aDesc);
  already_AddRefed<Sampler> CreateSampler(
      const dom::GPUSamplerDescriptor& aDesc);

  already_AddRefed<CommandEncoder> CreateCommandEncoder(
      const dom::GPUCommandEncoderDescriptor& aDesc);

  already_AddRefed<BindGroupLayout> CreateBindGroupLayout(
      const dom::GPUBindGroupLayoutDescriptor& aDesc);
  already_AddRefed<PipelineLayout> CreatePipelineLayout(
      const dom::GPUPipelineLayoutDescriptor& aDesc);
  already_AddRefed<BindGroup> CreateBindGroup(
      const dom::GPUBindGroupDescriptor& aDesc);

  already_AddRefed<ShaderModule> CreateShaderModule(
      const dom::GPUShaderModuleDescriptor& aDesc);
  already_AddRefed<ComputePipeline> CreateComputePipeline(
      const dom::GPUComputePipelineDescriptor& aDesc);
  already_AddRefed<RenderPipeline> CreateRenderPipeline(
      const dom::GPURenderPipelineDescriptor& aDesc);

  // IMPL_EVENT_HANDLER(uncapturederror)
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_DEVICE_H_
