/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_DEVICE_H_
#define GPU_DEVICE_H_

#include "mozilla/RefPtr.h"

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
class GPUBufferOrGPUTexture;
enum class GPUErrorFilter : uint8_t;
class GPULogCallback;
}  // namespace dom

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

class Device final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Device, DOMEventTargetHelper)
  GPU_DECL_JS_WRAP(Device)

  explicit Device(nsIGlobalObject* aGlobal);

 private:
  Device() = delete;
  virtual ~Device();

  nsString mLabel;

 public:
  void GetLabel(nsAString& aValue) const;
  void SetLabel(const nsAString& aLabel);

  // IMPL_EVENT_HANDLER(uncapturederror)
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_DEVICE_H_
