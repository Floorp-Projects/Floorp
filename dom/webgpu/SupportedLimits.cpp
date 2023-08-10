/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SupportedLimits.h"
#include "Adapter.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(SupportedLimits, mParent)
GPU_IMPL_JS_WRAP(SupportedLimits)

SupportedLimits::SupportedLimits(Adapter* const aParent,
                                 const ffi::WGPULimits& aLimits)
    : ChildOf(aParent), mFfi(std::make_unique<ffi::WGPULimits>(aLimits)) {}

SupportedLimits::~SupportedLimits() = default;

uint64_t GetLimit(const ffi::WGPULimits& limits, const Limit limit) {
  switch (limit) {
    case Limit::MaxTextureDimension1D:
      return limits.max_texture_dimension_1d;
    case Limit::MaxTextureDimension2D:
      return limits.max_texture_dimension_2d;
    case Limit::MaxTextureDimension3D:
      return limits.max_texture_dimension_3d;
    case Limit::MaxTextureArrayLayers:
      return limits.max_texture_array_layers;
    case Limit::MaxBindGroups:
      return limits.max_bind_groups;
    case Limit::MaxBindGroupsPlusVertexBuffers:
      // Not in ffi::WGPULimits, so synthesize:
      return GetLimit(limits, Limit::MaxBindGroups) +
             GetLimit(limits, Limit::MaxVertexBuffers);
    case Limit::MaxBindingsPerBindGroup:
      return limits.max_bindings_per_bind_group;
    case Limit::MaxDynamicUniformBuffersPerPipelineLayout:
      return limits.max_dynamic_uniform_buffers_per_pipeline_layout;
    case Limit::MaxDynamicStorageBuffersPerPipelineLayout:
      return limits.max_dynamic_storage_buffers_per_pipeline_layout;
    case Limit::MaxSampledTexturesPerShaderStage:
      return limits.max_sampled_textures_per_shader_stage;
    case Limit::MaxSamplersPerShaderStage:
      return limits.max_samplers_per_shader_stage;
    case Limit::MaxStorageBuffersPerShaderStage:
      return limits.max_storage_buffers_per_shader_stage;
    case Limit::MaxStorageTexturesPerShaderStage:
      return limits.max_storage_textures_per_shader_stage;
    case Limit::MaxUniformBuffersPerShaderStage:
      return limits.max_uniform_buffers_per_shader_stage;
    case Limit::MaxUniformBufferBindingSize:
      return limits.max_uniform_buffer_binding_size;
    case Limit::MaxStorageBufferBindingSize:
      return limits.max_storage_buffer_binding_size;
    case Limit::MinUniformBufferOffsetAlignment:
      return limits.min_uniform_buffer_offset_alignment;
    case Limit::MinStorageBufferOffsetAlignment:
      return limits.min_storage_buffer_offset_alignment;
    case Limit::MaxVertexBuffers:
      return limits.max_vertex_buffers;
    case Limit::MaxBufferSize:
      return limits.max_buffer_size;
    case Limit::MaxVertexAttributes:
      return limits.max_vertex_attributes;
    case Limit::MaxVertexBufferArrayStride:
      return limits.max_vertex_buffer_array_stride;
    case Limit::MaxInterStageShaderComponents:
      return limits.max_inter_stage_shader_components;
    case Limit::MaxInterStageShaderVariables:
      return 16;  // From the spec. (not in ffi::WGPULimits)
    case Limit::MaxColorAttachments:
      return 8;  // From the spec. (not in ffi::WGPULimits)
    case Limit::MaxColorAttachmentBytesPerSample:
      return 32;  // From the spec. (not in ffi::WGPULimits)
    case Limit::MaxComputeWorkgroupStorageSize:
      return limits.max_compute_workgroup_storage_size;
    case Limit::MaxComputeInvocationsPerWorkgroup:
      return limits.max_compute_invocations_per_workgroup;
    case Limit::MaxComputeWorkgroupSizeX:
      return limits.max_compute_workgroup_size_x;
    case Limit::MaxComputeWorkgroupSizeY:
      return limits.max_compute_workgroup_size_y;
    case Limit::MaxComputeWorkgroupSizeZ:
      return limits.max_compute_workgroup_size_z;
    case Limit::MaxComputeWorkgroupsPerDimension:
      return limits.max_compute_workgroups_per_dimension;
  }
  MOZ_CRASH("Bad Limit");
}

