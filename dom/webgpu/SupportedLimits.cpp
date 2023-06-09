/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SupportedLimits.h"
#include "Adapter.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(SupportedLimits, mParent)
GPU_IMPL_JS_WRAP(SupportedLimits)

SupportedLimits::SupportedLimits(Adapter* const aParent,
                                 UniquePtr<ffi::WGPULimits>&& aLimits)
    : ChildOf(aParent), mLimits(std::move(aLimits)) {}

SupportedLimits::~SupportedLimits() = default;

uint32_t SupportedLimits::MaxTextureDimension1D() const {
  return mLimits->max_texture_dimension_1d;
}
uint32_t SupportedLimits::MaxTextureDimension2D() const {
  return mLimits->max_texture_dimension_2d;
}
uint32_t SupportedLimits::MaxTextureDimension3D() const {
  return mLimits->max_texture_dimension_3d;
}
uint32_t SupportedLimits::MaxTextureArrayLayers() const {
  return mLimits->max_texture_array_layers;
}
uint32_t SupportedLimits::MaxBindGroups() const {
  return mLimits->max_bind_groups;
}
uint32_t SupportedLimits::MaxDynamicUniformBuffersPerPipelineLayout() const {
  return mLimits->max_dynamic_uniform_buffers_per_pipeline_layout;
}
uint32_t SupportedLimits::MaxDynamicStorageBuffersPerPipelineLayout() const {
  return mLimits->max_dynamic_storage_buffers_per_pipeline_layout;
}
uint32_t SupportedLimits::MaxSampledTexturesPerShaderStage() const {
  return mLimits->max_sampled_textures_per_shader_stage;
}
uint32_t SupportedLimits::MaxSamplersPerShaderStage() const {
  return mLimits->max_samplers_per_shader_stage;
}
uint32_t SupportedLimits::MaxStorageBuffersPerShaderStage() const {
  return mLimits->max_storage_buffers_per_shader_stage;
}
uint32_t SupportedLimits::MaxStorageTexturesPerShaderStage() const {
  return mLimits->max_storage_textures_per_shader_stage;
}
uint32_t SupportedLimits::MaxUniformBuffersPerShaderStage() const {
  return mLimits->max_uniform_buffers_per_shader_stage;
}
uint32_t SupportedLimits::MaxUniformBufferBindingSize() const {
  return mLimits->max_uniform_buffer_binding_size;
}
uint32_t SupportedLimits::MaxStorageBufferBindingSize() const {
  return mLimits->max_storage_buffer_binding_size;
}
uint32_t SupportedLimits::MinUniformBufferOffsetAlignment() const {
  return mLimits->min_uniform_buffer_offset_alignment;
}
uint32_t SupportedLimits::MinStorageBufferOffsetAlignment() const {
  return mLimits->min_storage_buffer_offset_alignment;
}
uint32_t SupportedLimits::MaxVertexBuffers() const {
  return mLimits->max_vertex_buffers;
}
uint32_t SupportedLimits::MaxVertexAttributes() const {
  return mLimits->max_vertex_attributes;
}
uint32_t SupportedLimits::MaxVertexBufferArrayStride() const {
  return mLimits->max_vertex_buffer_array_stride;
}
uint32_t SupportedLimits::MaxInterStageShaderComponents() const {
  return mLimits->max_inter_stage_shader_components;
}
uint32_t SupportedLimits::MaxComputeWorkgroupStorageSize() const {
  return mLimits->max_compute_workgroup_storage_size;
}
uint32_t SupportedLimits::MaxComputeInvocationsPerWorkgroup() const {
  return mLimits->max_compute_invocations_per_workgroup;
}
uint32_t SupportedLimits::MaxComputeWorkgroupSizeX() const {
  return mLimits->max_compute_workgroup_size_x;
}
uint32_t SupportedLimits::MaxComputeWorkgroupSizeY() const {
  return mLimits->max_compute_workgroup_size_y;
}
uint32_t SupportedLimits::MaxComputeWorkgroupSizeZ() const {
  return mLimits->max_compute_workgroup_size_z;
}
uint32_t SupportedLimits::MaxComputeWorkgroupsPerDimension() const {
  return mLimits->max_compute_workgroups_per_dimension;
}

}  // namespace mozilla::webgpu
