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
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUAddressMode);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUCompareFunction);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUFilterMode);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureDimension);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureFormat);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureAspect);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPURequestAdapterOptions,
                                  mPowerPreference);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUExtensions,
                                  mAnisotropicFiltering);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPULimits, mMaxBindGroups);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUDeviceDescriptor,
                                  mExtensions, mLimits);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUBufferDescriptor, mSize,
                                  mUsage);
DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandEncoderDescriptor);
DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandBufferDescriptor);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUExtent3d, width,
                                  height, depth);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUTextureDescriptor,
                                  size, array_layer_count, mip_level_count,
                                  sample_count, dimension, format, usage);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUTextureViewDescriptor, format, dimension, aspect,
    base_mip_level, level_count, base_array_layer, array_layer_count);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::ffi::WGPUSamplerDescriptor,
                                  address_mode_u, address_mode_v,
                                  address_mode_w, mag_filter, min_filter,
                                  mipmap_filter, lod_min_clamp, lod_max_clamp,
                                  compare_function);

DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUBindingType);
DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUTextureViewDimension);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::ffi::WGPUBindGroupLayoutBinding, binding, visibility, ty,
    texture_dimension, multisampled, dynamic);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialBindGroupLayoutDescriptor, mBindings);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialPipelineLayoutDescriptor, mBindGroupLayouts);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::webgpu::SerialBindGroupBindingType);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::SerialBindGroupBinding,
                                  mBinding, mType, mValue, mBufferOffset,
                                  mBufferSize);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::SerialBindGroupDescriptor,
                                  mLayout, mBindings);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialProgrammableStageDescriptor, mModule, mEntryPoint);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::webgpu::SerialComputePipelineDescriptor, mLayout, mComputeStage);

#undef DEFINE_IPC_SERIALIZER_FFI_ENUM
#undef DEFINE_IPC_SERIALIZER_DOM_ENUM
#undef DEFINE_IPC_SERIALIZER_ENUM_GUARD

}  // namespace IPC
#endif  // WEBGPU_SERIALIZE_H_
