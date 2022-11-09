/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"
#include "js/Warnings.h"  // JS::WarnUTF8
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "Adapter.h"
#include "DeviceLostInfo.h"
#include "Sampler.h"
#include "CompilationInfo.h"

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGPUChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGPUChild, Release)

void WebGPUChild::JsWarning(nsIGlobalObject* aGlobal,
                            const nsACString& aMessage) {
  const auto& flatString = PromiseFlatCString(aMessage);
  if (aGlobal) {
    dom::AutoJSAPI api;
    if (api.Init(aGlobal)) {
      JS::WarnUTF8(api.cx(), "%s", flatString.get());
    }
  } else {
    printf_stderr("Validation error without device target: %s\n",
                  flatString.get());
  }
}

static ffi::WGPUCompareFunction ConvertCompareFunction(
    const dom::GPUCompareFunction& aCompare) {
  // Value of 0 = Undefined is reserved on the C side for "null" semantics.
  return ffi::WGPUCompareFunction(UnderlyingValue(aCompare) + 1);
}

static ffi::WGPUTextureFormat ConvertTextureFormat(
    const dom::GPUTextureFormat& aFormat) {
  ffi::WGPUTextureFormat result = {ffi::WGPUTextureFormat_Sentinel};
  switch (aFormat) {
    case dom::GPUTextureFormat::R8unorm:
      result.tag = ffi::WGPUTextureFormat_R8Unorm;
      break;
    case dom::GPUTextureFormat::R8snorm:
      result.tag = ffi::WGPUTextureFormat_R8Snorm;
      break;
    case dom::GPUTextureFormat::R8uint:
      result.tag = ffi::WGPUTextureFormat_R8Uint;
      break;
    case dom::GPUTextureFormat::R8sint:
      result.tag = ffi::WGPUTextureFormat_R8Sint;
      break;
    case dom::GPUTextureFormat::R16uint:
      result.tag = ffi::WGPUTextureFormat_R16Uint;
      break;
    case dom::GPUTextureFormat::R16sint:
      result.tag = ffi::WGPUTextureFormat_R16Sint;
      break;
    case dom::GPUTextureFormat::R16float:
      result.tag = ffi::WGPUTextureFormat_R16Float;
      break;
    case dom::GPUTextureFormat::Rg8unorm:
      result.tag = ffi::WGPUTextureFormat_Rg8Unorm;
      break;
    case dom::GPUTextureFormat::Rg8snorm:
      result.tag = ffi::WGPUTextureFormat_Rg8Snorm;
      break;
    case dom::GPUTextureFormat::Rg8uint:
      result.tag = ffi::WGPUTextureFormat_Rg8Uint;
      break;
    case dom::GPUTextureFormat::Rg8sint:
      result.tag = ffi::WGPUTextureFormat_Rg8Sint;
      break;
    case dom::GPUTextureFormat::R32uint:
      result.tag = ffi::WGPUTextureFormat_R32Uint;
      break;
    case dom::GPUTextureFormat::R32sint:
      result.tag = ffi::WGPUTextureFormat_R32Sint;
      break;
    case dom::GPUTextureFormat::R32float:
      result.tag = ffi::WGPUTextureFormat_R32Float;
      break;
    case dom::GPUTextureFormat::Rg16uint:
      result.tag = ffi::WGPUTextureFormat_Rg16Uint;
      break;
    case dom::GPUTextureFormat::Rg16sint:
      result.tag = ffi::WGPUTextureFormat_Rg16Sint;
      break;
    case dom::GPUTextureFormat::Rg16float:
      result.tag = ffi::WGPUTextureFormat_Rg16Float;
      break;
    case dom::GPUTextureFormat::Rgba8unorm:
      result.tag = ffi::WGPUTextureFormat_Rgba8Unorm;
      break;
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Rgba8UnormSrgb;
      break;
    case dom::GPUTextureFormat::Rgba8snorm:
      result.tag = ffi::WGPUTextureFormat_Rgba8Snorm;
      break;
    case dom::GPUTextureFormat::Rgba8uint:
      result.tag = ffi::WGPUTextureFormat_Rgba8Uint;
      break;
    case dom::GPUTextureFormat::Rgba8sint:
      result.tag = ffi::WGPUTextureFormat_Rgba8Sint;
      break;
    case dom::GPUTextureFormat::Bgra8unorm:
      result.tag = ffi::WGPUTextureFormat_Bgra8Unorm;
      break;
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Bgra8UnormSrgb;
      break;
    case dom::GPUTextureFormat::Rgb10a2unorm:
      result.tag = ffi::WGPUTextureFormat_Rgb10a2Unorm;
      break;
    case dom::GPUTextureFormat::Rg11b10float:
      result.tag = ffi::WGPUTextureFormat_Rg11b10Float;
      break;
    case dom::GPUTextureFormat::Rg32uint:
      result.tag = ffi::WGPUTextureFormat_Rg32Uint;
      break;
    case dom::GPUTextureFormat::Rg32sint:
      result.tag = ffi::WGPUTextureFormat_Rg32Sint;
      break;
    case dom::GPUTextureFormat::Rg32float:
      result.tag = ffi::WGPUTextureFormat_Rg32Float;
      break;
    case dom::GPUTextureFormat::Rgba16uint:
      result.tag = ffi::WGPUTextureFormat_Rgba16Uint;
      break;
    case dom::GPUTextureFormat::Rgba16sint:
      result.tag = ffi::WGPUTextureFormat_Rgba16Sint;
      break;
    case dom::GPUTextureFormat::Rgba16float:
      result.tag = ffi::WGPUTextureFormat_Rgba16Float;
      break;
    case dom::GPUTextureFormat::Rgba32uint:
      result.tag = ffi::WGPUTextureFormat_Rgba32Uint;
      break;
    case dom::GPUTextureFormat::Rgba32sint:
      result.tag = ffi::WGPUTextureFormat_Rgba32Sint;
      break;
    case dom::GPUTextureFormat::Rgba32float:
      result.tag = ffi::WGPUTextureFormat_Rgba32Float;
      break;
    case dom::GPUTextureFormat::Depth32float:
      result.tag = ffi::WGPUTextureFormat_Depth32Float;
      break;
    case dom::GPUTextureFormat::Bc1_rgba_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc1RgbaUnorm;
      break;
    case dom::GPUTextureFormat::Bc1_rgba_unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Bc1RgbaUnormSrgb;
      break;
    case dom::GPUTextureFormat::Bc4_r_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc4RUnorm;
      break;
    case dom::GPUTextureFormat::Bc4_r_snorm:
      result.tag = ffi::WGPUTextureFormat_Bc4RSnorm;
      break;
    case dom::GPUTextureFormat::Bc2_rgba_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc2RgbaUnorm;
      break;
    case dom::GPUTextureFormat::Bc2_rgba_unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Bc2RgbaUnormSrgb;
      break;
    case dom::GPUTextureFormat::Bc3_rgba_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc3RgbaUnorm;
      break;
    case dom::GPUTextureFormat::Bc3_rgba_unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Bc3RgbaUnormSrgb;
      break;
    case dom::GPUTextureFormat::Bc5_rg_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc5RgUnorm;
      break;
    case dom::GPUTextureFormat::Bc5_rg_snorm:
      result.tag = ffi::WGPUTextureFormat_Bc5RgSnorm;
      break;
    case dom::GPUTextureFormat::Bc6h_rgb_ufloat:
      result.tag = ffi::WGPUTextureFormat_Bc6hRgbUfloat;
      break;
    case dom::GPUTextureFormat::Bc6h_rgb_float:
      result.tag = ffi::WGPUTextureFormat_Bc6hRgbSfloat;
      break;
    case dom::GPUTextureFormat::Bc7_rgba_unorm:
      result.tag = ffi::WGPUTextureFormat_Bc7RgbaUnorm;
      break;
    case dom::GPUTextureFormat::Bc7_rgba_unorm_srgb:
      result.tag = ffi::WGPUTextureFormat_Bc7RgbaUnormSrgb;
      break;
    case dom::GPUTextureFormat::Depth24plus:
      result.tag = ffi::WGPUTextureFormat_Depth24Plus;
      break;
    case dom::GPUTextureFormat::Depth24plus_stencil8:
      result.tag = ffi::WGPUTextureFormat_Depth24PlusStencil8;
      break;
    case dom::GPUTextureFormat::EndGuard_:
      MOZ_ASSERT_UNREACHABLE();
  }

  // Clang will check for us that the switch above is exhaustive,
  // but not if we add a 'default' case. So, check this here.
  MOZ_ASSERT(result.tag != ffi::WGPUTextureFormat_Sentinel,
             "unexpected texture format enum");

  return result;
}

