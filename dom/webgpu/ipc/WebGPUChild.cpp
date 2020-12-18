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
          GetCurrentSerialEventTarget(), __func__,
          [](const RawId& aId) {
            return aId == 0 ? RawIdPromise::CreateAndReject(Nothing(), __func__)
                            : RawIdPromise::CreateAndResolve(aId, __func__);
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
    desc.size.depth = seq.Length() > 2 ? seq[2] : 1;
  } else if (aDesc.mSize.IsGPUExtent3DDict()) {
    const auto& dict = aDesc.mSize.GetAsGPUExtent3DDict();
    desc.size.width = dict.mWidth;
    desc.size.height = dict.mHeight;
    desc.size.depth = dict.mDepth;
  } else {
    MOZ_CRASH("Unexpected union");
  }
  desc.mip_level_count = aDesc.mMipLevelCount;
  desc.sample_count = aDesc.mSampleCount;
  desc.dimension = ffi::WGPUTextureDimension(aDesc.mDimension);
  desc.format = ffi::WGPUTextureFormat(aDesc.mFormat);
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
    format = ffi::WGPUTextureFormat(aDesc.mFormat.Value());
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
  desc.level_count =
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
    if (entry.mViewDimension.WasPassed()) {
      data.dim = ffi::WGPUTextureViewDimension(entry.mViewDimension.Value());
    }
    if (entry.mTextureComponentType.WasPassed()) {
      switch (entry.mTextureComponentType.Value()) {
        case dom::GPUTextureComponentType::Float:
          data.type = ffi::WGPURawTextureSampleType_Float;
          break;
        case dom::GPUTextureComponentType::Uint:
          data.type = ffi::WGPURawTextureSampleType_Uint;
          break;
        case dom::GPUTextureComponentType::Sint:
          data.type = ffi::WGPURawTextureSampleType_Sint;
          break;
        case dom::GPUTextureComponentType::Depth_comparison:
          data.type = ffi::WGPURawTextureSampleType_Depth;
          break;
        default:
          MOZ_ASSERT_UNREACHABLE();
          break;
      }
    }
    if (entry.mStorageTextureFormat.WasPassed()) {
      data.format = ffi::WGPUTextureFormat(entry.mStorageTextureFormat.Value());
    }
    optional.AppendElement(data);
  }

  nsTArray<ffi::WGPUBindGroupLayoutEntry> entries(aDesc.mEntries.Length());
  for (size_t i = 0; i < aDesc.mEntries.Length(); ++i) {
    const auto& entry = aDesc.mEntries[i];
    ffi::WGPUBindGroupLayoutEntry e = {};
    e.binding = entry.mBinding;
    e.visibility = entry.mVisibility;
    e.ty = ffi::WGPURawBindingType(entry.mType);
    e.multisampled = entry.mMultisampled;
    e.has_dynamic_offset = entry.mHasDynamicOffset;
    if (entry.mViewDimension.WasPassed()) {
      e.view_dimension = &optional[i].dim;
    }
    if (entry.mTextureComponentType.WasPassed()) {
      e.texture_sample_type = &optional[i].type;
    }
    if (entry.mStorageTextureFormat.WasPassed()) {
      e.storage_texture_format = &optional[i].format;
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
  if (aDesc.mCode.IsString()) {
    LossyCopyUTF16toASCII(aDesc.mCode.GetAsString(), wgsl);
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
  desc.compute_stage.module = aDesc.mComputeStage.mModule->mId;
  LossyCopyUTF16toASCII(aDesc.mComputeStage.mEntryPoint, entryPoint);
  desc.compute_stage.entry_point = entryPoint.get();

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
  desc.alpha_blend = ConvertBlendDescriptor(aDesc.mAlphaBlend);
  desc.color_blend = ConvertBlendDescriptor(aDesc.mColorBlend);
  desc.write_mask = aDesc.mWriteMask;
  return desc;
}

static ffi::WGPUStencilStateFaceDescriptor ConvertStencilFaceDescriptor(
    const dom::GPUStencilStateFaceDescriptor& aDesc) {
  ffi::WGPUStencilStateFaceDescriptor desc = {};
  desc.compare = ConvertCompareFunction(aDesc.mCompare);
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
  desc.depth_compare = ConvertCompareFunction(aDesc.mDepthCompare);
  desc.stencil.front = ConvertStencilFaceDescriptor(aDesc.mStencilFront);
  desc.stencil.back = ConvertStencilFaceDescriptor(aDesc.mStencilBack);
  desc.stencil.read_mask = aDesc.mStencilReadMask;
  desc.stencil.write_mask = aDesc.mStencilWriteMask;
  return desc;
}

RawId WebGPUChild::DeviceCreateRenderPipeline(
    RawId aSelfId, const dom::GPURenderPipelineDescriptor& aDesc,
    nsTArray<RawId>* const aImplicitBindGroupLayoutIds) {
  ffi::WGPURenderPipelineDescriptor desc = {};
  nsCString label, vsEntry, fsEntry;
  ffi::WGPUProgrammableStageDescriptor vertexStage = {};
  ffi::WGPUProgrammableStageDescriptor fragmentStage = {};

  if (aDesc.mLabel.WasPassed()) {
    LossyCopyUTF16toASCII(aDesc.mLabel.Value(), label);
    desc.label = label.get();
  }
  if (aDesc.mLayout.WasPassed()) {
    desc.layout = aDesc.mLayout.Value().mId;
  }

  vertexStage.module = aDesc.mVertexStage.mModule->mId;
  LossyCopyUTF16toASCII(aDesc.mVertexStage.mEntryPoint, vsEntry);
  vertexStage.entry_point = vsEntry.get();
  desc.vertex_stage = &vertexStage;

  if (aDesc.mFragmentStage.WasPassed()) {
    const auto& stage = aDesc.mFragmentStage.Value();
    fragmentStage.module = stage.mModule->mId;
    LossyCopyUTF16toASCII(stage.mEntryPoint, fsEntry);
    fragmentStage.entry_point = fsEntry.get();
    desc.fragment_stage = &fragmentStage;
  }

  desc.primitive_topology =
      ffi::WGPUPrimitiveTopology(aDesc.mPrimitiveTopology);
  const auto rasterization =
      ConvertRasterizationDescriptor(aDesc.mRasterizationState);
  desc.rasterization_state = &rasterization;

  nsTArray<ffi::WGPUColorStateDescriptor> colorStates;
  for (const auto& colorState : aDesc.mColorStates) {
    colorStates.AppendElement(ConvertColorDescriptor(colorState));
  }
  desc.color_states = colorStates.Elements();
  desc.color_states_length = colorStates.Length();

  ffi::WGPUDepthStencilStateDescriptor depthStencilState = {};
  if (aDesc.mDepthStencilState.WasPassed()) {
    depthStencilState =
        ConvertDepthStencilDescriptor(aDesc.mDepthStencilState.Value());
    desc.depth_stencil_state = &depthStencilState;
  }

  desc.vertex_state.index_format =
      ffi::WGPUIndexFormat(aDesc.mVertexState.mIndexFormat);
  nsTArray<ffi::WGPUVertexBufferDescriptor> vertexBuffers;
  nsTArray<ffi::WGPUVertexAttributeDescriptor> vertexAttributes;
  for (const auto& vertex_desc : aDesc.mVertexState.mVertexBuffers) {
    ffi::WGPUVertexBufferDescriptor vb_desc = {};
    if (!vertex_desc.IsNull()) {
      const auto& vd = vertex_desc.Value();
      vb_desc.stride = vd.mArrayStride;
      vb_desc.step_mode = ffi::WGPUInputStepMode(vd.mStepMode);
      // Note: we are setting the length but not the pointer
      vb_desc.attributes_length = vd.mAttributes.Length();
      for (const auto& vat : vd.mAttributes) {
        ffi::WGPUVertexAttributeDescriptor ad = {};
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

  desc.vertex_state.vertex_buffers = vertexBuffers.Elements();
  desc.vertex_state.vertex_buffers_length = vertexBuffers.Length();
  desc.sample_count = aDesc.mSampleCount;
  desc.sample_mask = aDesc.mSampleMask;
  desc.alpha_to_coverage_enabled = aDesc.mAlphaToCoverageEnabled;

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
