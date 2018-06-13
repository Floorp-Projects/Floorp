/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_CommandEncoder_H_
#define WEBGPU_CommandEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
template<typename T> class Sequence;
class WebGPUComputePipelineOrWebGPURenderPipeline;
struct WebGPURenderPassDescriptor;
} // namespace dom
namespace webgpu {

class BindGroup;
class Buffer;
class CommandBuffer;
class Device;

class CommandEncoder final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(CommandEncoder)

private:
    CommandEncoder() = delete;
    virtual ~CommandEncoder();

public:
    already_AddRefed<CommandBuffer> FinishEncoding() const;

    // Commands allowed outside of "passes"
    void CopyBufferToBuffer(const Buffer& src, uint32_t srcOffset, const Buffer& dst,
                            uint32_t dstOffset, uint32_t size) const;
    // TODO figure out all the arguments required for these
    void CopyBufferToTexture() const;
    void CopyTextureToBuffer() const;
    void CopyTextureToTexture() const;
    void Blit() const;

    void TransitionBuffer(const Buffer& b, uint32_t flags) const;

    // Allowed in both compute and render passes
    void SetPushConstants(uint32_t stageFlags,
                          uint32_t offset,
                          uint32_t count,
                          const dom::ArrayBuffer& data) const;
    void SetBindGroup(uint32_t index, const BindGroup& bindGroup) const;
    void SetPipeline(const dom::WebGPUComputePipelineOrWebGPURenderPipeline& pipeline) const;

    // Compute pass commands
    void BeginComputePass() const;
    void EndComputePass() const;

    void Dispatch(uint32_t x, uint32_t y, uint32_t z) const;

    // Render pass commands
    void BeginRenderPass(const dom::WebGPURenderPassDescriptor& desc) const;
    void EndRenderPass() const;

    void SetBlendColor(float r, float g, float b, float a) const;
    void SetIndexBuffer(const Buffer& buffer, uint32_t offset) const;
    void SetVertexBuffers(uint32_t startSlot,
                          const dom::Sequence<OwningNonNull<Buffer>>& buffers,
                          const dom::Sequence<uint32_t>& offsets) const;

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
              uint32_t firstInstance) const;
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                     uint32_t firstInstance, uint32_t firstVertex) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_CommandEncoder_H_
