/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "Sampler.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGPUChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGPUChild, Release)

static ffi::WGPUCompareFunction ConvertCompareFunction(
    const dom::GPUCompareFunction& aCompare) {
  // Value of 0 = Undefined is reserved on the C side for "null" semantics.
  return ffi::WGPUCompareFunction(UnderlyingValue(aCompare) + 1);
}

static ffi::WGPUTextureFormat ConvertTextureFormat(
    const dom::GPUTextureFormat& aFormat) {
  switch (aFormat) {
    case dom::GPUTextureFormat::R8unorm:
      return ffi::WGPUTextureFormat_R8Unorm;
    case dom::GPUTextureFormat::R8snorm:
      return ffi::WGPUTextureFormat_R8Snorm;
    case dom::GPUTextureFormat::R8uint:
      return ffi::WGPUTextureFormat_R8Uint;
    case dom::GPUTextureFormat::R8sint:
      return ffi::WGPUTextureFormat_R8Sint;
    case dom::GPUTextureFormat::R16uint:
      return ffi::WGPUTextureFormat_R16Uint;
    case dom::GPUTextureFormat::R16sint:
      return ffi::WGPUTextureFormat_R16Sint;
    case dom::GPUTextureFormat::R16float:
      return ffi::WGPUTextureFormat_R16Float;
    case dom::GPUTextureFormat::Rg8unorm:
      return ffi::WGPUTextureFormat_Rg8Unorm;
    case dom::GPUTextureFormat::Rg8snorm:
      return ffi::WGPUTextureFormat_Rg8Snorm;
    case dom::GPUTextureFormat::Rg8uint:
      return ffi::WGPUTextureFormat_Rg8Uint;
    case dom::GPUTextureFormat::Rg8sint:
      return ffi::WGPUTextureFormat_Rg8Sint;
    case dom::GPUTextureFormat::R32uint:
      return ffi::WGPUTextureFormat_R32Uint;
    case dom::GPUTextureFormat::R32sint:
      return ffi::WGPUTextureFormat_R32Sint;
    case dom::GPUTextureFormat::R32float:
      return ffi::WGPUTextureFormat_R32Float;
    case dom::GPUTextureFormat::Rg16uint:
      return ffi::WGPUTextureFormat_Rg16Uint;
    case dom::GPUTextureFormat::Rg16sint:
      return ffi::WGPUTextureFormat_Rg16Sint;
    case dom::GPUTextureFormat::Rg16float:
      return ffi::WGPUTextureFormat_Rg16Float;
    case dom::GPUTextureFormat::Rgba8unorm:
      return ffi::WGPUTextureFormat_Rgba8Unorm;
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
      return ffi::WGPUTextureFormat_Rgba8UnormSrgb;
    case dom::GPUTextureFormat::Rgba8snorm:
      return ffi::WGPUTextureFormat_Rgba8Snorm;
    case dom::GPUTextureFormat::Rgba8uint:
      return ffi::WGPUTextureFormat_Rgba8Uint;
    case dom::GPUTextureFormat::Rgba8sint:
      return ffi::WGPUTextureFormat_Rgba8Sint;
    case dom::GPUTextureFormat::Bgra8unorm:
      return ffi::WGPUTextureFormat_Bgra8Unorm;
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
      return ffi::WGPUTextureFormat_Bgra8UnormSrgb;
    case dom::GPUTextureFormat::Rgb10a2unorm:
      return ffi::WGPUTextureFormat_Rgb10a2Unorm;
    case dom::GPUTextureFormat::Rg11b10float:
      return ffi::WGPUTextureFormat_Rg11b10Float;
    case dom::GPUTextureFormat::Rg32uint:
      return ffi::WGPUTextureFormat_Rg32Uint;
    case dom::GPUTextureFormat::Rg32sint:
      return ffi::WGPUTextureFormat_Rg32Sint;
    case dom::GPUTextureFormat::Rg32float:
      return ffi::WGPUTextureFormat_Rg32Float;
    case dom::GPUTextureFormat::Rgba16uint:
      return ffi::WGPUTextureFormat_Rgba16Uint;
    case dom::GPUTextureFormat::Rgba16sint:
      return ffi::WGPUTextureFormat_Rgba16Sint;
    case dom::GPUTextureFormat::Rgba16float:
      return ffi::WGPUTextureFormat_Rgba16Float;
    case dom::GPUTextureFormat::Rgba32uint:
      return ffi::WGPUTextureFormat_Rgba32Uint;
    case dom::GPUTextureFormat::Rgba32sint:
      return ffi::WGPUTextureFormat_Rgba32Sint;
    case dom::GPUTextureFormat::Rgba32float:
      return ffi::WGPUTextureFormat_Rgba32Float;
    case dom::GPUTextureFormat::Depth32float:
      return ffi::WGPUTextureFormat_Depth32Float;
    case dom::GPUTextureFormat::Bc1_rgba_unorm:
      return ffi::WGPUTextureFormat_Bc1RgbaUnorm;
    case dom::GPUTextureFormat::Bc1_rgba_unorm_srgb:
      return ffi::WGPUTextureFormat_Bc1RgbaUnormSrgb;
    case dom::GPUTextureFormat::Bc4_r_unorm:
      return ffi::WGPUTextureFormat_Bc4RUnorm;
    case dom::GPUTextureFormat::Bc4_r_snorm:
      return ffi::WGPUTextureFormat_Bc4RSnorm;
    case dom::GPUTextureFormat::Bc2_rgba_unorm:
      return ffi::WGPUTextureFormat_Bc2RgbaUnorm;
    case dom::GPUTextureFormat::Bc2_rgba_unorm_srgb:
      return ffi::WGPUTextureFormat_Bc2RgbaUnormSrgb;
    case dom::GPUTextureFormat::Bc3_rgba_unorm:
      return ffi::WGPUTextureFormat_Bc3RgbaUnorm;
    case dom::GPUTextureFormat::Bc3_rgba_unorm_srgb:
      return ffi::WGPUTextureFormat_Bc3RgbaUnormSrgb;
    case dom::GPUTextureFormat::Bc5_rg_unorm:
      return ffi::WGPUTextureFormat_Bc5RgUnorm;
    case dom::GPUTextureFormat::Bc5_rg_snorm:
      return ffi::WGPUTextureFormat_Bc5RgSnorm;
    case dom::GPUTextureFormat::Bc6h_rgb_ufloat:
      return ffi::WGPUTextureFormat_Bc6hRgbUfloat;
    case dom::GPUTextureFormat::Bc6h_rgb_float:
      return ffi::WGPUTextureFormat_Bc6hRgbSfloat;
    case dom::GPUTextureFormat::Bc7_rgba_unorm:
      return ffi::WGPUTextureFormat_Bc7RgbaUnorm;
    case dom::GPUTextureFormat::Bc7_rgba_unorm_srgb:
      return ffi::WGPUTextureFormat_Bc7RgbaUnormSrgb;
    case dom::GPUTextureFormat::Depth24plus:
      return ffi::WGPUTextureFormat_Depth24Plus;
    case dom::GPUTextureFormat::Depth24plus_stencil8:
      return ffi::WGPUTextureFormat_Depth24PlusStencil8;
    case dom::GPUTextureFormat::EndGuard_:
      MOZ_ASSERT_UNREACHABLE();
  }
  MOZ_CRASH("unexpected texture format enum");
}

