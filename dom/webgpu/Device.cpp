/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "Device.h"
#include "CommandEncoder.h"
#include "BindGroup.h"

#include "Adapter.h"
#include "Buffer.h"
#include "CompilationInfo.h"
#include "ComputePipeline.h"
#include "DeviceLostInfo.h"
#include "InternalError.h"
#include "OutOfMemoryError.h"
#include "PipelineLayout.h"
#include "Queue.h"
#include "RenderBundleEncoder.h"
#include "RenderPipeline.h"
#include "Sampler.h"
#include "SupportedFeatures.h"
#include "SupportedLimits.h"
#include "Texture.h"
#include "TextureView.h"
#include "ValidationError.h"
#include "ipc/WebGPUChild.h"
#include "Utility.h"
#include "nsGlobalWindowInner.h"

namespace mozilla::webgpu {

mozilla::LazyLogModule gWebGPULog("WebGPU");

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(Device, DOMEventTargetHelper,
                                                 mBridge, mQueue, mFeatures,
                                                 mLimits, mLostPromise);
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(Device, DOMEventTargetHelper)
GPU_IMPL_JS_WRAP(Device)

/* static */ CheckedInt<uint32_t> Device::BufferStrideWithMask(
    const gfx::IntSize& aSize, const gfx::SurfaceFormat& aFormat) {
  constexpr uint32_t kBufferAlignmentMask = 0xff;
  return CheckedInt<uint32_t>(aSize.width) * gfx::BytesPerPixel(aFormat) +
         kBufferAlignmentMask;
}

RefPtr<WebGPUChild> Device::GetBridge() { return mBridge; }

Device::Device(Adapter* const aParent, RawId aId,
               const ffi::WGPULimits& aRawLimits)
    : DOMEventTargetHelper(aParent->GetParentObject()),
      mId(aId),
      // features are filled in Adapter::RequestDevice
      mFeatures(new SupportedFeatures(aParent)),
      mLimits(new SupportedLimits(aParent, aRawLimits)),
      mBridge(aParent->mBridge),
      mQueue(new class Queue(this, aParent->mBridge, aId)) {
  mBridge->RegisterDevice(this);
}

Device::~Device() { Cleanup(); }

void Device::Cleanup() {
  if (!mValid) {
    return;
  }

  mValid = false;

  if (mBridge) {
    mBridge->UnregisterDevice(mId);
  }
}

void Device::CleanupUnregisteredInParent() {
  if (mBridge) {
    mBridge->FreeUnregisteredInParentDevice(mId);
  }
  mValid = false;
}

bool Device::IsLost() const {
  return !mBridge || !mBridge->CanSend() ||
         (mLostPromise &&
          (mLostPromise->State() != dom::Promise::PromiseState::Pending));
}

bool Device::IsBridgeAlive() const { return mBridge && mBridge->CanSend(); }

// Generate an error on the Device timeline for this device.
//
// aMessage is interpreted as UTF-8.
void Device::GenerateValidationError(const nsCString& aMessage) {
  if (!IsBridgeAlive()) {
    return;  // Just drop it?
  }
  mBridge->SendGenerateError(Some(mId), dom::GPUErrorFilter::Validation,
                             aMessage);
}

void Device::TrackBuffer(Buffer* aBuffer) { mTrackedBuffers.Insert(aBuffer); }

void Device::UntrackBuffer(Buffer* aBuffer) { mTrackedBuffers.Remove(aBuffer); }

void Device::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void Device::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

dom::Promise* Device::GetLost(ErrorResult& aRv) {
  aRv = NS_OK;
  if (!mLostPromise) {
    mLostPromise = dom::Promise::Create(GetParentObject(), aRv);
    if (mLostPromise && !mBridge->CanSend()) {
      auto info = MakeRefPtr<DeviceLostInfo>(GetParentObject(),
                                             u"WebGPUChild destroyed"_ns);
      mLostPromise->MaybeResolve(info);
    }
  }
  return mLostPromise;
}

void Device::ResolveLost(Maybe<dom::GPUDeviceLostReason> aReason,
                         const nsAString& aMessage) {
  IgnoredErrorResult rv;
  dom::Promise* lostPromise = GetLost(rv);
  if (!lostPromise) {
    // Promise doesn't exist? Maybe out of memory.
    return;
  }
  if (lostPromise->State() != dom::Promise::PromiseState::Pending) {
    // lostPromise was already resolved or rejected.
    return;
  }
  if (!lostPromise->PromiseObj()) {
    // The underlying JS object is gone.
    return;
  }

  RefPtr<DeviceLostInfo> info;
  if (aReason.isSome()) {
    info = MakeRefPtr<DeviceLostInfo>(GetParentObject(), *aReason, aMessage);
  } else {
    info = MakeRefPtr<DeviceLostInfo>(GetParentObject(), aMessage);
  }
  lostPromise->MaybeResolve(info);
}

already_AddRefed<Buffer> Device::CreateBuffer(
    const dom::GPUBufferDescriptor& aDesc, ErrorResult& aRv) {
  return Buffer::Create(this, mId, aDesc, aRv);
}

already_AddRefed<Texture> Device::CreateTextureForSwapChain(
    const dom::GPUCanvasConfiguration* const aConfig,
    const gfx::IntSize& aCanvasSize, layers::RemoteTextureOwnerId aOwnerId) {
  MOZ_ASSERT(aConfig);

  dom::GPUTextureDescriptor desc;
  desc.mDimension = dom::GPUTextureDimension::_2d;
  auto& sizeDict = desc.mSize.SetAsGPUExtent3DDict();
  sizeDict.mWidth = aCanvasSize.width;
  sizeDict.mHeight = aCanvasSize.height;
  sizeDict.mDepthOrArrayLayers = 1;
  desc.mFormat = aConfig->mFormat;
  desc.mMipLevelCount = 1;
  desc.mSampleCount = 1;
  desc.mUsage = aConfig->mUsage | dom::GPUTextureUsage_Binding::COPY_SRC;
  desc.mViewFormats = aConfig->mViewFormats;

  return CreateTexture(desc, Some(aOwnerId));
}

already_AddRefed<Texture> Device::CreateTexture(
    const dom::GPUTextureDescriptor& aDesc) {
  return CreateTexture(aDesc, /* aOwnerId */ Nothing());
}

already_AddRefed<Texture> Device::CreateTexture(
    const dom::GPUTextureDescriptor& aDesc,
    Maybe<layers::RemoteTextureOwnerId> aOwnerId) {
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

  AutoTArray<ffi::WGPUTextureFormat, 8> viewFormats;
  for (auto format : aDesc.mViewFormats) {
    viewFormats.AppendElement(ConvertTextureFormat(format));
  }
  desc.view_formats = {viewFormats.Elements(), viewFormats.Length()};

  Maybe<ffi::WGPUSwapChainId> ownerId;
  if (aOwnerId.isSome()) {
    ownerId = Some(ffi::WGPUSwapChainId{aOwnerId->mId});
  }

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_texture(
      mBridge->GetClient(), mId, &desc, ownerId.ptrOr(nullptr), ToFFI(&bb));

  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<Texture> texture = new Texture(this, id, aDesc);
  return texture.forget();
}

already_AddRefed<Sampler> Device::CreateSampler(
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

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_sampler(mBridge->GetClient(), mId, &desc,
                                             ToFFI(&bb));

  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<Sampler> sampler = new Sampler(this, id);
  return sampler.forget();
}

already_AddRefed<CommandEncoder> Device::CreateCommandEncoder(
    const dom::GPUCommandEncoderDescriptor& aDesc) {
  ffi::WGPUCommandEncoderDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_command_encoder(mBridge->GetClient(), mId,
                                                     &desc, ToFFI(&bb));
  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<CommandEncoder> encoder = new CommandEncoder(this, mBridge, id);
  return encoder.forget();
}

already_AddRefed<RenderBundleEncoder> Device::CreateRenderBundleEncoder(
    const dom::GPURenderBundleEncoderDescriptor& aDesc) {
  RefPtr<RenderBundleEncoder> encoder =
      new RenderBundleEncoder(this, mBridge, aDesc);
  return encoder.forget();
}

already_AddRefed<BindGroupLayout> Device::CreateBindGroupLayout(
    const dom::GPUBindGroupLayoutDescriptor& aDesc) {
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

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_bind_group_layout(mBridge->GetClient(),
                                                       mId, &desc, ToFFI(&bb));
  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<BindGroupLayout> object = new BindGroupLayout(this, id, true);
  return object.forget();
}

already_AddRefed<PipelineLayout> Device::CreatePipelineLayout(
    const dom::GPUPipelineLayoutDescriptor& aDesc) {
  nsTArray<ffi::WGPUBindGroupLayoutId> bindGroupLayouts(
      aDesc.mBindGroupLayouts.Length());

  for (const auto& layout : aDesc.mBindGroupLayouts) {
    bindGroupLayouts.AppendElement(layout->mId);
  }

  ffi::WGPUPipelineLayoutDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
  desc.bind_group_layouts = bindGroupLayouts.Elements();
  desc.bind_group_layouts_length = bindGroupLayouts.Length();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_pipeline_layout(mBridge->GetClient(), mId,
                                                     &desc, ToFFI(&bb));
  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<PipelineLayout> object = new PipelineLayout(this, id);
  return object.forget();
}

already_AddRefed<BindGroup> Device::CreateBindGroup(
    const dom::GPUBindGroupDescriptor& aDesc) {
  nsTArray<ffi::WGPUBindGroupEntry> entries(aDesc.mEntries.Length());
  for (const auto& entry : aDesc.mEntries) {
    ffi::WGPUBindGroupEntry e = {};
    e.binding = entry.mBinding;
    if (entry.mResource.IsGPUBufferBinding()) {
      const auto& bufBinding = entry.mResource.GetAsGPUBufferBinding();
      if (!bufBinding.mBuffer->mId) {
        NS_WARNING("Buffer binding has no id -- ignoring.");
        continue;
      }
      e.buffer = bufBinding.mBuffer->mId;
      e.offset = bufBinding.mOffset;
      e.size = bufBinding.mSize.WasPassed() ? bufBinding.mSize.Value() : 0;
    } else if (entry.mResource.IsGPUTextureView()) {
      e.texture_view = entry.mResource.GetAsGPUTextureView()->mId;
    } else if (entry.mResource.IsGPUSampler()) {
      e.sampler = entry.mResource.GetAsGPUSampler()->mId;
    } else {
      // Not a buffer, nor a texture view, nor a sampler. If we pass
      // this to wgpu_client, it'll panic. Log a warning instead and
      // ignore this entry.
      NS_WARNING("Bind group entry has unknown type.");
      continue;
    }
    entries.AppendElement(e);
  }

  ffi::WGPUBindGroupDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();
  desc.layout = aDesc.mLayout->mId;
  desc.entries = entries.Elements();
  desc.entries_length = entries.Length();

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_bind_group(mBridge->GetClient(), mId,
                                                &desc, ToFFI(&bb));
  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<BindGroup> object = new BindGroup(this, id);
  return object.forget();
}

MOZ_CAN_RUN_SCRIPT void reportCompilationMessagesToConsole(
    const RefPtr<ShaderModule>& aShaderModule,
    const nsTArray<WebGPUCompilationMessage>& aMessages) {
  auto* global = aShaderModule->GetParentObject();

  dom::AutoJSAPI api;
  if (!api.Init(global)) {
    return;
  }

  const auto& cx = api.cx();

  ErrorResult rv;
  RefPtr<dom::Console> console =
      nsGlobalWindowInner::Cast(global->GetAsInnerWindow())->GetConsole(cx, rv);
  if (rv.Failed()) {
    return;
  }

  dom::GlobalObject globalObj(cx, global->GetGlobalJSObject());

  dom::Sequence<JS::Value> args;
  dom::SequenceRooter<JS::Value> msgArgsRooter(cx, &args);
  auto SetSingleStrAsArgs =
      [&](const nsString& message, dom::Sequence<JS::Value>* args)
          MOZ_CAN_RUN_SCRIPT {
            args->Clear();
            JS::Rooted<JSString*> jsStr(
                cx, JS_NewUCStringCopyN(cx, message.Data(), message.Length()));
            if (!jsStr) {
              return;
            }
            JS::Rooted<JS::Value> val(cx, JS::StringValue(jsStr));
            if (!args->AppendElement(val, fallible)) {
              return;
            }
          };

  nsString label;
  aShaderModule->GetLabel(label);
  auto appendNiceLabelIfPresent = [&label](nsString* buf) MOZ_CAN_RUN_SCRIPT {
    if (!label.IsEmpty()) {
      buf->AppendLiteral(u" \"");
      buf->Append(label);
      buf->AppendLiteral(u"\"");
    }
  };

  // We haven't actually inspected a message for severity, but
  // it doesn't actually matter, since we don't do anything at
  // this level.
  auto highestSeveritySeen = WebGPUCompilationMessageType::Info;
  uint64_t errorCount = 0;
  uint64_t warningCount = 0;
  uint64_t infoCount = 0;
  for (const auto& message : aMessages) {
    bool higherThanSeen =
        static_cast<std::underlying_type_t<WebGPUCompilationMessageType>>(
            message.messageType) <
        static_cast<std::underlying_type_t<WebGPUCompilationMessageType>>(
            highestSeveritySeen);
    if (higherThanSeen) {
      highestSeveritySeen = message.messageType;
    }
    switch (message.messageType) {
      case WebGPUCompilationMessageType::Error:
        errorCount += 1;
        break;
      case WebGPUCompilationMessageType::Warning:
        warningCount += 1;
        break;
      case WebGPUCompilationMessageType::Info:
        infoCount += 1;
        break;
    }
  }
  switch (highestSeveritySeen) {
    case WebGPUCompilationMessageType::Info:
      // shouldn't happen, but :shrug:
      break;
    case WebGPUCompilationMessageType::Warning: {
      nsString msg(
          u"Encountered one or more warnings while creating shader module");
      appendNiceLabelIfPresent(&msg);
      SetSingleStrAsArgs(msg, &args);
      console->Warn(globalObj, args);
      break;
    }
    case WebGPUCompilationMessageType::Error: {
      nsString msg(
          u"Encountered one or more errors while creating shader module");
      appendNiceLabelIfPresent(&msg);
      SetSingleStrAsArgs(msg, &args);
      console->Error(globalObj, args);
      break;
    }
  }

  nsString header;
  header.AppendLiteral(u"WebGPU compilation info for shader module");
  appendNiceLabelIfPresent(&header);
  header.AppendLiteral(u" (");
  header.AppendInt(errorCount);
  header.AppendLiteral(u" error(s), ");
  header.AppendInt(warningCount);
  header.AppendLiteral(u" warning(s), ");
  header.AppendInt(infoCount);
  header.AppendLiteral(u" info)");
  SetSingleStrAsArgs(header, &args);
  console->GroupCollapsed(globalObj, args);

  for (const auto& message : aMessages) {
    SetSingleStrAsArgs(message.message, &args);
    switch (message.messageType) {
      case WebGPUCompilationMessageType::Error:
        console->Error(globalObj, args);
        break;
      case WebGPUCompilationMessageType::Warning:
        console->Warn(globalObj, args);
        break;
      case WebGPUCompilationMessageType::Info:
        console->Info(globalObj, args);
        break;
    }
  }
  console->GroupEnd(globalObj);
}

already_AddRefed<ShaderModule> Device::CreateShaderModule(
    JSContext* aCx, const dom::GPUShaderModuleDescriptor& aDesc,
    ErrorResult& aRv) {
  Unused << aCx;

  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RawId moduleId =
      ffi::wgpu_client_make_shader_module_id(mBridge->GetClient(), mId);

  RefPtr<ShaderModule> shaderModule = new ShaderModule(this, moduleId, promise);

  shaderModule->SetLabel(aDesc.mLabel);

  RefPtr<Device> device = this;

  if (mBridge->CanSend()) {
    mBridge
        ->SendDeviceCreateShaderModule(mId, moduleId, aDesc.mLabel, aDesc.mCode)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [promise, device,
             shaderModule](nsTArray<WebGPUCompilationMessage>&& messages)
                MOZ_CAN_RUN_SCRIPT {
                  if (!messages.IsEmpty()) {
                    reportCompilationMessagesToConsole(shaderModule,
                                                       std::cref(messages));
                  }
                  RefPtr<CompilationInfo> infoObject(
                      new CompilationInfo(device));
                  infoObject->SetMessages(messages);
                  promise->MaybeResolve(infoObject);
                },
            [promise](const ipc::ResponseRejectReason& aReason) {
              promise->MaybeRejectWithNotSupportedError("IPC error");
            });
  } else {
    promise->MaybeRejectWithNotSupportedError("IPC error");
  }