void WebGPUChild::ConvertTextureFormatRef(const dom::GPUTextureFormat& aInput,
                                          ffi::WGPUTextureFormat& aOutput) {
  aOutput = ConvertTextureFormat(aInput);
}

static UniquePtr<ffi::WGPUClient> initialize() {
  ffi::WGPUInfrastructure infra = ffi::wgpu_client_new();
  return UniquePtr<ffi::WGPUClient>{infra.client};
}

WebGPUChild::WebGPUChild() : mClient(initialize()) {}

WebGPUChild::~WebGPUChild() = default;

RefPtr<AdapterPromise> WebGPUChild::InstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions) {
  const int max_ids = 10;
  RawId ids[max_ids] = {0};
  unsigned long count =
      ffi::wgpu_client_make_adapter_ids(mClient.get(), ids, max_ids);

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

Maybe<DeviceRequest> WebGPUChild::AdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc,
    ffi::WGPULimits* aLimits) {
  ffi::WGPUDeviceDescriptor desc = {};
  ffi::wgpu_client_fill_default_limits(&desc.limits);

  // webgpu::StringHelper label(aDesc.mLabel);
  // desc.label = label.Get();

  const auto featureBits = Adapter::MakeFeatureBits(aDesc.mRequiredFeatures);
  if (!featureBits) {
    return Nothing();
  }
  desc.features = *featureBits;

  if (aDesc.mRequiredLimits.WasPassed()) {
    for (const auto& entry : aDesc.mRequiredLimits.Value().Entries()) {
      const uint32_t valueU32 =
          entry.mValue < std::numeric_limits<uint32_t>::max()
              ? entry.mValue
              : std::numeric_limits<uint32_t>::max();
      if (entry.mKey == u"maxTextureDimension1D"_ns) {
        desc.limits.max_texture_dimension_1d = valueU32;
      } else if (entry.mKey == u"maxTextureDimension2D"_ns) {
        desc.limits.max_texture_dimension_2d = valueU32;
      } else if (entry.mKey == u"maxTextureDimension3D"_ns) {
        desc.limits.max_texture_dimension_3d = valueU32;
      } else if (entry.mKey == u"maxTextureArrayLayers"_ns) {
        desc.limits.max_texture_array_layers = valueU32;
      } else if (entry.mKey == u"maxBindGroups"_ns) {
        desc.limits.max_bind_groups = valueU32;
      } else if (entry.mKey ==
                 u"maxDynamicUniformBuffersPerPipelineLayout"_ns) {
        desc.limits.max_dynamic_uniform_buffers_per_pipeline_layout = valueU32;
      } else if (entry.mKey ==
                 u"maxDynamicStorageBuffersPerPipelineLayout"_ns) {
        desc.limits.max_dynamic_storage_buffers_per_pipeline_layout = valueU32;
      } else if (entry.mKey == u"maxSampledTexturesPerShaderStage"_ns) {
        desc.limits.max_sampled_textures_per_shader_stage = valueU32;
      } else if (entry.mKey == u"maxSamplersPerShaderStage"_ns) {
        desc.limits.max_samplers_per_shader_stage = valueU32;
      } else if (entry.mKey == u"maxStorageBuffersPerShaderStage"_ns) {
        desc.limits.max_storage_buffers_per_shader_stage = valueU32;
      } else if (entry.mKey == u"maxStorageTexturesPerShaderStage"_ns) {
        desc.limits.max_storage_textures_per_shader_stage = valueU32;
      } else if (entry.mKey == u"maxUniformBuffersPerShaderStage"_ns) {
        desc.limits.max_uniform_buffers_per_shader_stage = valueU32;
      } else if (entry.mKey == u"maxUniformBufferBindingSize"_ns) {
        desc.limits.max_uniform_buffer_binding_size = entry.mValue;
      } else if (entry.mKey == u"maxStorageBufferBindingSize"_ns) {
        desc.limits.max_storage_buffer_binding_size = entry.mValue;
      } else if (entry.mKey == u"minUniformBufferOffsetAlignment"_ns) {
        desc.limits.min_uniform_buffer_offset_alignment = valueU32;
      } else if (entry.mKey == u"minStorageBufferOffsetAlignment"_ns) {
        desc.limits.min_storage_buffer_offset_alignment = valueU32;
      } else if (entry.mKey == u"maxVertexBuffers"_ns) {
        desc.limits.max_vertex_buffers = valueU32;
      } else if (entry.mKey == u"maxVertexAttributes"_ns) {
        desc.limits.max_vertex_attributes = valueU32;
      } else if (entry.mKey == u"maxVertexBufferArrayStride"_ns) {
        desc.limits.max_vertex_buffer_array_stride = valueU32;
      } else if (entry.mKey == u"maxComputeWorkgroupSizeX"_ns) {
        desc.limits.max_compute_workgroup_size_x = valueU32;
      } else if (entry.mKey == u"maxComputeWorkgroupSizeY"_ns) {
        desc.limits.max_compute_workgroup_size_y = valueU32;
      } else if (entry.mKey == u"maxComputeWorkgroupSizeZ"_ns) {
        desc.limits.max_compute_workgroup_size_z = valueU32;
      } else if (entry.mKey == u"maxComputeWorkgroupsPerDimension"_ns) {
        desc.limits.max_compute_workgroups_per_dimension = valueU32;
      } else {
        NS_WARNING(nsPrintfCString("Requested limit '%s' is not recognized.",
                                   NS_ConvertUTF16toUTF8(entry.mKey).get())
                       .get());
        return Nothing();
      }

      // TODO: maxInterStageShaderComponents
      // TODO: maxComputeWorkgroupStorageSize
      // TODO: maxComputeInvocationsPerWorkgroup
    }
  }

  RawId id = ffi::wgpu_client_make_device_id(mClient.get(), aSelfId);

  ByteBuf bb;
  ffi::wgpu_client_serialize_device_descriptor(&desc, ToFFI(&bb));

  DeviceRequest request;
  request.mId = id;
  request.mPromise = SendAdapterRequestDevice(aSelfId, std::move(bb), id);
  *aLimits = desc.limits;

  return Some(std::move(request));
}

