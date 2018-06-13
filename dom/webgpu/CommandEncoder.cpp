/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CommandEncoder.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

CommandEncoder::~CommandEncoder() = default;

already_AddRefed<CommandBuffer>
CommandEncoder::FinishEncoding() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::CopyBufferToBuffer(const Buffer& src, const uint32_t srcOffset,
                                   const Buffer& dst, const uint32_t dstOffset,
                                   const uint32_t size) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::CopyBufferToTexture() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::CopyTextureToBuffer() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::CopyTextureToTexture() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::Blit() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::TransitionBuffer(const Buffer& b, const uint32_t flags) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetPushConstants(const uint32_t stageFlags, const uint32_t offset,
                                 const uint32_t count, const dom::ArrayBuffer& data) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetBindGroup(const uint32_t index, const BindGroup& bindGroup) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetPipeline(const dom::WebGPUComputePipelineOrWebGPURenderPipeline& pipeline) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::BeginComputePass() const
{
    MOZ_CRASH("todo");
}
void
CommandEncoder::EndComputePass() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::Dispatch(const uint32_t x, const uint32_t y, const uint32_t z) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::BeginRenderPass(const dom::WebGPURenderPassDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::EndRenderPass() const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetBlendColor(const float r, const float g, const float b,
                              const float a) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetIndexBuffer(const Buffer& buffer, const uint32_t offset) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::SetVertexBuffers(const uint32_t startSlot,
                                 const dom::Sequence<OwningNonNull<Buffer>>& buffers,
                                 const dom::Sequence<uint32_t>& offsets) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::Draw(const uint32_t vertexCount, const uint32_t instanceCount,
                     const uint32_t firstVertex, const uint32_t firstInstance) const
{
    MOZ_CRASH("todo");
}

void
CommandEncoder::DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount,
                            const uint32_t firstIndex, const uint32_t firstInstance,
                            const uint32_t firstVertex) const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(CommandEncoder)

} // namespace webgpu
} // namespace mozilla
