/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandEncoder_H_
#define GPU_CommandEncoder_H_

#include "mozilla/dom/TypedArray.h"
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

 private:
  CommandEncoder() = delete;
  virtual ~CommandEncoder();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandEncoder_H_
