/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ComputePassEncoder_H_
#define GPU_ComputePassEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class BindGroup;
class Buffer;
class CommandEncoder;
class ComputePipeline;

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

  ffi::WGPURawPass mRaw;
  // keep all the used objects alive while the pass is recorded
  nsTArray<RefPtr<const BindGroup>> mUsedBindGroups;
  nsTArray<RefPtr<const ComputePipeline>> mUsedPipelines;

 public:
  void SetBindGroup(uint32_t aSlot, const BindGroup& aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets);
  void SetPipeline(const ComputePipeline& aPipeline);
  void Dispatch(uint32_t x, uint32_t y, uint32_t z);
  void DispatchIndirect(const Buffer& aIndirectBuffer,
                        uint64_t aIndirectOffset);
  void EndPass(ErrorResult& aRv);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ComputePassEncoder_H_
