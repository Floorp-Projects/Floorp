/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ComputePassEncoder_H_
#define GPU_ComputePassEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUComputePassDescriptor;
}

namespace webgpu {
namespace ffi {
struct WGPUComputePass;
}  // namespace ffi

class BindGroup;
class Buffer;
class CommandEncoder;
class ComputePipeline;

struct ffiWGPUComputePassDeleter {
  void operator()(ffi::WGPUComputePass*);
};

class ComputePassEncoder final : public ObjectBase,
                                 public ChildOf<CommandEncoder> {
 public:
  GPU_DECL_CYCLE_COLLECTION(ComputePassEncoder)
  GPU_DECL_JS_WRAP(ComputePassEncoder)

  ComputePassEncoder(CommandEncoder* const aParent,
                     const dom::GPUComputePassDescriptor& aDesc);

 private:
  virtual ~ComputePassEncoder();
  void Cleanup() {}

  std::unique_ptr<ffi::WGPUComputePass, ffiWGPUComputePassDeleter> mPass;
  // keep all the used objects alive while the pass is recorded
  nsTArray<RefPtr<const BindGroup>> mUsedBindGroups;
  nsTArray<RefPtr<const ComputePipeline>> mUsedPipelines;

 public:
  // programmable pass encoder
  void SetBindGroup(uint32_t aSlot, const BindGroup& aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets);
  // self
  void SetPipeline(const ComputePipeline& aPipeline);

  void DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY,
                          uint32_t workgroupCountZ);
  void DispatchWorkgroupsIndirect(const Buffer& aIndirectBuffer,
                                  uint64_t aIndirectOffset);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

  void End();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ComputePassEncoder_H_
