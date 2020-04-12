/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_SERIALIZE_H_
#define WEBGPU_SERIALIZE_H_

#include "WebGPUTypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace IPC {

#define DEFINE_IPC_SERIALIZER_ENUM_GUARD(something, guard) \
  template <>                                              \
  struct ParamTraits<something>                            \
      : public ContiguousEnumSerializer<something, something(0), guard> {}

#define DEFINE_IPC_SERIALIZER_DOM_ENUM(something) \
  DEFINE_IPC_SERIALIZER_ENUM_GUARD(something, something::EndGuard_)
#define DEFINE_IPC_SERIALIZER_FFI_ENUM(something) \
  DEFINE_IPC_SERIALIZER_ENUM_GUARD(something, something##_Sentinel)

DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUPowerPreference);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUAddressMode);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUCompareFunction);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUFilterMode);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::webgpu::SerialBindGroupEntryType);

DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUBindingType);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUBlendFactor);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUBlendOperation);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUCompareFunction);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUCullMode);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUFrontFace);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUIndexFormat);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUInputStepMode);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUPrimitiveTopology);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUStencilOperation);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureAspect);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureComponentType);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureDimension);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureFormat);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureViewDimension);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUVertexFormat);

DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandEncoderDescriptor);
DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandBufferDescriptor);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPURequestAdapterOptions,
                                  mPowerPreference);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUExtensions,
                                  mAnisotropicFiltering);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPULimits, mMaxBindGroups);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUDeviceDescriptor,
                                  mExtensions, mLimits);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUBufferDescriptor, mSize,
                                  mUsage);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUSamplerDescriptor,
                                  mAddressModeU, mAddressModeV, mAddressModeW,
                                  mMagFilter, mMinFilter, mMipmapFilter,
                                  mLodMinClamp, mLodMaxClamp, mCompare);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUExtent3d, width,
                                  height, depth);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUTextureDescriptor,
                                  size, array_layer_count, mip_level_count,
                                  sample_count, dimension, format, usage);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUTextureViewDescriptor, format, dimension, aspect,
    base_mip_level, level_count, base_array_layer, array_layer_count);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUBlendDescriptor,
                                  src_factor, dst_factor, operation);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPURasterizationStateDescriptor, front_face,
    cull_mode, depth_bias, depth_bias_slope_scale, depth_bias_clamp);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUColorStateDescriptor, format, alpha_blend,
    color_blend, write_mask);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUStencilStateFaceDescriptor, compare, fail_op,
    depth_fail_op, pass_op);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUDepthStencilStateDescriptor, format,
    depth_write_enabled, depth_compare, stencil_front, stencil_back,
    stencil_read_mask, stencil_write_mask);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUVertexAttributeDescriptor, offset, format,
    shader_location);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUBindGroupLayoutEntry, binding, visibility, ty,
    multisampled, has_dynamic_offset, view_dimension, texture_component_type,
    storage_texture_format);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialBindGroupLayoutDescriptor, mEntries);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialPipelineLayoutDescriptor, mBindGroupLayouts);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::SerialBindGroupEntry,
                                  mBinding, mType, mValue, mBufferOffset,
                                  mBufferSize);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::SerialBindGroupDescriptor,
                                  mLayout, mEntries);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialProgrammableStageDescriptor, mModule, mEntryPoint);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialVertexBufferLayoutDescriptor, mArrayStride,
    mStepMode, mAttributes);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::SerialVertexStateDescriptor,
                                  mIndexFormat, mVertexBuffers);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialComputePipelineDescriptor, mLayout, mComputeStage);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialRenderPipelineDescriptor, mLayout, mVertexStage,
    mFragmentStage, mPrimitiveTopology, mRasterizationState, mColorStates,
    mDepthStencilState, mVertexState, mSampleCount, mSampleMask,
    mAlphaToCoverageEnabled);

#undef DEFINE_IPC_SERIALIZER_FFI_ENUM
#undef DEFINE_IPC_SERIALIZER_DOM_ENUM
#undef DEFINE_IPC_SERIALIZER_ENUM_GUARD

}  // namespace IPC
#endif  // WEBGPU_SERIALIZE_H_