RawId WebGPUChild::DeviceCreateBuffer(RawId aSelfId,
                                      const dom::GPUBufferDescriptor& aDesc,
                                      MaybeShmem&& aShmem) {
  RawId bufferId = ffi::wgpu_client_make_buffer_id(mClient.get(), aSelfId);
  if (!SendCreateBuffer(aSelfId, bufferId, aDesc, aShmem)) {
    MOZ_CRASH("IPC failure");
  }
  return bufferId;
}

RawId WebGPUChild::DeviceCreateTexture(RawId aSelfId,
                                       const dom::GPUTextureDescriptor& aDesc) {
  ffi::WGPUTextureDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

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
  RawId id = ffi::wgpu_client_create_texture(mClient.get(), aSelfId, &desc,
                                             ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::TextureCreateView(
    RawId aSelfId, RawId aDeviceId,
    const dom::GPUTextureViewDescriptor& aDesc) {
  ffi::WGPUTextureViewDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPUTextureFormat format = {ffi::WGPUTextureFormat_Sentinel};
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
  RawId id = ffi::wgpu_client_create_texture_view(mClient.get(), aSelfId, &desc,
                                                  ToFFI(&bb));
  if (!SendTextureAction(aSelfId, aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateSampler(RawId aSelfId,
                                       const dom::GPUSamplerDescriptor& aDesc) {
  ffi::WGPUSamplerDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
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
  RawId id = ffi::wgpu_client_create_sampler(mClient.get(), aSelfId, &desc,
                                             ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateCommandEncoder(
    RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc) {
  ffi::WGPUCommandEncoderDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_command_encoder(mClient.get(), aSelfId,
                                                     &desc, ToFFI(&bb));
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

RawId WebGPUChild::RenderBundleEncoderFinish(
    ffi::WGPURenderBundleEncoder& aEncoder, RawId aDeviceId,
    const dom::GPURenderBundleDescriptor& aDesc) {
  ffi::WGPURenderBundleDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle(
      mClient.get(), &aEncoder, aDeviceId, &desc, ToFFI(&bb));

  if (!SendDeviceAction(aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  return id;
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

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
  desc.entries = entries.Elements();
  desc.entries_length = entries.Length();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_bind_group_layout(mClient.get(), aSelfId,
                                                       &desc, ToFFI(&bb));
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
    if (!layout->IsValid()) {
      return 0;
    }
    bindGroupLayouts.AppendElement(layout->mId);
  }

  ffi::WGPUPipelineLayoutDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
  desc.bind_group_layouts = bindGroupLayouts.Elements();
  desc.bind_group_layouts_length = bindGroupLayouts.Length();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_pipeline_layout(mClient.get(), aSelfId,
                                                     &desc, ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::DeviceCreateBindGroup(
    RawId aSelfId, const dom::GPUBindGroupDescriptor& aDesc) {
  if (!aDesc.mLayout->IsValid()) {
    return 0;
  }

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

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
  desc.layout = aDesc.mLayout->mId;
  desc.entries = entries.Elements();
  desc.entries_length = entries.Length();

  ByteBuf bb;
  RawId id = ffi::wgpu_client_create_bind_group(mClient.get(), aSelfId, &desc,
                                                ToFFI(&bb));
  if (!SendDeviceAction(aSelfId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

already_AddRefed<ShaderModule> WebGPUChild::DeviceCreateShaderModule(
    Device* aDevice, const dom::GPUShaderModuleDescriptor& aDesc,
    RefPtr<dom::Promise> aPromise) {
  RawId deviceId = aDevice->mId;
  RawId moduleId =
      ffi::wgpu_client_make_shader_module_id(mClient.get(), deviceId);

  RefPtr<ShaderModule> shaderModule =
      new ShaderModule(aDevice, moduleId, aPromise);

  nsString noLabel;
  const nsString& label =
      aDesc.mLabel.WasPassed() ? aDesc.mLabel.Value() : noLabel;
  SendDeviceCreateShaderModule(deviceId, moduleId, label, aDesc.mCode)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aPromise,
           shaderModule](nsTArray<WebGPUCompilationMessage>&& messages) {
            RefPtr<CompilationInfo> infoObject(
                new CompilationInfo(shaderModule));
            infoObject->SetMessages(messages);
            aPromise->MaybeResolve(infoObject);
          },
          [aPromise](const ipc::ResponseRejectReason& aReason) {
            aPromise->MaybeRejectWithNotSupportedError("IPC error");
          });

  return shaderModule.forget();
}

RawId WebGPUChild::DeviceCreateComputePipelineImpl(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc, ByteBuf* const aByteBuf) {
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

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_compute_pipeline(
      mClient.get(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

RawId WebGPUChild::DeviceCreateComputePipeline(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateComputePipelineImpl(aContext, aDesc, &bb);

  if (!SendDeviceAction(aContext->mParentId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RefPtr<PipelinePromise> WebGPUChild::DeviceCreateComputePipelineAsync(
    PipelineCreationContext* const aContext,
    const dom::GPUComputePipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateComputePipelineImpl(aContext, aDesc, &bb);

  return SendDeviceActionWithAck(aContext->mParentId, std::move(bb))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [id](bool aDummy) {
            Unused << aDummy;
            return PipelinePromise::CreateAndResolve(id, __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return PipelinePromise::CreateAndReject(aReason, __func__);
          });
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

RawId WebGPUChild::DeviceCreateRenderPipelineImpl(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc, ByteBuf* const aByteBuf) {
  // A bunch of stack locals that we can have pointers into
  nsTArray<ffi::WGPUVertexBufferLayout> vertexBuffers;
  nsTArray<ffi::WGPUVertexAttribute> vertexAttributes;
  ffi::WGPURenderPipelineDescriptor desc = {};
  nsCString vsEntry, fsEntry;
  ffi::WGPUIndexFormat stripIndexFormat = ffi::WGPUIndexFormat_Uint16;
  ffi::WGPUFace cullFace = ffi::WGPUFace_Front;
  ffi::WGPUVertexState vertexState = {};
  ffi::WGPUFragmentState fragmentState = {};
  nsTArray<ffi::WGPUColorTargetState> colorStates;
  nsTArray<ffi::WGPUBlendState> blendStates;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

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
        vb_desc.step_mode = ffi::WGPUVertexStepMode(vd.mStepMode);
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

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_render_pipeline(
      mClient.get(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

RawId WebGPUChild::DeviceCreateRenderPipeline(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateRenderPipelineImpl(aContext, aDesc, &bb);

  if (!SendDeviceAction(aContext->mParentId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RefPtr<PipelinePromise> WebGPUChild::DeviceCreateRenderPipelineAsync(
    PipelineCreationContext* const aContext,
    const dom::GPURenderPipelineDescriptor& aDesc) {
  ByteBuf bb;
  const RawId id = DeviceCreateRenderPipelineImpl(aContext, aDesc, &bb);

  return SendDeviceActionWithAck(aContext->mParentId, std::move(bb))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [id](bool aDummy) {
            Unused << aDummy;
            return PipelinePromise::CreateAndResolve(id, __func__);
          },
          [](const ipc::ResponseRejectReason& aReason) {
            return PipelinePromise::CreateAndReject(aReason, __func__);
          });
}

ipc::IPCResult WebGPUChild::RecvDeviceUncapturedError(
    RawId aDeviceId, const nsACString& aMessage) {
  auto targetIter = mDeviceMap.find(aDeviceId);
  if (!aDeviceId || targetIter == mDeviceMap.end()) {
    JsWarning(nullptr, aMessage);
  } else {
    auto* target = targetIter->second.get();
    MOZ_ASSERT(target);
    // We don't want to spam the errors to the console indefinitely
    if (target->CheckNewWarning(aMessage)) {
      JsWarning(target->GetOwnerGlobal(), aMessage);

      dom::GPUUncapturedErrorEventInit init;
      init.mError.SetAsGPUValidationError() =
          new ValidationError(target->GetParentObject(), aMessage);
      RefPtr<mozilla::dom::GPUUncapturedErrorEvent> event =
          dom::GPUUncapturedErrorEvent::Constructor(
              target, u"uncapturederror"_ns, init);
      target->DispatchEvent(*event);
    }
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDropAction(const ipc::ByteBuf& aByteBuf) {
  const auto* byteBuf = ToFFI(&aByteBuf);
  ffi::wgpu_client_drop_action(mClient.get(), byteBuf);
  return IPC_OK();
}

void WebGPUChild::DeviceCreateSwapChain(
    RawId aSelfId, const RGBDescriptor& aRgbDesc, size_t maxBufferCount,
    const layers::CompositableHandle& aHandle) {
  RawId queueId = aSelfId;  // TODO: multiple queues
  nsTArray<RawId> bufferIds(maxBufferCount);
  for (size_t i = 0; i < maxBufferCount; ++i) {
    bufferIds.AppendElement(
        ffi::wgpu_client_make_buffer_id(mClient.get(), aSelfId));
  }
  SendDeviceCreateSwapChain(aSelfId, queueId, aRgbDesc, bufferIds, aHandle);
}

void WebGPUChild::SwapChainPresent(const layers::CompositableHandle& aHandle,
                                   RawId aTextureId) {
  // Hack: the function expects `DeviceId`, but it only uses it for `backend()`
  // selection.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mClient.get(), aTextureId);
  SendSwapChainPresent(aHandle, aTextureId, encoderId);
}

void WebGPUChild::RegisterDevice(Device* const aDevice) {
  mDeviceMap.insert({aDevice->mId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aId) {
  mDeviceMap.erase(aId);
  if (IsOpen()) {
    SendDeviceDestroy(aId);
  }
}

void WebGPUChild::FreeUnregisteredInParentDevice(RawId aId) {
  ffi::wgpu_client_kill_device_id(mClient.get(), aId);
  mDeviceMap.erase(aId);
}

void WebGPUChild::ActorDestroy(ActorDestroyReason) {
  // Resolving the promise could cause us to update the original map if the
  // callee frees the Device objects immediately. Since any remaining entries
  // in the map are no longer valid, we can just move the map onto the stack.
  const auto deviceMap = std::move(mDeviceMap);
  mDeviceMap.clear();

  for (const auto& targetIter : deviceMap) {
    RefPtr<Device> device = targetIter.second.get();
    if (!device) {
      // The Device may have gotten freed when we resolved the Promise for
      // another Device in the map.
      continue;
    }

    RefPtr<dom::Promise> promise = device->MaybeGetLost();
    if (!promise) {
      continue;
    }

    auto info = MakeRefPtr<DeviceLostInfo>(device->GetParentObject(),
                                           u"WebGPUChild destroyed"_ns);

    // We have strong references to both the Device and the DeviceLostInfo and
    // the Promise objects on the stack which keeps them alive for long enough.
    promise->MaybeResolve(info);
  }
}

}  // namespace mozilla::webgpu
