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

  nsTArray<RawId> sharedIds(count);
  for (unsigned long i = 0; i != count; ++i) {
    sharedIds.AppendElement(ids[i]);
  }

  return SendInstanceRequestAdapter(aOptions, sharedIds)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [](const RawId& aId) {
            if (aId == 0) {
              return RawIdPromise::CreateAndReject(Nothing(), __func__);
            } else {
              return RawIdPromise::CreateAndResolve(aId, __func__);
            }
          },
          [](const ipc::ResponseRejectReason& aReason) {
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

RawId WebGPUChild::DeviceCreateBuffer(RawId aSelfId,
                                      const dom::GPUBufferDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_buffer_id(mClient, aSelfId);
  if (!SendDeviceCreateBuffer(aSelfId, aDesc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
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

RawId WebGPUChild::DeviceCreateSampler(RawId aSelfId,
                                       const dom::GPUSamplerDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_sampler_id(mClient, aSelfId);
  if (!SendDeviceCreateSampler(aSelfId, aDesc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
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

RawId WebGPUChild::DeviceCreateBindGroupLayout(
    RawId aSelfId, const dom::GPUBindGroupLayoutDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_bind_group_layout_id(mClient, aSelfId);
  nsTArray<ffi::WGPUBindGroupLayoutEntry> entries(aDesc.mEntries.Length());
  for (const auto& entry : aDesc.mEntries) {
    ffi::WGPUBindGroupLayoutEntry e = {};
    e.binding = entry.mBinding;
    e.visibility = entry.mVisibility;
    e.ty = ffi::WGPUBindingType(entry.mType);
    e.multisampled = entry.mMultisampled;
    e.has_dynamic_offset = entry.mHasDynamicOffset;
    e.view_dimension = ffi::WGPUTextureViewDimension(entry.mViewDimension);
    e.texture_component_type =
        ffi::WGPUTextureComponentType(entry.mTextureComponentType);
    e.storage_texture_format =
        entry.mStorageTextureFormat.WasPassed()
            ? ffi::WGPUTextureFormat(entry.mStorageTextureFormat.Value())
            : ffi::WGPUTextureFormat(0);
    entries.AppendElement(e);
  }
  SerialBindGroupLayoutDescriptor desc = {std::move(entries)};
  if (!SendDeviceCreateBindGroupLayout(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
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

RawId WebGPUChild::DeviceCreateBindGroup(
    RawId aSelfId, const dom::GPUBindGroupDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_bind_group_id(mClient, aSelfId);
  SerialBindGroupDescriptor desc = {};
  desc.mLayout = aDesc.mLayout->mId;
  for (const auto& entry : aDesc.mEntries) {
    SerialBindGroupEntry bd = {};
    bd.mBinding = entry.mBinding;
    if (entry.mResource.IsGPUBufferBinding()) {
      bd.mType = SerialBindGroupEntryType::Buffer;
      const auto& bufBinding = entry.mResource.GetAsGPUBufferBinding();
      bd.mValue = bufBinding.mBuffer->mId;
      bd.mBufferOffset = bufBinding.mOffset;
      bd.mBufferSize =
          bufBinding.mSize.WasPassed() ? bufBinding.mSize.Value() : 0;
    }
    if (entry.mResource.IsGPUTextureView()) {
      bd.mType = SerialBindGroupEntryType::Texture;
      bd.mValue = entry.mResource.GetAsGPUTextureView()->mId;
    }
    if (entry.mResource.IsGPUSampler()) {
      bd.mType = SerialBindGroupEntryType::Sampler;
      bd.mValue = entry.mResource.GetAsGPUSampler()->mId;
    }
    desc.mEntries.AppendElement(bd);
  }
  if (!SendDeviceCreateBindGroup(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
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

static SerialProgrammableStageDescriptor ConvertProgrammableStageDescriptor(
    const dom::GPUProgrammableStageDescriptor& aDesc) {
  SerialProgrammableStageDescriptor stage = {};
  stage.mModule = aDesc.mModule->mId;
  stage.mEntryPoint = aDesc.mEntryPoint;
  return stage;
}

RawId WebGPUChild::DeviceCreateComputePipeline(
    RawId aSelfId, const dom::GPUComputePipelineDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_compute_pipeline_id(mClient, aSelfId);
  const SerialComputePipelineDescriptor desc = {
      aDesc.mLayout->mId,
      ConvertProgrammableStageDescriptor(aDesc.mComputeStage),
  };
  if (!SendDeviceCreateComputePipeline(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

static ffi::WGPURasterizationStateDescriptor ConvertRasterizationDescriptor(
    const dom::GPURasterizationStateDescriptor& aDesc) {
  ffi::WGPURasterizationStateDescriptor desc = {};
  desc.front_face = ffi::WGPUFrontFace(aDesc.mFrontFace);
  desc.cull_mode = ffi::WGPUCullMode(aDesc.mCullMode);
  desc.depth_bias = aDesc.mDepthBias;
  desc.depth_bias_slope_scale = aDesc.mDepthBiasSlopeScale;
  desc.depth_bias_clamp = aDesc.mDepthBiasClamp;
  return desc;
}

static ffi::WGPUBlendDescriptor ConvertBlendDescriptor(
    const dom::GPUBlendDescriptor& aDesc) {
  ffi::WGPUBlendDescriptor desc = {};
  desc.src_factor = ffi::WGPUBlendFactor(aDesc.mSrcFactor);
  desc.dst_factor = ffi::WGPUBlendFactor(aDesc.mDstFactor);
  desc.operation = ffi::WGPUBlendOperation(aDesc.mOperation);
  return desc;
}

static ffi::WGPUColorStateDescriptor ConvertColorDescriptor(
    const dom::GPUColorStateDescriptor& aDesc) {
  ffi::WGPUColorStateDescriptor desc = {};
  desc.format = ffi::WGPUTextureFormat(aDesc.mFormat);
  const ffi::WGPUBlendDescriptor no_blend = {
      ffi::WGPUBlendFactor_One,
      ffi::WGPUBlendFactor_Zero,
      ffi::WGPUBlendOperation_Add,
  };
  desc.alpha_blend = aDesc.mAlpha.WasPassed()
                         ? ConvertBlendDescriptor(aDesc.mAlpha.Value())
                         : no_blend;
  desc.color_blend = aDesc.mColor.WasPassed()
                         ? ConvertBlendDescriptor(aDesc.mColor.Value())
                         : no_blend;
  desc.write_mask = aDesc.mWriteMask;
  return desc;
}

static ffi::WGPUStencilStateFaceDescriptor ConvertStencilFaceDescriptor(
    const dom::GPUStencilStateFaceDescriptor& aDesc) {
  ffi::WGPUStencilStateFaceDescriptor desc = {};
  desc.compare = ffi::WGPUCompareFunction(aDesc.mCompare);
  desc.fail_op = ffi::WGPUStencilOperation(aDesc.mFailOp);
  desc.depth_fail_op = ffi::WGPUStencilOperation(aDesc.mDepthFailOp);
  desc.pass_op = ffi::WGPUStencilOperation(aDesc.mPassOp);
  return desc;
}

static ffi::WGPUDepthStencilStateDescriptor ConvertDepthStencilDescriptor(
    const dom::GPUDepthStencilStateDescriptor& aDesc) {
  ffi::WGPUDepthStencilStateDescriptor desc = {};
  desc.format = ffi::WGPUTextureFormat(aDesc.mFormat);
  desc.depth_write_enabled = aDesc.mDepthWriteEnabled;
  desc.depth_compare = ffi::WGPUCompareFunction(aDesc.mDepthCompare);
  desc.stencil_front = ConvertStencilFaceDescriptor(aDesc.mStencilFront);
  desc.stencil_back = ConvertStencilFaceDescriptor(aDesc.mStencilBack);
  desc.stencil_read_mask = aDesc.mStencilReadMask;
  desc.stencil_write_mask = aDesc.mStencilWriteMask;
  return desc;
}

static ffi::WGPUVertexAttributeDescriptor ConvertVertexAttributeDescriptor(
    const dom::GPUVertexAttributeDescriptor& aDesc) {
  ffi::WGPUVertexAttributeDescriptor desc = {};
  desc.offset = aDesc.mOffset;
  desc.format = ffi::WGPUVertexFormat(aDesc.mFormat);
  desc.shader_location = aDesc.mShaderLocation;
  return desc;
}

static SerialVertexBufferLayoutDescriptor ConvertVertexBufferLayoutDescriptor(
    const dom::GPUVertexBufferLayoutDescriptor& aDesc) {
  SerialVertexBufferLayoutDescriptor desc = {};
  desc.mArrayStride = aDesc.mArrayStride;
  desc.mStepMode = ffi::WGPUInputStepMode(aDesc.mStepMode);
  for (const auto& vat : aDesc.mAttributeSet) {
    desc.mAttributeSet.AppendElement(ConvertVertexAttributeDescriptor(vat));
  }
  return desc;
}

RawId WebGPUChild::DeviceCreateRenderPipeline(
    RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_render_pipeline_id(mClient, aSelfId);
  SerialRenderPipelineDescriptor desc = {};
  desc.mLayout = aDesc.mLayout->mId;
  desc.mVertexStage = ConvertProgrammableStageDescriptor(aDesc.mVertexStage);
  if (aDesc.mFragmentStage.WasPassed()) {
    desc.mFragmentStage =
        ConvertProgrammableStageDescriptor(aDesc.mFragmentStage.Value());
  }
  desc.mPrimitiveTopology =
      ffi::WGPUPrimitiveTopology(aDesc.mPrimitiveTopology);
  if (aDesc.mRasterizationState.WasPassed()) {
    desc.mRasterizationState =
        Some(ConvertRasterizationDescriptor(aDesc.mRasterizationState.Value()));
  }
  for (const auto& color_state : aDesc.mColorStates) {
    desc.mColorStates.AppendElement(ConvertColorDescriptor(color_state));
  }
  if (aDesc.mDepthStencilState.WasPassed()) {
    desc.mDepthStencilState =
        Some(ConvertDepthStencilDescriptor(aDesc.mDepthStencilState.Value()));
  }
  desc.mVertexState.mIndexFormat =
      ffi::WGPUIndexFormat(aDesc.mVertexState.mIndexFormat);
  for (const auto& vertex_desc : aDesc.mVertexState.mVertexBuffers) {
    SerialVertexBufferLayoutDescriptor vb_desc = {};
    if (!vertex_desc.IsNull()) {
      vb_desc = ConvertVertexBufferLayoutDescriptor(vertex_desc.Value());
    }
    desc.mVertexState.mVertexBuffers.AppendElement(vb_desc);
  }
  desc.mSampleCount = aDesc.mSampleCount;
  desc.mSampleMask = aDesc.mSampleMask;
  desc.mAlphaToCoverageEnabled = aDesc.mAlphaToCoverageEnabled;
  if (!SendDeviceCreateRenderPipeline(aSelfId, desc, id)) {
    MOZ_CRASH("IPC failure");
  }
  return id;
}

void WebGPUChild::QueueSubmit(RawId aSelfId,
                              const nsTArray<RawId>& aCommandBufferIds) {
  SendQueueSubmit(aSelfId, aCommandBufferIds);
}

ipc::IPCResult WebGPUChild::RecvFreeAdapter(RawId id) {
  ffi::wgpu_client_kill_adapter_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeDevice(RawId id) {
  ffi::wgpu_client_kill_device_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreePipelineLayout(RawId id) {
  ffi::wgpu_client_kill_pipeline_layout_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeShaderModule(RawId id) {
  ffi::wgpu_client_kill_shader_module_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeBindGroupLayout(RawId id) {
  ffi::wgpu_client_kill_bind_group_layout_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeBindGroup(RawId id) {
  ffi::wgpu_client_kill_bind_group_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeCommandBuffer(RawId id) {
  ffi::wgpu_client_kill_encoder_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeRenderPipeline(RawId id) {
  ffi::wgpu_client_kill_render_pipeline_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeComputePipeline(RawId id) {
  ffi::wgpu_client_kill_compute_pipeline_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeBuffer(RawId id) {
  ffi::wgpu_client_kill_buffer_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeTexture(RawId id) {
  ffi::wgpu_client_kill_texture_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeTextureView(RawId id) {
  ffi::wgpu_client_kill_texture_view_id(mClient, id);
  return IPC_OK();
}
ipc::IPCResult WebGPUChild::RecvFreeSampler(RawId id) {
  ffi::wgpu_client_kill_sampler_id(mClient, id);
  return IPC_OK();
}

}  // namespace webgpu
}  // namespace mozilla
