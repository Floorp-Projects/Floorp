/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Device.h"

#include "Adapter.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

Device::~Device() = default;

already_AddRefed<webgpu::Adapter>
Device::Adapter() const
{
    MOZ_CRASH("todo");
}

void
Device::Extensions(dom::WebGPUExtensions& out) const
{
    MOZ_CRASH("todo");
}

void
Device::Features(dom::WebGPUFeatures& out) const
{
    MOZ_CRASH("todo");
}

void
Device::Limits(dom::WebGPULimits& out) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Buffer>
Device::CreateBuffer(const dom::WebGPUBufferDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Texture>
Device::CreateTexture(const dom::WebGPUTextureDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Sampler>
Device::CreateSampler(const dom::WebGPUSamplerDescriptor& desc) const
{
    MOZ_CRASH("todo");
}


already_AddRefed<BindGroupLayout>
Device::CreateBindGroupLayout(const dom::WebGPUBindGroupLayoutDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<PipelineLayout>
Device::CreatePipelineLayout(const dom::WebGPUPipelineLayoutDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<BindGroup>
Device::CreateBindGroup(const dom::WebGPUBindGroupDescriptor& desc) const
{
    MOZ_CRASH("todo");
}


already_AddRefed<BlendState>
Device::CreateBlendState(const dom::WebGPUBlendStateDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<DepthStencilState>
Device::CreateDepthStencilState(const dom::WebGPUDepthStencilStateDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<InputState>
Device::CreateInputState(const dom::WebGPUInputStateDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<ShaderModule>
Device::CreateShaderModule(const dom::WebGPUShaderModuleDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<AttachmentState>
Device::CreateAttachmentState(const dom::WebGPUAttachmentStateDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<ComputePipeline>
Device::CreateComputePipeline(const dom::WebGPUComputePipelineDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<RenderPipeline>
Device::CreateRenderPipeline(const dom::WebGPURenderPipelineDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<CommandEncoder>
Device::CreateCommandEncoder(const dom::WebGPUCommandEncoderDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Queue>
Device::GetQueue() const
{
    MOZ_CRASH("todo");
}

RefPtr<dom::WebGPULogCallback>
Device::OnLog() const
{
    MOZ_CRASH("todo");
}

void
Device::SetOnLog(const dom::WebGPULogCallback& callback) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<dom::Promise>
Device::GetObjectStatus(const dom::WebGPUBufferOrWebGPUTexture& obj) const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(Device)

} // namespace webgpu
} // namespace mozilla