static ffi::WGPUClient* initialize() {
  ffi::WGPUInfrastructure infra = ffi::wgpu_client_new();
  return infra.client;
}

WebGPUChild::WebGPUChild() : mClient(initialize()), mIPCOpen(false) {}

WebGPUChild::~WebGPUChild() {
  if (mClient) {
    ffi::wgpu_client_delete(mClient);
  }
}

RefPtr<AdapterPromise> WebGPUChild::InstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions) {
  const int max_ids = 10;
  RawId ids[max_ids] = {0};
  unsigned long count =
      ffi::wgpu_client_make_adapter_ids(mClient, ids, max_ids);

  nsTArray<RawId> sharedIds(count);
  for (unsigned long i = 0; i != count; ++i) {
    sharedIds.AppendElement(ids[i]);
  }

  return SendInstanceRequestAdapter(aOptions, sharedIds)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [](ipc::ByteBuf&& aInfoBuf) {
            // Ideally, we'd just send an empty ByteBuf, but the IPC code
            // complains if the capacity is zero...
            // So for the case where an adapter wasn't found, we just
            // transfer a single 0u64 in this buffer.
            return aInfoBuf.mLen > sizeof(uint64_t)
                       ? AdapterPromise::CreateAndResolve(std::move(aInfoBuf),
                                                          __func__)
                       : AdapterPromise::CreateAndReject(Nothing(), __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return AdapterPromise::CreateAndReject(Some(aReason), __func__);
          });
}

