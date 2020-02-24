/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGPUChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGPUChild, Release)

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

RefPtr<RawIdPromise> WebGPUChild::InstanceRequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions) {
  const int max_ids = 10;
  RawId ids[max_ids] = {0};
  unsigned long count =
      ffi::wgpu_client_make_adapter_ids(mClient, ids, max_ids);

  auto client = mClient;
  nsTArray<RawId> sharedIds;
  for (unsigned long i = 0; i != count; ++i) {
    sharedIds.AppendElement(ids[i]);
  }

  return SendInstanceRequestAdapter(aOptions, sharedIds)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [client, ids, count](const RawId& aId) {
            if (aId == 0) {
              ffi::wgpu_client_kill_adapter_ids(client, ids, count);
              return RawIdPromise::CreateAndReject(Nothing(), __func__);
            } else {
              // find the position in the list
              unsigned int i = 0;
              while (ids[i] != aId) {
                i++;
              }
              if (i > 0) {
                ffi::wgpu_client_kill_adapter_ids(client, ids, i);
              }
              if (i + 1 < count) {
                ffi::wgpu_client_kill_adapter_ids(client, ids + i + 1,
                                                  count - i - 1);
              }
              return RawIdPromise::CreateAndResolve(aId, __func__);
            }
          },
          [client, ids, count](const ipc::ResponseRejectReason& aReason) {
            ffi::wgpu_client_kill_adapter_ids(client, ids, count);
            return RawIdPromise::CreateAndReject(Some(aReason), __func__);
          });
}

Maybe<RawId> WebGPUChild::AdapterRequestDevice(
    RawId aSelfId, const dom::GPUDeviceDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient, aSelfId);
  if (SendAdapterRequestDevice(aSelfId, aDesc, id)) {
    return Some(id);
  } else {
    ffi::wgpu_client_kill_device_id(mClient, id);
    return Nothing();
  }
}

void WebGPUChild::DestroyAdapter(RawId aId) {
  SendAdapterDestroy(aId);
  ffi::wgpu_client_kill_adapter_ids(mClient, &aId, 1);
}

