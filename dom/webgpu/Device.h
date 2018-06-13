/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_DEVICE_H_
#define WEBGPU_DEVICE_H_

#include "mozilla/RefPtr.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct WebGPUExtensions;
struct WebGPUFeatures;
struct WebGPULimits;

struct WebGPUBufferDescriptor;
struct WebGPUTextureDescriptor;
struct WebGPUSamplerDescriptor;
struct WebGPUBindGroupLayoutDescriptor;
struct WebGPUPipelineLayoutDescriptor;
struct WebGPUBindGroupDescriptor;
struct WebGPUBlendStateDescriptor;
struct WebGPUDepthStencilStateDescriptor;
struct WebGPUInputStateDescriptor;
struct WebGPUShaderModuleDescriptor;
struct WebGPUAttachmentStateDescriptor;
struct WebGPUComputePipelineDescriptor;
struct WebGPURenderPipelineDescriptor;
struct WebGPUCommandEncoderDescriptor;

class Promise;
class WebGPUBufferOrWebGPUTexture;
class WebGPULogCallback;
} // namespace dom

namespace webgpu {
class Adapter;
class AttachmentState;
class BindGroup;
class BindGroupLayout;
class BlendState;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class DepthStencilState;
class Fence;
class InputState;
class PipelineLayout;
class Queue;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Texture;

class Device final
    : public ChildOf<Adapter>
{
public:
    WEBGPU_DECL_GOOP(Device)

private:
    Device() = delete;
    virtual ~Device();

public:
    already_AddRefed<webgpu::Adapter> Adapter() const;

    void Extensions(dom::WebGPUExtensions& out) const;
    void Features(dom::WebGPUFeatures& out) const;
    void Limits(dom::WebGPULimits& out) const;

    already_AddRefed<Buffer> CreateBuffer(const dom::WebGPUBufferDescriptor& desc) const;
    already_AddRefed<Texture> CreateTexture(const dom::WebGPUTextureDescriptor& desc) const;
    already_AddRefed<Sampler> CreateSampler(const dom::WebGPUSamplerDescriptor& desc) const;

    already_AddRefed<BindGroupLayout> CreateBindGroupLayout(const dom::WebGPUBindGroupLayoutDescriptor& desc) const;
    already_AddRefed<PipelineLayout> CreatePipelineLayout(const dom::WebGPUPipelineLayoutDescriptor& desc) const;
    already_AddRefed<BindGroup> CreateBindGroup(const dom::WebGPUBindGroupDescriptor& desc) const;

    already_AddRefed<BlendState> CreateBlendState(const dom::WebGPUBlendStateDescriptor& desc) const;
    already_AddRefed<DepthStencilState> CreateDepthStencilState(const dom::WebGPUDepthStencilStateDescriptor& desc) const;
    already_AddRefed<InputState> CreateInputState(const dom::WebGPUInputStateDescriptor& desc) const;
    already_AddRefed<ShaderModule> CreateShaderModule(const dom::WebGPUShaderModuleDescriptor& desc) const;
    already_AddRefed<AttachmentState> CreateAttachmentState(const dom::WebGPUAttachmentStateDescriptor& desc) const;
    already_AddRefed<ComputePipeline> CreateComputePipeline(const dom::WebGPUComputePipelineDescriptor& desc) const;
    already_AddRefed<RenderPipeline> CreateRenderPipeline(const dom::WebGPURenderPipelineDescriptor& desc) const;

    already_AddRefed<CommandEncoder> CreateCommandEncoder(const dom::WebGPUCommandEncoderDescriptor& desc) const;

    already_AddRefed<Queue> GetQueue() const;

    RefPtr<dom::WebGPULogCallback> OnLog() const;
    void SetOnLog(const dom::WebGPULogCallback& callback) const;

    already_AddRefed<dom::Promise> GetObjectStatus(const dom::WebGPUBufferOrWebGPUTexture& obj) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_DEVICE_H_