Maybe<RawId> WebGPUChild::AdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient, aSelfId);

  ffi::WGPUDeviceDescriptor desc = {};
  ffi::wgpu_client_fill_default_limits(&desc.limits);

  if (aDesc.mNonGuaranteedLimits.WasPassed()) {
    for (const auto& entry : aDesc.mNonGuaranteedLimits.Value().Entries()) {
      Unused << entry;  // TODO
    }
    /*desc.limits.max_bind_groups = lim.mMaxBindGroups;
    desc.limits.max_dynamic_uniform_buffers_per_pipeline_layout =
        lim.mMaxDynamicUniformBuffersPerPipelineLayout;
    desc.limits.max_dynamic_storage_buffers_per_pipeline_layout =
        lim.mMaxDynamicStorageBuffersPerPipelineLayout;
    desc.limits.max_sampled_textures_per_shader_stage =
        lim.mMaxSampledTexturesPerShaderStage;
    desc.limits.max_samplers_per_shader_stage = lim.mMaxSamplersPerShaderStage;
    desc.limits.max_storage_buffers_per_shader_stage =
        lim.mMaxStorageBuffersPerShaderStage;
    desc.limits.max_storage_textures_per_shader_stage =
        lim.mMaxStorageTexturesPerShaderStage;
    desc.limits.max_uniform_buffers_per_shader_stage =
        lim.mMaxUniformBuffersPerShaderStage;
    desc.limits.max_uniform_buffer_binding_size =
        lim.mMaxUniformBufferBindingSize;*/
  }

  ByteBuf bb;
  ffi::wgpu_client_serialize_device_descriptor(&desc, ToFFI(&bb));
  if (SendAdapterRequestDevice(aSelfId, std::move(bb), id)) {
    return Some(id);
  }
  ffi::wgpu_client_kill_device_id(mClient, id);
  return Nothing();
}