RawId WebGPUChild::DeviceCreateBuffer(RawId aSelfId,
                                      const dom::GPUBufferDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_buffer_id(mClient, aSelfId);
  if (!SendDeviceCreateBuffer(aSelfId, aDesc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyBuffer(RawId aId) {
  SendBufferDestroy(aId);
  ffi::wgpu_client_kill_buffer_id(mClient, aId);
}

UniquePtr<ffi::WGPUTextureViewDescriptor> WebGPUChild::GetDefaultViewDescriptor(
    const dom::GPUTextureDescriptor& aDesc) {
  ffi::WGPUTextureViewDescriptor desc = {};
  desc.format = ffi::WGPUTextureFormat(aDesc.mFormat);
  switch (aDesc.mDimension) {
    case dom::GPUTextureDimension::_1d:
      desc.dimension = ffi::WGPUTextureViewDimension_D1;
      break;
    case dom::GPUTextureDimension::_2d:
      desc.dimension = ffi::WGPUTextureViewDimension_D2;
      break;
    case dom::GPUTextureDimension::_3d:
      desc.dimension = ffi::WGPUTextureViewDimension_D3;
      break;
    default:
      MOZ_CRASH("Unexpected texture dimension");
  }
  desc.level_count = aDesc.mMipLevelCount;
  desc.array_layer_count = aDesc.mArrayLayerCount;
  return UniquePtr<ffi::WGPUTextureViewDescriptor>(
      new ffi::WGPUTextureViewDescriptor(desc));
}

RawId WebGPUChild::DeviceCreateTexture(RawId aSelfId,
                                       const dom::GPUTextureDescriptor& aDesc) {
  ffi::WGPUTextureDescriptor desc = {};
  if (aDesc.mSize.IsUnsignedLongSequence()) {
    const auto& seq = aDesc.mSize.GetAsUnsignedLongSequence();
    desc.size.width = seq.Length() > 0 ? seq[0] : 1;
    desc.size.height = seq.Length() > 1 ? seq[1] : 1;
    desc.size.depth = seq.Length() > 2 ? seq[2] : 1;
  } else if (aDesc.mSize.IsGPUExtent3DDict()) {
    const auto& dict = aDesc.mSize.GetAsGPUExtent3DDict();
    desc.size.width = dict.mWidth;
    desc.size.height = dict.mHeight;
    desc.size.depth = dict.mDepth;
  } else {
    MOZ_CRASH("Unexpected union");
  }
  desc.array_layer_count = aDesc.mArrayLayerCount;
  desc.mip_level_count = aDesc.mMipLevelCount;
  desc.sample_count = aDesc.mSampleCount;
  desc.dimension = ffi::WGPUTextureDimension(aDesc.mDimension);
  desc.format = ffi::WGPUTextureFormat(aDesc.mFormat);
  desc.usage = aDesc.mUsage;

  RawId id = ffi::wgpu_client_make_texture_id(mClient, aSelfId);
  if (!SendDeviceCreateTexture(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::TextureCreateView(
    RawId aSelfId, const dom::GPUTextureViewDescriptor& aDesc,
    const ffi::WGPUTextureViewDescriptor& aDefaultViewDesc) {
  ffi::WGPUTextureViewDescriptor desc = aDefaultViewDesc;
  if (aDesc.mFormat.WasPassed()) {
    desc.format = ffi::WGPUTextureFormat(aDesc.mFormat.Value());
  }
  if (aDesc.mDimension.WasPassed()) {
    desc.dimension = ffi::WGPUTextureViewDimension(aDesc.mDimension.Value());
  }

  desc.aspect = ffi::WGPUTextureAspect(aDesc.mAspect);
  desc.base_mip_level = aDesc.mBaseMipLevel;
  desc.level_count = aDesc.mMipLevelCount
                         ? aDesc.mMipLevelCount
                         : aDefaultViewDesc.level_count - aDesc.mBaseMipLevel;
  desc.base_array_layer = aDesc.mBaseArrayLayer;
  desc.array_layer_count =
      aDesc.mArrayLayerCount
          ? aDesc.mArrayLayerCount
          : aDefaultViewDesc.array_layer_count - aDesc.mBaseArrayLayer;

  RawId id = ffi::wgpu_client_make_texture_view_id(mClient, aSelfId);
  if (!SendTextureCreateView(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyTexture(RawId aId) {
  SendTextureDestroy(aId);
  ffi::wgpu_client_kill_texture_id(mClient, aId);
}

void WebGPUChild::DestroyTextureView(RawId aId) {
  SendTextureViewDestroy(aId);
  ffi::wgpu_client_kill_texture_view_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateSampler(RawId aSelfId,
                                       const dom::GPUSamplerDescriptor& aDesc) {
  ffi::WGPUSamplerDescriptor desc = {};
  desc.address_mode_u = ffi::WGPUAddressMode(aDesc.mAddressModeU);
  desc.address_mode_v = ffi::WGPUAddressMode(aDesc.mAddressModeV);
  desc.address_mode_w = ffi::WGPUAddressMode(aDesc.mAddressModeW);
  desc.mag_filter = ffi::WGPUFilterMode(aDesc.mMagFilter);
  desc.min_filter = ffi::WGPUFilterMode(aDesc.mMinFilter);
  desc.mipmap_filter = ffi::WGPUFilterMode(aDesc.mMipmapFilter);
  desc.lod_min_clamp = aDesc.mLodMinClamp;
  desc.lod_max_clamp = aDesc.mLodMaxClamp;
  desc.compare_function = ffi::WGPUCompareFunction(aDesc.mCompare);

  RawId id = ffi::wgpu_client_make_sampler_id(mClient, aSelfId);
  if (!SendDeviceCreateSampler(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroySampler(RawId aId) {
  SendSamplerDestroy(aId);
  ffi::wgpu_client_kill_sampler_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateCommandEncoder(
    RawId aSelfId, const dom::GPUCommandEncoderDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_encoder_id(mClient, aSelfId);
  if (!SendDeviceCreateCommandEncoder(aSelfId, aDesc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

RawId WebGPUChild::CommandEncoderFinish(
    RawId aSelfId, const dom::GPUCommandBufferDescriptor& aDesc) {
  if (!SendCommandEncoderFinish(aSelfId, aDesc)) {
    MOZ_CRASH("IPC failure");
  }
  // We rely on knowledge that `CommandEncoderId` == `CommandBufferId`
  // TODO: refactor this to truly behave as if the encoder is being finished,
  // and a new command buffer ID is being created from it. Resolve the ID
  // type aliasing at the place that introduces it: `wgpu-core`.
  return aSelfId;
}

void WebGPUChild::DestroyCommandEncoder(RawId aId) {
  SendCommandEncoderDestroy(aId);
  ffi::wgpu_client_kill_encoder_id(mClient, aId);
}
void WebGPUChild::DestroyCommandBuffer(RawId aId) {
  SendCommandBufferDestroy(aId);
  ffi::wgpu_client_kill_encoder_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateBindGroupLayout(
    RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_bind_group_layout_id(mClient, aSelfId);
  nsTArray<ffi::WGPUBindGroupLayoutBinding> bindings(aDesc.mBindings.Length());
  for (const auto& binding : aDesc.mBindings) {
    ffi::WGPUBindGroupLayoutBinding b = {};
    b.binding = binding.mBinding;
    b.visibility = binding.mVisibility;
    b.ty = ffi::WGPUBindingType(binding.mType);
    Unused << binding.mTextureComponentType;  // TODO
    b.texture_dimension =
        ffi::WGPUTextureViewDimension(binding.mTextureDimension);
    b.multisampled = binding.mMultisampled;
    b.dynamic = binding.mDynamic;
    bindings.AppendElement(b);
  }
  SerialBindGroupLayoutDescriptor desc = {std::move(bindings)};
  if (!SendDeviceCreateBindGroupLayout(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyBindGroupLayout(RawId aId) {
  SendBindGroupLayoutDestroy(aId);
  ffi::wgpu_client_kill_bind_group_layout_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreatePipelineLayout(
    RawId aSelfId, const dom::GPUPipelineLayoutDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_pipeline_layout_id(mClient, aSelfId);
  SerialPipelineLayoutDescriptor desc = {};
  for (const auto& layouts : aDesc.mBindGroupLayouts) {
    desc.mBindGroupLayouts.AppendElement(layouts->mId);
  }
  if (!SendDeviceCreatePipelineLayout(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyPipelineLayout(RawId aId) {
  SendPipelineLayoutDestroy(aId);
  ffi::wgpu_client_kill_pipeline_layout_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateBindGroup(
    RawId aSelfId, const dom::GPUBindGroupDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_bind_group_id(mClient, aSelfId);
  SerialBindGroupDescriptor desc = {};
  desc.mLayout = aDesc.mLayout->mId;
  for (const auto& binding : aDesc.mBindings) {
    SerialBindGroupBinding bd = {};
    bd.mBinding = binding.mBinding;
    if (binding.mResource.IsGPUBufferBinding()) {
      bd.mType = SerialBindGroupBindingType::Buffer;
      const auto& bufBinding = binding.mResource.GetAsGPUBufferBinding();
      bd.mValue = bufBinding.mBuffer->mId;
      bd.mBufferOffset = bufBinding.mOffset;
      bd.mBufferSize =
          bufBinding.mSize.WasPassed() ? bufBinding.mSize.Value() : 0;
    }
    if (binding.mResource.IsGPUTextureView()) {
      bd.mType = SerialBindGroupBindingType::Texture;
      bd.mValue = binding.mResource.GetAsGPUTextureView()->mId;
    }
    if (binding.mResource.IsGPUSampler()) {
      bd.mType = SerialBindGroupBindingType::Sampler;
      bd.mValue = binding.mResource.GetAsGPUSampler()->mId;
    }
    desc.mBindings.AppendElement(bd);
  }
  if (!SendDeviceCreateBindGroup(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyBindGroup(RawId aId) {
  SendBindGroupDestroy(aId);
  ffi::wgpu_client_kill_bind_group_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateShaderModule(
    RawId aSelfId, const dom::GPUShaderModuleDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_shader_module_id(mClient, aSelfId);
  MOZ_ASSERT(aDesc.mCode.IsUint32Array());
  const auto& code = aDesc.mCode.GetAsUint32Array();
  code.ComputeState();
  nsTArray<uint32_t> data(code.Length());
  data.AppendElements(code.Data(), code.Length());
  if (!SendDeviceCreateShaderModule(aSelfId, data, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyShaderModule(RawId aId) {
  SendShaderModuleDestroy(aId);
  ffi::wgpu_client_kill_shader_module_id(mClient, aId);
}

RawId WebGPUChild::DeviceCreateComputePipeline(
    RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_compute_pipeline_id(mClient, aSelfId);
  SerialProgrammableStageDescriptor stage = {};
  stage.mModule = aDesc.mComputeStage.mModule->mId;
  stage.mEntryPoint = aDesc.mComputeStage.mEntryPoint;
  SerialComputePipelineDescriptor desc = {aDesc.mLayout->mId, stage};
  if (!SendDeviceCreateComputePipeline(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::DestroyComputePipeline(RawId aId) {
  SendComputePipelineDestroy(aId);
  ffi::wgpu_client_kill_compute_pipeline_id(mClient, aId);
}

void WebGPUChild::QueueSubmit(RawId aSelfId,
                              const nsTArray<RawId>& aCommandBufferIds) {
  SendQueueSubmit(aSelfId, aCommandBufferIds);
  for (const auto& cur : aCommandBufferIds) {
    ffi::wgpu_client_kill_encoder_id(mClient, cur);
  }
}

void WebGPUChild::DestroyRenderPipeline(RawId aId) {
  SendRenderPipelineDestroy(aId);
  ffi::wgpu_client_kill_render_pipeline_id(mClient, aId);
}

}  // namespace webgpu
}  // namespace mozilla