  return shaderModule.forget();
}

RawId CreateComputePipelineImpl(PipelineCreationContext* const aContext,
                                WebGPUChild* aBridge,
                                const dom::GPUComputePipelineDescriptor& aDesc,
                                ipc::ByteBuf* const aByteBuf) {
  ffi::WGPUComputePipelineDescriptor desc = {};
  nsCString entryPoint;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  if (aDesc.mLayout.IsGPUAutoLayoutMode()) {
    desc.layout = 0;
  } else if (aDesc.mLayout.IsGPUPipelineLayout()) {
    desc.layout = aDesc.mLayout.GetAsGPUPipelineLayout()->mId;
  } else {
    MOZ_ASSERT_UNREACHABLE();
  }
  desc.stage.module = aDesc.mCompute.mModule->mId;
  CopyUTF16toUTF8(aDesc.mCompute.mEntryPoint, entryPoint);
  desc.stage.entry_point = entryPoint.get();

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_compute_pipeline(
      aBridge->GetClient(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

RawId CreateRenderPipelineImpl(PipelineCreationContext* const aContext,
                               WebGPUChild* aBridge,
                               const dom::GPURenderPipelineDescriptor& aDesc,
                               ipc::ByteBuf* const aByteBuf) {
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

  if (aDesc.mLayout.IsGPUAutoLayoutMode()) {
    desc.layout = 0;
  } else if (aDesc.mLayout.IsGPUPipelineLayout()) {
    desc.layout = aDesc.mLayout.GetAsGPUPipelineLayout()->mId;
  } else {
    MOZ_ASSERT_UNREACHABLE();
  }

  {
    const auto& stage = aDesc.mVertex;
    vertexState.stage.module = stage.mModule->mId;
    CopyUTF16toUTF8(stage.mEntryPoint, vsEntry);
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
    CopyUTF16toUTF8(stage.mEntryPoint, fsEntry);
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
    desc.primitive.unclipped_depth = prim.mUnclippedDepth;
  }
  desc.multisample = ConvertMultisampleState(aDesc.mMultisample);

  ffi::WGPUDepthStencilState depthStencilState = {};
  if (aDesc.mDepthStencil.WasPassed()) {
    depthStencilState = ConvertDepthStencilState(aDesc.mDepthStencil.Value());
    desc.depth_stencil = &depthStencilState;
  }

  RawId implicit_bgl_ids[WGPUMAX_BIND_GROUPS] = {};
  RawId id = ffi::wgpu_client_create_render_pipeline(
      aBridge->GetClient(), aContext->mParentId, &desc, ToFFI(aByteBuf),
      &aContext->mImplicitPipelineLayoutId, implicit_bgl_ids);

  for (const auto& cur : implicit_bgl_ids) {
    if (!cur) break;
    aContext->mImplicitBindGroupLayoutIds.AppendElement(cur);
  }

  return id;
}

already_AddRefed<ComputePipeline> Device::CreateComputePipeline(
    const dom::GPUComputePipelineDescriptor& aDesc) {
  PipelineCreationContext context = {mId};
  ipc::ByteBuf bb;
  RawId id = CreateComputePipelineImpl(&context, mBridge, aDesc, &bb);

  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<ComputePipeline> object =
      new ComputePipeline(this, id, context.mImplicitPipelineLayoutId,
                          std::move(context.mImplicitBindGroupLayoutIds));
  return object.forget();
}

already_AddRefed<RenderPipeline> Device::CreateRenderPipeline(
    const dom::GPURenderPipelineDescriptor& aDesc) {
  PipelineCreationContext context = {mId};
  ipc::ByteBuf bb;
  RawId id = CreateRenderPipelineImpl(&context, mBridge, aDesc, &bb);

  if (mBridge->CanSend()) {
    mBridge->SendDeviceAction(mId, std::move(bb));
  }

  RefPtr<RenderPipeline> object =
      new RenderPipeline(this, id, context.mImplicitPipelineLayoutId,
                         std::move(context.mImplicitBindGroupLayoutIds));
  return object.forget();
}

already_AddRefed<dom::Promise> Device::CreateComputePipelineAsync(
    const dom::GPUComputePipelineDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  std::shared_ptr<PipelineCreationContext> context(
      new PipelineCreationContext());
  context->mParentId = mId;

  ipc::ByteBuf bb;
  RawId pipelineId =
      CreateComputePipelineImpl(context.get(), mBridge, aDesc, &bb);

  if (mBridge->CanSend()) {
    mBridge->SendDeviceActionWithAck(mId, std::move(bb))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [self = RefPtr{this}, context, pipelineId, promise](bool aDummy) {
              Unused << aDummy;
              RefPtr<ComputePipeline> object = new ComputePipeline(
                  self, pipelineId, context->mImplicitPipelineLayoutId,
                  std::move(context->mImplicitBindGroupLayoutIds));
              promise->MaybeResolve(object);
            },
            [promise](const ipc::ResponseRejectReason&) {
              promise->MaybeRejectWithOperationError(
                  "Internal communication error");
            });
  } else {
    promise->MaybeRejectWithOperationError("Internal communication error");
  }

  return promise.forget();
}

already_AddRefed<dom::Promise> Device::CreateRenderPipelineAsync(
    const dom::GPURenderPipelineDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  std::shared_ptr<PipelineCreationContext> context(
      new PipelineCreationContext());
  context->mParentId = mId;

  ipc::ByteBuf bb;
  RawId pipelineId =
      CreateRenderPipelineImpl(context.get(), mBridge, aDesc, &bb);

  if (mBridge->CanSend()) {
    mBridge->SendDeviceActionWithAck(mId, std::move(bb))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [self = RefPtr{this}, context, promise, pipelineId](bool aDummy) {
              Unused << aDummy;
              RefPtr<RenderPipeline> object = new RenderPipeline(
                  self, pipelineId, context->mImplicitPipelineLayoutId,
                  std::move(context->mImplicitBindGroupLayoutIds));
              promise->MaybeResolve(object);
            },
            [promise](const ipc::ResponseRejectReason&) {
              promise->MaybeRejectWithOperationError(
                  "Internal communication error");
            });
  } else {
    promise->MaybeRejectWithOperationError("Internal communication error");
  }

  return promise.forget();
}

already_AddRefed<Texture> Device::InitSwapChain(
    const dom::GPUCanvasConfiguration* const aConfig,
    const layers::RemoteTextureOwnerId aOwnerId,
    bool aUseExternalTextureInSwapChain, gfx::SurfaceFormat aFormat,
    gfx::IntSize aCanvasSize) {
  MOZ_ASSERT(aConfig);

  if (!mBridge->CanSend()) {
    return nullptr;
  }

  // Check that aCanvasSize and aFormat will generate a texture stride
  // within limits.
  const auto bufferStrideWithMask = BufferStrideWithMask(aCanvasSize, aFormat);
  if (!bufferStrideWithMask.isValid()) {
    return nullptr;
  }

  const layers::RGBDescriptor rgbDesc(aCanvasSize, aFormat);
  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  mBridge->DeviceCreateSwapChain(mId, rgbDesc, maxBufferCount, aOwnerId,
                                 aUseExternalTextureInSwapChain);

  // TODO: `mColorSpace`: <https://bugzilla.mozilla.org/show_bug.cgi?id=1846608>
  // TODO: `mAlphaMode`: <https://bugzilla.mozilla.org/show_bug.cgi?id=1846605>
  return CreateTextureForSwapChain(aConfig, aCanvasSize, aOwnerId);
}

bool Device::CheckNewWarning(const nsACString& aMessage) {
  return mKnownWarnings.EnsureInserted(aMessage);
}

void Device::Destroy() {
  if (IsLost()) {
    return;
  }

  // Unmap all buffers from this device, as specified by
  // https://gpuweb.github.io/gpuweb/#dom-gpudevice-destroy.
  dom::AutoJSAPI jsapi;
  if (jsapi.Init(GetOwnerGlobal())) {
    IgnoredErrorResult rv;
    for (const auto& buffer : mTrackedBuffers) {
      buffer->Unmap(jsapi.cx(), rv);
    }

    mTrackedBuffers.Clear();
  }

  mBridge->SendDeviceDestroy(mId);
}

void Device::PushErrorScope(const dom::GPUErrorFilter& aFilter) {
  if (!IsBridgeAlive()) {
    return;
  }
  mBridge->SendDevicePushErrorScope(mId, aFilter);
}

already_AddRefed<dom::Promise> Device::PopErrorScope(ErrorResult& aRv) {
  /*
  https://www.w3.org/TR/webgpu/#errors-and-debugging:
  > After a device is lost (described below), errors are no longer surfaced.
  > At this point, implementations do not need to run validation or error
  tracking: > popErrorScope() and uncapturederror stop reporting errors, > and
  the validity of objects on the device becomes unobservable.
  */
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!IsBridgeAlive()) {
    WebGPUChild::JsWarning(
        GetOwnerGlobal(),
        "popErrorScope resolving to null because device is already lost."_ns);
    promise->MaybeResolve(JS::NullHandleValue);
    return promise.forget();
  }

  auto errorPromise = mBridge->SendDevicePopErrorScope(mId);

  errorPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, promise](const PopErrorScopeResult& aResult) {
        RefPtr<Error> error;

        switch (aResult.resultType) {
          case PopErrorScopeResultType::NoError:
            promise->MaybeResolve(JS::NullHandleValue);
            return;

          case PopErrorScopeResultType::DeviceLost:
            WebGPUChild::JsWarning(
                self->GetOwnerGlobal(),
                "popErrorScope resolving to null because device was lost."_ns);
            promise->MaybeResolve(JS::NullHandleValue);
            return;

          case PopErrorScopeResultType::ThrowOperationError:
            promise->MaybeRejectWithOperationError(aResult.message);
            return;

          case PopErrorScopeResultType::OutOfMemory:
            error =
                new OutOfMemoryError(self->GetParentObject(), aResult.message);
            break;

          case PopErrorScopeResultType::ValidationError:
            error =
                new ValidationError(self->GetParentObject(), aResult.message);
            break;

          case PopErrorScopeResultType::InternalError:
            error = new InternalError(self->GetParentObject(), aResult.message);
            break;
        }
        promise->MaybeResolve(std::move(error));
      },
      [self = RefPtr{this}, promise](const ipc::ResponseRejectReason&) {
        // Device was lost.
        WebGPUChild::JsWarning(
            self->GetOwnerGlobal(),
            "popErrorScope resolving to null because device was just lost."_ns);
        promise->MaybeResolve(JS::NullHandleValue);
      });

  return promise.forget();
}

}  // namespace mozilla::webgpu