RawId WebGPUChild::DeviceCreateBuffer(RawId aSelfId,
                                      const dom::GPUBufferDescriptor& aDesc) {
  ffi::WGPUBufferDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  desc.size = aDesc.mSize;
  desc.usage = aDesc.mUsage;
  desc.mapped_at_creation = aDesc.mMappedAtCreation;

  ByteBuf bb;
  RawId id =
      ffi::wgpu_client_create_buffer(mClient, aSelfId, &desc, ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateTexture(RawId aSelfId,
                                       const dom::GPUTextureDescriptor& aDesc) {
  ffi::WGPUTextureDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  if (aDesc.mSize.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = aDesc.mSize.GetAsRangeEnforcedUnsignedLongSequence();
    desc.size.width = seq.Length() > 0 ? seq[0] : 1;
    desc.size.height = seq.Length() > 1 ? seq[1] : 1;
    desc.size.depth_or_array_layers = seq.Length() > 2 ? seq[2] : 1;
  } else if (aDesc.mSize.IsGPUExtent3DDict()) {
    const auto& dict = aDesc.mSize.GetAsGPUExtent3DDict();
    desc.size.width = dict.mWidth;
    desc.size.height = dict.mHeight;
    desc.size.depth_or_array_layers = dict.mDepthOrArrayLayers;
  } else {
    MOZ_CRASH("Unexpected union");
  }
  desc.mip_level_count = aDesc.mMipLevelCount;
  desc.sample_count = aDesc.mSampleCount;
  desc.dimension = ffi::WGPUTextureDimension(aDesc.mDimension);
  desc.format = ConvertTextureFormat(aDesc.mFormat);
  desc.usage = aDesc.mUsage;

  ByteBuf bb;
  RawId id =
      ffi::wgpu_client_create_texture(mClient, aSelfId, &desc, ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::TextureCreateView(
    RawId aSelfId, RawId aDeviceId,
    const dom::GPUTextureViewDescriptor& aDesc) {
  ffi::WGPUTextureViewDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }

  ffi::WGPUTextureFormat format = ffi::WGPUTextureFormat_Sentinel;
  if (aDesc.mFormat.WasPassed()) {
    format = ConvertTextureFormat(aDesc.mFormat.Value());
    desc.format = &format;
  }
  ffi::WGPUTextureViewDimension dimension =
      ffi::WGPUTextureViewDimension_Sentinel;
  if (aDesc.mDimension.WasPassed()) {
    dimension = ffi::WGPUTextureViewDimension(aDesc.mDimension.Value());
    desc.dimension = &dimension;
  }

  desc.aspect = ffi::WGPUTextureAspect(aDesc.mAspect);
  desc.base_mip_level = aDesc.mBaseMipLevel;
  desc.mip_level_count =
      aDesc.mMipLevelCount.WasPassed() ? aDesc.mMipLevelCount.Value() : 0;
  desc.base_array_layer = aDesc.mBaseArrayLayer;
  desc.array_layer_count =
      aDesc.mArrayLayerCount.WasPassed() ? aDesc.mArrayLayerCount.Value() : 0;

  ByteBuf bb;
  RawId id =
      ffi::wgpu_client_create_texture_view(mClient, aSelfId, &desc, ToFFI(&bb));
  if (!SendTextureAction(aSelfId, aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateSampler(RawId aSelfId,
                                       const dom::GPUSamplerDescriptor& aDesc) {
  ffi::WGPUSamplerDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }

  desc.address_modes[0] = ffi::WGPUAddressMode(aDesc.mAddressModeU);
  desc.address_modes[1] = ffi::WGPUAddressMode(aDesc.mAddressModeV);
  desc.address_modes[2] = ffi::WGPUAddressMode(aDesc.mAddressModeW);
  desc.mag_filter = ffi::WGPUFilterMode(aDesc.mMagFilter);
  desc.min_filter = ffi::WGPUFilterMode(aDesc.mMinFilter);
  desc.mipmap_filter = ffi::WGPUFilterMode(aDesc.mMipmapFilter);
  desc.lod_min_clamp = aDesc.mLodMinClamp;
  desc.lod_max_clamp = aDesc.mLodMaxClamp;

  ffi::WGPUCompareFunction comparison = ffi::WGPUCompareFunction_Sentinel;
  if (aDesc.mCompare.WasPassed()) {
    comparison = ConvertCompareFunction(aDesc.mCompare.Value());
    desc.compare = &comparison;
  }

  ByteBuf bb;
  RawId id =
      ffi::wgpu_client_create_sampler(mClient, aSelfId, &desc, ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateCommandEncoder(
    RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc) {
  ffi::WGPUCommandEncoderDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_command_encoder(mClient, aSelfId, &desc,
                                                     ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::CommandEncoderFinish(
    RawId aSelfId, RawId aDeviceId,
    const dom::GPUCommandBufferDescriptor& aDesc) {
  if (!SendCommandEncoderFinish(aSelfId, aDeviceId, aDesc)) {
    MOZ_CRASH("IPC failure");
  }
  // We rely on knowledge that `CommandEncoderId` == `CommandBufferId`
  // TODO: refactor this to truly behave as if the encoder is being finished,
  // and a new command buffer ID is being created from it. Resolve the ID
  // type aliasing at the place that introduces it: `wgpu-core`.
  return aSelfId;
}

RawId WebGPUChild::DeviceCreateBindGroupLayout(
    RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc) {
  struct OptionalData {
    ffi::WGPUTextureViewDimension dim;
    ffi::WGPURawTextureSampleType type;
    ffi::WGPUTextureFormat format;
  };
  nsTArray<OptionalData> optional(aDesc.mEntries.Length());
  for (const auto& entry : aDesc.mEntries) {
    OptionalData data = {};
    if (entry.mTexture.WasPassed()) {
      const auto& texture = entry.mTexture.Value();
      data.dim = ffi::WGPUTextureViewDimension(texture.mViewDimension);
      switch (texture.mSampleType) {
        case dom::GPUTextureSampleType::Float:
          data.type = ffi::WGPURawTextureSampleType_Float;
          break;
        case dom::GPUTextureSampleType::Unfilterable_float:
          data.type = ffi::WGPURawTextureSampleType_UnfilterableFloat;
          break;
        case dom::GPUTextureSampleType::Uint:
          data.type = ffi::WGPURawTextureSampleType_Uint;
          break;
        case dom::GPUTextureSampleType::Sint:
          data.type = ffi::WGPURawTextureSampleType_Sint;
          break;
        case dom::GPUTextureSampleType::Depth:
          data.type = ffi::WGPURawTextureSampleType_Depth;
          break;
        case dom::GPUTextureSampleType::EndGuard_:
          MOZ_ASSERT_UNREACHABLE();
      }
    }
    if (entry.mStorageTexture.WasPassed()) {
      const auto& texture = entry.mStorageTexture.Value();
      data.dim = ffi::WGPUTextureViewDimension(texture.mViewDimension);
      data.format = ConvertTextureFormat(texture.mFormat);
    }
    optional.AppendElement(data);
  }

  nsTArray<ffi::WGPUBindGroupLayoutEntry> entries(aDesc.mEntries.Length());
  for (size_t i = 0; i < aDesc.mEntries.Length(); ++i) {
    const auto& entry = aDesc.mEntries[i];
    ffi::WGPUBindGroupLayoutEntry e = {};
    e.binding = entry.mBinding;
    e.visibility = entry.mVisibility;
    if (entry.mBuffer.WasPassed()) {
      switch (entry.mBuffer.Value().mType) {
        case dom::GPUBufferBindingType::Uniform:
          e.ty = ffi::WGPURawBindingType_UniformBuffer;
          break;
        case dom::GPUBufferBindingType::Storage:
          e.ty = ffi::WGPURawBindingType_StorageBuffer;
          break;
        case dom::GPUBufferBindingType::Read_only_storage:
          e.ty = ffi::WGPURawBindingType_ReadonlyStorageBuffer;
          break;
        case dom::GPUBufferBindingType::EndGuard_:
          MOZ_ASSERT_UNREACHABLE();
      }
      e.has_dynamic_offset = entry.mBuffer.Value().mHasDynamicOffset;
    }
    if (entry.mTexture.WasPassed()) {
      e.ty = ffi::WGPURawBindingType_SampledTexture;
      e.view_dimension = &optional[i].dim;
      e.texture_sample_type = &optional[i].type;
      e.multisampled = entry.mTexture.Value().mMultisampled;
    }
    if (entry.mStorageTexture.WasPassed()) {
      e.ty = entry.mStorageTexture.Value().mAccess ==
                     dom::GPUStorageTextureAccess::Write_only
                 ? ffi::WGPURawBindingType_WriteonlyStorageTexture
                 : ffi::WGPURawBindingType_ReadonlyStorageTexture;
      e.view_dimension = &optional[i].dim;
      e.storage_texture_format = &optional[i].format;
    }
    if (entry.mSampler.WasPassed()) {
      e.ty = ffi::WGPURawBindingType_Sampler;
      switch (entry.mSampler.Value().mType) {
        case dom::GPUSamplerBindingType::Filtering:
          e.sampler_filter = true;
          break;
        case dom::GPUSamplerBindingType::Non_filtering:
          break;
        case dom::GPUSamplerBindingType::Comparison:
          e.sampler_compare = true;
          break;
        case dom::GPUSamplerBindingType::EndGuard_:
          MOZ_ASSERT_UNREACHABLE();
      }
    }
    entries.AppendElement(e);
  }

  ffi::WGPUBindGroupLayoutDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  desc.entries = entries.Elements();
  desc.entries_length = entries.Length();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_bind_group_layout(mClient, aSelfId, &desc,
                                                       ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreatePipelineLayout(
    RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc) {
  nsTArray<ffi::WGPUBindGroupLayoutId> bindGroupLayouts(
      aDesc.mBindGroupLayouts.Length());
  for (const auto& layout : aDesc.mBindGroupLayouts) {
    bindGroupLayouts.AppendElement(layout->mId);
  }

  ffi::WGPUPipelineLayoutDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  desc.bind_group_layouts = bindGroupLayouts.Elements();
  desc.bind_group_layouts_length = bindGroupLayouts.Length();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_pipeline_layout(mClient, aSelfId, &desc,
                                                     ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateBindGroup(
    RawId aSelfId, const dom::GPUBindGroupDescriptor& aDesc) {
  nsTArray<ffi::WGPUBindGroupEntry> entries(aDesc.mEntries.Length());
  for (const auto& entry : aDesc.mEntries) {
    ffi::WGPUBindGroupEntry e = {};
    e.binding = entry.mBinding;
    if (entry.mResource.IsGPUBufferBinding()) {
      const auto& bufBinding = entry.mResource.GetAsGPUBufferBinding();
      e.buffer = bufBinding.mBuffer->mId;
      e.offset = bufBinding.mOffset;
      e.size = bufBinding.mSize.WasPassed() ? bufBinding.mSize.Value() : 0;
    }
    if (entry.mResource.IsGPUTextureView()) {
      e.texture_view = entry.mResource.GetAsGPUTextureView()->mId;
    }
    if (entry.mResource.IsGPUSampler()) {
      e.sampler = entry.mResource.GetAsGPUSampler()->mId;
    }
    entries.AppendElement(e);
  }

  ffi::WGPUBindGroupDescriptor desc = {};
  nsCString label;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  desc.layout = aDesc.mLayout->mId;
  desc.entries = entries.Elements();
  desc.entries_length = entries.Length();

  ByteBuf bb;
  RawId id =
      ffi::wgpu_client_create_bind_group(mClient, aSelfId, &desc, ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateShaderModule(
    RawId aSelfId, const dom::GPUShaderModuleDescriptor& aDesc) {
  ffi::WGPUShaderModuleDescriptor desc = {};

  nsCString wgsl;
  if (aDesc.mCode.IsUSVString()) {
    LossyCopyUTF16toASCII(aDesc.mCode.GetAsUSVString(), wgsl);
    desc.wgsl_chars = wgsl.get();
  } else {
    const auto& code = aDesc.mCode.GetAsUint32Array();
    code.ComputeState();
    desc.spirv_words = code.Data();
    desc.spirv_words_length = code.Length();
  }

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_shader_module(mClient, aSelfId, &desc,
                                                   ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateComputePipeline(
    RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc,
    nsTArray<RawId>* const aImplicitBindGroupLayoutIds) {
  ffi::WGPUComputePipelineDescriptor desc = {};
  nsCString label, entryPoint;
  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  if (aDesc.mLayout.WasPassed()) {
    desc.layout = aDesc.mLayout.Value().mId;
  }
  desc.stage.module = aDesc.mCompute.mModule->mId;
  LossyCopyUTF16toASCII(aDesc.mCompute.mEntryPoint, entryPoint);
  desc.stage.entry_point = entryPoint.get();

  ByteBuf bb;
  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_compute_pipeline(
      mClient, aSelfId, &desc, ToFFI(&bb), implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aImplicitBindGroupLayoutIds->AppendElement(cur);
  }
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

static ffi::WGPUMultisampleState ConvertMultisampleState(
    const dom::GPUMultisampleState& aDesc) {
  ffi::WGPUMultisampleState desc = {};
  desc.count = aDesc.mCount;
  desc.mask = aDesc.mMask;
  desc.alpha_to_coverage_enabled = aDesc.mAlphaToCoverageEnabled;
  return desc;
}

static ffi::WGPUBlendComponent ConvertBlendComponent(
    const dom::GPUBlendComponent& aDesc) {
  ffi::WGPUBlendComponent desc = {};
  desc.src_factor = ffi::WGPUBlendFactor(aDesc.mSrcFactor);
  desc.dst_factor = ffi::WGPUBlendFactor(aDesc.mDstFactor);
  desc.operation = ffi::WGPUBlendOperation(aDesc.mOperation);
  return desc;
}

static ffi::WGPUStencilFaceState ConvertStencilFaceState(
    const dom::GPUStencilFaceState& aDesc) {
  ffi::WGPUStencilFaceState desc = {};
  desc.compare = ConvertCompareFunction(aDesc.mCompare);
  desc.fail_op = ffi::WGPUStencilOperation(aDesc.mFailOp);
  desc.depth_fail_op = ffi::WGPUStencilOperation(aDesc.mDepthFailOp);
  desc.pass_op = ffi::WGPUStencilOperation(aDesc.mPassOp);
  return desc;
}

static ffi::WGPUDepthStencilState ConvertDepthStencilState(
    const dom::GPUDepthStencilState& aDesc) {
  ffi::WGPUDepthStencilState desc = {};
  desc.format = ConvertTextureFormat(aDesc.mFormat);
  desc.depth_write_enabled = aDesc.mDepthWriteEnabled;
  desc.depth_compare = ConvertCompareFunction(aDesc.mDepthCompare);
  desc.stencil.front = ConvertStencilFaceState(aDesc.mStencilFront);
  desc.stencil.back = ConvertStencilFaceState(aDesc.mStencilBack);
  desc.stencil.read_mask = aDesc.mStencilReadMask;
  desc.stencil.write_mask = aDesc.mStencilWriteMask;
  desc.bias.constant = aDesc.mDepthBias;
  desc.bias.slope_scale = aDesc.mDepthBiasSlopeScale;
  desc.bias.clamp = aDesc.mDepthBiasClamp;
  return desc;
}

RawId WebGPUChild::DeviceCreateRenderPipeline(
    RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc,
    nsTArray<RawId>* const aImplicitBindGroupLayoutIds) {
  // A bunch of stack locals that we can have pointers into
  nsTArray<ffi::WGPUVertexBufferLayout> vertexBuffers;
  nsTArray<ffi::WGPUVertexAttribute> vertexAttributes;
  ffi::WGPURenderPipelineDescriptor desc = {};
  nsCString label, vsEntry, fsEntry;
  ffi::WGPUIndexFormat stripIndexFormat = ffi::WGPUIndexFormat_Uint16;
  ffi::WGPUFace cullFace = ffi::WGPUFace_Front;
  ffi::WGPUVertexState vertexState = {};
  ffi::WGPUFragmentState fragmentState = {};
  nsTArray<ffi::WGPUColorTargetState> colorStates;
  nsTArray<ffi::WGPUBlendState> blendStates;

  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  if (aDesc.mLayout.WasPassed()) {
    desc.layout = aDesc.mLayout.Value().mId;
  }

  {
    const auto& stage = aDesc.mVertex;
    vertexState.stage.module = stage.mModule->mId;
    LossyCopyUTF16toASCII(stage.mEntryPoint, vsEntry);
    vertexState.stage.entry_point = vsEntry.get();

    for (const auto& vertex_desc : stage.mBuffers) {
      ffi::WGPUVertexBufferLayout vb_desc = {};
      if (!vertex_desc.IsNull()) {
        const auto& vd = vertex_desc.Value();
        vb_desc.array_stride = vd.mArrayStride;
        vb_desc.step_mode = ffi::WGPUInputStepMode(vd.mStepMode);
        // Note: we are setting the length but not the pointer
        vb_desc.attributes_length = vd.mAttributes.Length();
        for (const auto& vat : vd.mAttributes) {
          ffi::WGPUVertexAttribute ad = {};
          ad.offset = vat.mOffset;
          ad.format = ffi::WGPUVertexFormat(vat.mFormat);
          ad.shader_location = vat.mShaderLocation;
          vertexAttributes.AppendElement(ad);
        }
      }
      vertexBuffers.AppendElement(vb_desc);
    }
    // Now patch up all the pointers to attribute lists.
    size_t numAttributes = 0;
    for (auto& vb_desc : vertexBuffers) {
      vb_desc.attributes = vertexAttributes.Elements() + numAttributes;
      numAttributes += vb_desc.attributes_length;
    }

    vertexState.buffers = vertexBuffers.Elements();
    vertexState.buffers_length = vertexBuffers.Length();
    desc.vertex = &vertexState;
  }

  if (aDesc.mFragment.WasPassed()) {
    const auto& stage = aDesc.mFragment.Value();
    fragmentState.stage.module = stage.mModule->mId;
    LossyCopyUTF16toASCII(stage.mEntryPoint, fsEntry);
    fragmentState.stage.entry_point = fsEntry.get();

    // Note: we pre-collect the blend states into a different array
    // so that we can have non-stale pointers into it.
    for (const auto& colorState : stage.mTargets) {
      ffi::WGPUColorTargetState desc = {};
      desc.format = ConvertTextureFormat(colorState.mFormat);
      desc.write_mask = colorState.mWriteMask;
      colorStates.AppendElement(desc);
      ffi::WGPUBlendState bs = {};
      if (colorState.mBlend.WasPassed()) {
        const auto& blend = colorState.mBlend.Value();
        bs.alpha = ConvertBlendComponent(blend.mAlpha);
        bs.color = ConvertBlendComponent(blend.mColor);
      }
      blendStates.AppendElement(bs);
    }
    for (size_t i = 0; i < colorStates.Length(); ++i) {
      if (stage.mTargets[i].mBlend.WasPassed()) {
        colorStates[i].blend = &blendStates[i];
      }
    }

    fragmentState.targets = colorStates.Elements();
    fragmentState.targets_length = colorStates.Length();
    desc.fragment = &fragmentState;
  }

  {
    const auto& prim = aDesc.mPrimitive;
    desc.primitive.topology = ffi::WGPUPrimitiveTopology(prim.mTopology);
    if (prim.mStripIndexFormat.WasPassed()) {
      stripIndexFormat = ffi::WGPUIndexFormat(prim.mStripIndexFormat.Value());
      desc.primitive.strip_index_format = &stripIndexFormat;
    }
    desc.primitive.front_face = ffi::WGPUFrontFace(prim.mFrontFace);
    if (prim.mCullMode != dom::GPUCullMode::None) {
      cullFace = prim.mCullMode == dom::GPUCullMode::Front ? ffi::WGPUFace_Front
                                                           : ffi::WGPUFace_Back;
      desc.primitive.cull_mode = &cullFace;
    }
  }
  desc.multisample = ConvertMultisampleState(aDesc.mMultisample);

  ffi::WGPUDepthStencilState depthStencilState = {};
  if (aDesc.mDepthStencil.WasPassed()) {
    depthStencilState = ConvertDepthStencilState(aDesc.mDepthStencil.Value());
    desc.depth_stencil = &depthStencilState;
  }

  ByteBuf bb;
  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_render_pipeline(
      mClient, aSelfId, &desc, ToFFI(&bb), implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aImplicitBindGroupLayoutIds->AppendElement(cur);
  }
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

ipc::IPCResult WebGPUChild::RecvError(RawId aDeviceId,
                                      const nsACString& aMessage) {
  if (!aDeviceId) {
    // TODO: figure out how to report these kinds of errors
    printf_stderr("Validation error without device target: %s\n",
                  PromiseFlatCString(aMessage).get());
  } else if (mDeviceMap.find(aDeviceId) == mDeviceMap.end()) {
    printf_stderr("Validation error on a dropped device: %s\n",
                  PromiseFlatCString(aMessage).get());
  } else {
    auto* target = mDeviceMap[aDeviceId];
    MOZ_ASSERT(target);
    dom::GPUUncapturedErrorEventInit init;
    init.mError.SetAsGPUValidationError() =
        new ValidationError(target, aMessage);
    RefPtr<mozilla::dom::GPUUncapturedErrorEvent> event =
        dom::GPUUncapturedErrorEvent::Constructor(target, u"uncapturederror"_ns,
                                                  init);
    target->DispatchEvent(*event);
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDropAction(const ipc::ByteBuf& aByteBuf) {
  const auto* byteBuf = ToFFI(&aByteBuf);
  ffi::wgpu_client_drop_action(mClient, byteBuf);
  return IPC_OK();
}

void WebGPUChild::DeviceCreateSwapChain(RawId aSelfId,
                                        const RGBDescriptor& aRgbDesc,
                                        size_t maxBufferCount,
                                        wr::ExternalImageId aExternalImageId) {
  RawId queueId = aSelfId;  // TODO: multiple queues
  nsTArray<RawId> bufferIds(maxBufferCount);
  for (size_t i = 0; i < maxBufferCount; ++i) {
    bufferIds.AppendElement(ffi::wgpu_client_make_buffer_id(mClient, aSelfId));
  }
  SendDeviceCreateSwapChain(aSelfId, queueId, aRgbDesc, bufferIds,
                            aExternalImageId);
}

void WebGPUChild::SwapChainPresent(wr::ExternalImageId aExternalImageId,
                                   RawId aTextureId) {
  // Hack: the function expects `DeviceId`, but it only uses it for `backend()`
  // selection.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mClient, aTextureId);
  SendSwapChainPresent(aExternalImageId, aTextureId, encoderId);
}

void WebGPUChild::RegisterDevice(RawId aId, Device* aDevice) {
  mDeviceMap.insert({aId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aId) {
  mDeviceMap.erase(aId);
  SendDeviceDestroy(aId);
}

}  // namespace webgpu
}  // namespace mozilla
