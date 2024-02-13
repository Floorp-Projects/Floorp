/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderPassEncoder_H_
#define GPU_RenderPassEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class DoubleSequenceOrGPUColorDict;
struct GPURenderPassDescriptor;
template <typename T>
class Sequence;
namespace binding_detail {
template <typename T>
class AutoSequence;
}  // namespace binding_detail
}  // namespace dom
namespace webgpu {
namespace ffi {
struct WGPURenderPass;
}  // namespace ffi

class BindGroup;
class Buffer;
class CommandEncoder;
class RenderBundle;
class RenderPipeline;
class TextureView;

struct ffiWGPURenderPassDeleter {
  void operator()(ffi::WGPURenderPass*);
};

class RenderPassEncoder final : public ObjectBase,
                                public ChildOf<CommandEncoder> {
 public:
  GPU_DECL_CYCLE_COLLECTION(RenderPassEncoder)
  GPU_DECL_JS_WRAP(RenderPassEncoder)

  RenderPassEncoder(CommandEncoder* const aParent,
                    const dom::GPURenderPassDescriptor& aDesc);

 protected:
  virtual ~RenderPassEncoder();
  void Cleanup() {}

  std::unique_ptr<ffi::WGPURenderPass, ffiWGPURenderPassDeleter> mPass;
  // keep all the used objects alive while the pass is recorded
  nsTArray<RefPtr<const BindGroup>> mUsedBindGroups;
  nsTArray<RefPtr<const Buffer>> mUsedBuffers;
  nsTArray<RefPtr<const RenderPipeline>> mUsedPipelines;
  nsTArray<RefPtr<const TextureView>> mUsedTextureViews;
  nsTArray<RefPtr<const RenderBundle>> mUsedRenderBundles;

 public:
  // programmable pass encoder
  void SetBindGroup(uint32_t aSlot, const BindGroup& aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets);
  // render encoder base
  void SetPipeline(const RenderPipeline& aPipeline);
  void SetIndexBuffer(const Buffer& aBuffer,
                      const dom::GPUIndexFormat& aIndexFormat, uint64_t aOffset,
                      uint64_t aSize);
  void SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer, uint64_t aOffset,
                       uint64_t aSize);
  void Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
            uint32_t aFirstVertex, uint32_t aFirstInstance);
  void DrawIndexed(uint32_t aIndexCount, uint32_t aInstanceCount,
                   uint32_t aFirstIndex, int32_t aBaseVertex,
                   uint32_t aFirstInstance);
  void DrawIndirect(const Buffer& aIndirectBuffer, uint64_t aIndirectOffset);
  void DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                           uint64_t aIndirectOffset);
  // self
  void SetViewport(float x, float y, float width, float height, float minDepth,
                   float maxDepth);
  void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
  void SetBlendConstant(const dom::DoubleSequenceOrGPUColorDict& color);
  void SetStencilReference(uint32_t reference);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

  void ExecuteBundles(
      const dom::Sequence<OwningNonNull<RenderBundle>>& aBundles);

  void End();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_RenderPassEncoder_H_
