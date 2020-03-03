/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUParent.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

const uint64_t POLL_TIME_MS = 100;

WebGPUParent::WebGPUParent() : mContext(ffi::wgpu_server_new()) {
  mTimer.Start(base::TimeDelta::FromMilliseconds(POLL_TIME_MS), this,
               &WebGPUParent::MaintainDevices);
}

WebGPUParent::~WebGPUParent() = default;

void WebGPUParent::MaintainDevices() {
  ffi::wgpu_server_poll_all_devices(mContext, false);
}

ipc::IPCResult WebGPUParent::RecvInstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions,
    const nsTArray<RawId>& aTargetIds,
    InstanceRequestAdapterResolver&& resolver) {
  ffi::WGPURequestAdapterOptions options = {};
  if (aOptions.mPowerPreference.WasPassed()) {
    options.power_preference = static_cast<ffi::WGPUPowerPreference>(
        aOptions.mPowerPreference.Value());
  }
  // TODO: make available backends configurable by prefs

  int8_t index = ffi::wgpu_server_instance_request_adapter(
      mContext, &options, aTargetIds.Elements(), aTargetIds.Length());
  if (index >= 0) {
    resolver(aTargetIds[index]);
  } else {
    resolver(0);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUDeviceDescriptor desc = {};
  desc.limits.max_bind_groups = aDesc.mLimits.WasPassed()
                                    ? aDesc.mLimits.Value().mMaxBindGroups
                                    : WGPUDEFAULT_BIND_GROUPS;
  Unused << aDesc;  // no useful fields
  // TODO: fill up the descriptor
  ffi::wgpu_server_adapter_request_device(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvAdapterDestroy(RawId aSelfId) {
  ffi::wgpu_server_adapter_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceDestroy(RawId aSelfId) {
  ffi::wgpu_server_device_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateBuffer(
    RawId aSelfId, const dom::GPUBufferDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUBufferDescriptor desc = {};
  desc.usage = aDesc.mUsage;
  desc.size = aDesc.mSize;
  // tweak: imply STORAGE_READ. This is yet to be figured out by the spec,
  // see https://github.com/gpuweb/gpuweb/issues/541
  if (desc.usage & WGPUBufferUsage_STORAGE) {
    desc.usage |= WGPUBufferUsage_STORAGE_READ;
  }
  ffi::wgpu_server_device_create_buffer(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceUnmapBuffer(RawId aSelfId,
                                                   RawId aBufferId,
                                                   Shmem&& shmem) {
  ffi::wgpu_server_device_set_buffer_sub_data(mContext, aSelfId, aBufferId, 0,
                                              shmem.get<uint8_t>(),
                                              shmem.Size<uint8_t>());
  return IPC_OK();
}

struct MapReadRequest {
  ipc::Shmem mShmem;
  WebGPUParent::BufferMapReadResolver mResolver;
  MapReadRequest(ipc::Shmem&& shmem,
                 WebGPUParent::BufferMapReadResolver&& resolver)
      : mShmem(shmem), mResolver(resolver) {}
};

void MapReadCallback(ffi::WGPUBufferMapAsyncStatus status, const uint8_t* ptr,
                     uint8_t* userdata) {
  auto req = reinterpret_cast<MapReadRequest*>(userdata);
  // TODO: better handle errors
  MOZ_ASSERT(status == ffi::WGPUBufferMapAsyncStatus_Success);
  memcpy(req->mShmem.get<uint8_t>(), ptr, req->mShmem.Size<uint8_t>());
  req->mResolver(std::move(req->mShmem));
  delete req;
}

ipc::IPCResult WebGPUParent::RecvBufferMapRead(
    RawId aSelfId, Shmem&& shmem, BufferMapReadResolver&& resolver) {
  auto size = shmem.Size<uint8_t>();
  auto request = new MapReadRequest(std::move(shmem), std::move(resolver));
  ffi::wgpu_server_buffer_map_read(mContext, aSelfId, 0, size, &MapReadCallback,
                                   reinterpret_cast<uint8_t*>(request));
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_buffer_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateTexture(
    RawId aSelfId, const ffi::WGPUTextureDescriptor& aDesc, RawId aNewId) {
  ffi::wgpu_server_device_create_texture(mContext, aSelfId, &aDesc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureCreateView(
    RawId aSelfId, const ffi::WGPUTextureViewDescriptor& aDesc, RawId aNewId) {
  ffi::wgpu_server_texture_create_view(mContext, aSelfId, &aDesc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureDestroy(RawId aSelfId) {
  ffi::wgpu_server_texture_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvTextureViewDestroy(RawId aSelfId) {
  ffi::wgpu_server_texture_view_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateSampler(
    RawId aSelfId, const ffi::WGPUSamplerDescriptor& aDesc, RawId aNewId) {
  ffi::wgpu_server_device_create_sampler(mContext, aSelfId, &aDesc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvSamplerDestroy(RawId aSelfId) {
  ffi::wgpu_server_sampler_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateCommandEncoder(
    RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc,
    RawId aNewId) {
  Unused << aDesc;  // no useful fields
  ffi::WGPUCommandEncoderDescriptor desc = {};
  ffi::wgpu_server_device_create_encoder(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderCopyBufferToBuffer(
    RawId aSelfId, RawId aSourceId, BufferAddress aSourceOffset,
    RawId aDestinationId, BufferAddress aDestinationOffset,
    BufferAddress aSize) {
  ffi::wgpu_server_encoder_copy_buffer_to_buffer(mContext, aSelfId, aSourceId,
                                                 aSourceOffset, aDestinationId,
                                                 aDestinationOffset, aSize);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderRunComputePass(RawId aSelfId,
                                                              Shmem&& shmem) {
  ffi::wgpu_server_encode_compute_pass(mContext, aSelfId, shmem.get<uint8_t>(),
                                       shmem.Size<uint8_t>());
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderRunRenderPass(RawId aSelfId,
                                                             Shmem&& shmem) {
  ffi::wgpu_server_encode_render_pass(mContext, aSelfId, shmem.get<uint8_t>(),
                                      shmem.Size<uint8_t>());
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderFinish(
    RawId aSelfId, const dom::GPUCommandBufferDescriptor& aDesc) {
  Unused << aDesc;
  ffi::WGPUCommandBufferDescriptor desc = {};
  ffi::wgpu_server_encoder_finish(mContext, aSelfId, &desc);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandEncoderDestroy(RawId aSelfId) {
  ffi::wgpu_server_encoder_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvCommandBufferDestroy(RawId aSelfId) {
  ffi::wgpu_server_command_buffer_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvQueueSubmit(
    RawId aSelfId, const nsTArray<RawId>& aCommandBuffers) {
  ffi::wgpu_server_queue_submit(mContext, aSelfId, aCommandBuffers.Elements(),
                                aCommandBuffers.Length());
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateBindGroupLayout(
    RawId aSelfId, const SerialBindGroupLayoutDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUBindGroupLayoutDescriptor desc = {};
  desc.bindings = aDesc.mBindings.Elements();
  desc.bindings_length = aDesc.mBindings.Length();
  ffi::wgpu_server_device_create_bind_group_layout(mContext, aSelfId, &desc,
                                                   aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupLayoutDestroy(RawId aSelfId) {
  ffi::wgpu_server_bind_group_layout_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreatePipelineLayout(
    RawId aSelfId, const SerialPipelineLayoutDescriptor& aDesc, RawId aNewId) {
  ffi::WGPUPipelineLayoutDescriptor desc = {};
  desc.bind_group_layouts = aDesc.mBindGroupLayouts.Elements();
  desc.bind_group_layouts_length = aDesc.mBindGroupLayouts.Length();
  ffi::wgpu_server_device_create_pipeline_layout(mContext, aSelfId, &desc,
                                                 aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvPipelineLayoutDestroy(RawId aSelfId) {
  ffi::wgpu_server_pipeline_layout_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateBindGroup(
    RawId aSelfId, const SerialBindGroupDescriptor& aDesc, RawId aNewId) {
  nsTArray<ffi::WGPUBindGroupBinding> ffiBindings(aDesc.mBindings.Length());
  for (const auto& binding : aDesc.mBindings) {
    ffi::WGPUBindGroupBinding bgb = {};
    bgb.binding = binding.mBinding;
    switch (binding.mType) {
      case SerialBindGroupBindingType::Buffer:
        bgb.resource.tag = ffi::WGPUBindingResource_Buffer;
        bgb.resource.buffer._0.buffer = binding.mValue;
        bgb.resource.buffer._0.offset = binding.mBufferOffset;
        bgb.resource.buffer._0.size = binding.mBufferSize;
        break;
      case SerialBindGroupBindingType::Texture:
        bgb.resource.tag = ffi::WGPUBindingResource_TextureView;
        bgb.resource.texture_view._0 = binding.mValue;
        break;
      case SerialBindGroupBindingType::Sampler:
        bgb.resource.tag = ffi::WGPUBindingResource_Sampler;
        bgb.resource.sampler._0 = binding.mValue;
        break;
      default:
        MOZ_CRASH("unreachable");
    }
    ffiBindings.AppendElement(bgb);
  }
  ffi::WGPUBindGroupDescriptor desc = {};
  desc.layout = aDesc.mLayout;
  desc.bindings = ffiBindings.Elements();
  desc.bindings_length = ffiBindings.Length();
  ffi::wgpu_server_device_create_bind_group(mContext, aSelfId, &desc, aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvBindGroupDestroy(RawId aSelfId) {
  ffi::wgpu_server_bind_group_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateShaderModule(
    RawId aSelfId, const nsTArray<uint32_t>& aData, RawId aNewId) {
  ffi::WGPUShaderModuleDescriptor desc = {};
  desc.code.bytes = aData.Elements();
  desc.code.length = aData.Length();
  ffi::wgpu_server_device_create_shader_module(mContext, aSelfId, &desc,
                                               aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShaderModuleDestroy(RawId aSelfId) {
  ffi::wgpu_server_shader_module_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateComputePipeline(
    RawId aSelfId, const SerialComputePipelineDescriptor& aDesc, RawId aNewId) {
  const NS_LossyConvertUTF16toASCII entryPoint(aDesc.mComputeStage.mEntryPoint);
  ffi::WGPUComputePipelineDescriptor desc = {};
  desc.layout = aDesc.mLayout;
  desc.compute_stage.module = aDesc.mComputeStage.mModule;
  desc.compute_stage.entry_point = entryPoint.get();
  ffi::wgpu_server_device_create_compute_pipeline(mContext, aSelfId, &desc,
                                                  aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvComputePipelineDestroy(RawId aSelfId) {
  ffi::wgpu_server_compute_pipeline_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvDeviceCreateRenderPipeline(
    RawId aSelfId, const SerialRenderPipelineDescriptor& aDesc, RawId aNewId) {
  const NS_LossyConvertUTF16toASCII vsEntryPoint(
      aDesc.mVertexStage.mEntryPoint);
  const NS_LossyConvertUTF16toASCII fsEntryPoint(
      aDesc.mFragmentStage.mEntryPoint);
  size_t totalAttributes = 0;
  for (const auto& vertexBuffer : aDesc.mVertexInput.mVertexBuffers) {
    totalAttributes += vertexBuffer.mAttributes.Length();
  }
  nsTArray<ffi::WGPUVertexBufferDescriptor> vertexBuffers(
      aDesc.mVertexInput.mVertexBuffers.Length());
  nsTArray<ffi::WGPUVertexAttributeDescriptor> vertexAttributes(
      totalAttributes);

  ffi::WGPURenderPipelineDescriptor desc = {};
  ffi::WGPUProgrammableStageDescriptor fragmentDesc = {};
  desc.layout = aDesc.mLayout;
  desc.vertex_stage.module = aDesc.mVertexStage.mModule;
  desc.vertex_stage.entry_point = vsEntryPoint.get();
  if (aDesc.mFragmentStage.mModule != 0) {
    fragmentDesc.module = aDesc.mFragmentStage.mModule;
    fragmentDesc.entry_point = fsEntryPoint.get();
    desc.fragment_stage = &fragmentDesc;
  }
  desc.primitive_topology = aDesc.mPrimitiveTopology;
  if (aDesc.mRasterizationState.isSome()) {
    desc.rasterization_state = aDesc.mRasterizationState.ptr();
  }
  desc.color_states = aDesc.mColorStates.Elements();
  desc.color_states_length = aDesc.mColorStates.Length();
  if (aDesc.mDepthStencilState.isSome()) {
    desc.depth_stencil_state = aDesc.mDepthStencilState.ptr();
  }
  totalAttributes = 0;
  for (const auto& vertexBuffer : aDesc.mVertexInput.mVertexBuffers) {
    ffi::WGPUVertexBufferDescriptor vb = {};
    vb.stride = vertexBuffer.mStride;
    vb.step_mode = vertexBuffer.mStepMode;
    vb.attributes = vertexAttributes.Elements() + totalAttributes;
    vb.attributes_length = vertexBuffer.mAttributes.Length();
    for (const auto& attribute : vertexBuffer.mAttributes) {
      vertexAttributes.AppendElement(attribute);
    }
    vertexBuffers.AppendElement(vb);
  }
  desc.vertex_input.index_format = aDesc.mVertexInput.mIndexFormat;
  desc.vertex_input.vertex_buffers = vertexBuffers.Elements();
  desc.vertex_input.vertex_buffers_length = vertexBuffers.Length();
  desc.sample_count = aDesc.mSampleCount;
  desc.sample_mask = aDesc.mSampleMask;
  desc.alpha_to_coverage_enabled = aDesc.mAlphaToCoverageEnabled;
  ffi::wgpu_server_device_create_render_pipeline(mContext, aSelfId, &desc,
                                                 aNewId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvRenderPipelineDestroy(RawId aSelfId) {
  ffi::wgpu_server_render_pipeline_destroy(mContext, aSelfId);
  return IPC_OK();
}

ipc::IPCResult WebGPUParent::RecvShutdown() {
  mTimer.Stop();
  ffi::wgpu_server_poll_all_devices(mContext, true);
  ffi::wgpu_server_delete(const_cast<ffi::WGPUGlobal*>(mContext));
  return IPC_OK();
}

}  // namespace webgpu
}  // namespace mozilla
