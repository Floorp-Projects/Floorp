/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandEncoder_H_
#define GPU_CommandEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
template <typename T>
class Sequence;
class GPUComputePipelineOrGPURenderPipeline;
class UnsignedLongSequenceOrGPUExtent3DDict;
struct GPUBufferCopyView;
struct GPUCommandBufferDescriptor;
struct GPUImageBitmapCopyView;
struct GPURenderPassDescriptor;
struct GPUTextureCopyView;
}  // namespace dom
namespace webgpu {

class BindGroup;
class Buffer;
class CommandBuffer;
class ComputePassEncoder;
class Device;
class RenderPassEncoder;

class CommandEncoder final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandEncoder)
  GPU_DECL_JS_WRAP(CommandEncoder)

  CommandEncoder(Device* const aParent, WebGPUChild* const aBridge, RawId aId);
  RawId GetId() const { return mId; }
  void EndComputePass(Span<const uint8_t> aData, ErrorResult& aRv);

  already_AddRefed<ComputePassEncoder> BeginComputePass(
      const dom::GPUComputePassDescriptor& aDesc);
  already_AddRefed<CommandBuffer> Finish(
      const dom::GPUCommandBufferDescriptor& aDesc);

 private:
  ~CommandEncoder();
  void Cleanup();

  const RefPtr<WebGPUChild> mBridge;
  const RawId mId;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandEncoder_H_