void SetLimit(ffi::WGPULimits* const limits, const Limit limit,
              const double val) {
  const auto autoVal = LazyAssertedCast(static_cast<uint64_t>(val));
  switch (limit) {
    case Limit::MaxTextureDimension1D:
      limits->max_texture_dimension_1d = autoVal;
      return;
    case Limit::MaxTextureDimension2D:
      limits->max_texture_dimension_2d = autoVal;
      return;
    case Limit::MaxTextureDimension3D:
      limits->max_texture_dimension_3d = autoVal;
      return;
    case Limit::MaxTextureArrayLayers:
      limits->max_texture_array_layers = autoVal;
      return;
    case Limit::MaxBindGroups:
      limits->max_bind_groups = autoVal;
      return;
    case Limit::MaxBindGroupsPlusVertexBuffers:
      // Not in ffi::WGPULimits, and we're allowed to give back better
      // limits than requested.
      return;
    case Limit::MaxBindingsPerBindGroup:
      limits->max_bindings_per_bind_group = autoVal;
      return;
    case Limit::MaxDynamicUniformBuffersPerPipelineLayout:
      limits->max_dynamic_uniform_buffers_per_pipeline_layout = autoVal;
      return;
    case Limit::MaxDynamicStorageBuffersPerPipelineLayout:
      limits->max_dynamic_storage_buffers_per_pipeline_layout = autoVal;
      return;
    case Limit::MaxSampledTexturesPerShaderStage:
      limits->max_sampled_textures_per_shader_stage = autoVal;
      return;
    case Limit::MaxSamplersPerShaderStage:
      limits->max_samplers_per_shader_stage = autoVal;
      return;
    case Limit::MaxStorageBuffersPerShaderStage:
      limits->max_storage_buffers_per_shader_stage = autoVal;
      return;
    case Limit::MaxStorageTexturesPerShaderStage:
      limits->max_storage_textures_per_shader_stage = autoVal;
      return;
    case Limit::MaxUniformBuffersPerShaderStage:
      limits->max_uniform_buffers_per_shader_stage = autoVal;
      return;
    case Limit::MaxUniformBufferBindingSize:
      limits->max_uniform_buffer_binding_size = autoVal;
      return;
    case Limit::MaxStorageBufferBindingSize:
      limits->max_storage_buffer_binding_size = autoVal;
      return;
    case Limit::MinUniformBufferOffsetAlignment:
      limits->min_uniform_buffer_offset_alignment = autoVal;
      return;
    case Limit::MinStorageBufferOffsetAlignment:
      limits->min_storage_buffer_offset_alignment = autoVal;
      return;
    case Limit::MaxVertexBuffers:
      limits->max_vertex_buffers = autoVal;
      return;
    case Limit::MaxBufferSize:
      limits->max_buffer_size = autoVal;
      return;
    case Limit::MaxVertexAttributes:
      limits->max_vertex_attributes = autoVal;
      return;
    case Limit::MaxVertexBufferArrayStride:
      limits->max_vertex_buffer_array_stride = autoVal;
      return;
    case Limit::MaxInterStageShaderComponents:
      limits->max_inter_stage_shader_components = autoVal;
      return;
    case Limit::MaxInterStageShaderVariables:
      // Not in ffi::WGPULimits, and we're allowed to give back better
      // limits than requested.
      return;
    case Limit::MaxColorAttachments:
      // Not in ffi::WGPULimits, and we're allowed to give back better
      // limits than requested.
      return;
    case Limit::MaxColorAttachmentBytesPerSample:
      // Not in ffi::WGPULimits, and we're allowed to give back better
      // limits than requested.
      return;
    case Limit::MaxComputeWorkgroupStorageSize:
      limits->max_compute_workgroup_storage_size = autoVal;
      return;
    case Limit::MaxComputeInvocationsPerWorkgroup:
      limits->max_compute_invocations_per_workgroup = autoVal;
      return;
    case Limit::MaxComputeWorkgroupSizeX:
      limits->max_compute_workgroup_size_x = autoVal;
      return;
    case Limit::MaxComputeWorkgroupSizeY:
      limits->max_compute_workgroup_size_y = autoVal;
      return;
    case Limit::MaxComputeWorkgroupSizeZ:
      limits->max_compute_workgroup_size_z = autoVal;
      return;
    case Limit::MaxComputeWorkgroupsPerDimension:
      limits->max_compute_workgroups_per_dimension = autoVal;
      return;
  }
  MOZ_CRASH("Bad Limit");
}

}  // namespace mozilla::webgpu
