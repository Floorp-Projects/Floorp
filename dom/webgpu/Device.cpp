/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ArrayBuffer.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "Device.h"
#include "CommandEncoder.h"
#include "BindGroup.h"

#include "Adapter.h"
#include "Buffer.h"
#include "ComputePipeline.h"
#include "DeviceLostInfo.h"
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

namespace mozilla::webgpu {

mozilla::LazyLogModule gWebGPULog("WebGPU");

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(Device, DOMEventTargetHelper,
                                                 mBridge, mQueue, mFeatures,
                                                 mLimits, mLostPromise);
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(Device, DOMEventTargetHelper)
GPU_IMPL_JS_WRAP(Device)

static void mapFreeCallback(void* aContents, void* aUserData) {
  Unused << aContents;
  Unused << aUserData;
}

RefPtr<WebGPUChild> Device::GetBridge() { return mBridge; }

JSObject* Device::CreateExternalArrayBuffer(JSContext* aCx, size_t aOffset,
                                            size_t aSize,
                                            const ipc::Shmem& aShmem) {
  MOZ_ASSERT(aOffset + aSize <= aShmem.Size<uint8_t>());
  return JS::NewExternalArrayBuffer(aCx, aSize, aShmem.get<uint8_t>() + aOffset,
                                    &mapFreeCallback, nullptr);
}

Device::Device(Adapter* const aParent, RawId aId,
               UniquePtr<ffi::WGPULimits> aRawLimits)
    : DOMEventTargetHelper(aParent->GetParentObject()),
      mId(aId),
      // features are filled in Adapter::RequestDevice
      mFeatures(new SupportedFeatures(aParent)),
      mLimits(new SupportedLimits(aParent, std::move(aRawLimits))),
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

  if (mLostPromise) {
    auto info = MakeRefPtr<DeviceLostInfo>(GetParentObject(),
                                           dom::GPUDeviceLostReason::Destroyed,
                                           u"Device destroyed"_ns);
    mLostPromise->MaybeResolve(info);
  }
}

void Device::CleanupUnregisteredInParent() {
  if (mBridge) {
    mBridge->FreeUnregisteredInParentDevice(mId);
  }
  mValid = false;
}

// Generate an error on the Device timeline for this device.
//
// aMessage is interpreted as UTF-8.
void Device::GenerateError(const nsCString& aMessage) {
  if (mBridge->CanSend()) {
    mBridge->SendGenerateError(mId, aMessage);
  }
}

void Device::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void Device::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

dom::Promise* Device::GetLost(ErrorResult& aRv) {
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

already_AddRefed<Buffer> Device::CreateBuffer(
    const dom::GPUBufferDescriptor& aDesc, ErrorResult& aRv) {
  if (!mBridge->CanSend()) {
    RefPtr<Buffer> buffer = new Buffer(this, 0, aDesc.mSize, 0);
    return buffer.forget();
  }

  ipc::Shmem shmem;
  bool hasMapFlags = aDesc.mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                                     dom::GPUBufferUsage_Binding::MAP_READ);
  if (hasMapFlags || aDesc.mMappedAtCreation) {
    const auto checked = CheckedInt<size_t>(aDesc.mSize);
    if (!checked.isValid()) {
      aRv.ThrowRangeError("Mappable size is too large");
      return nullptr;
    }
    const auto& size = checked.value();

    // TODO: use `ShmemPool`?
    if (!mBridge->AllocShmem(size, ipc::Shmem::SharedMemory::TYPE_BASIC,
                             &shmem)) {
      aRv.ThrowAbortError(
          nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, size));
      return nullptr;
    }

    // zero out memory
    memset(shmem.get<uint8_t>(), 0, size);
  }

  // If the buffer is not mapped at creation, and it has Shmem, we send it
  // to the GPU process. Otherwise, we keep it.
  RawId id = mBridge->DeviceCreateBuffer(mId, aDesc);
  RefPtr<Buffer> buffer = new Buffer(this, id, aDesc.mSize, aDesc.mUsage);
  if (aDesc.mMappedAtCreation) {
    buffer->SetMapped(std::move(shmem),
                      !(aDesc.mUsage & dom::GPUBufferUsage_Binding::MAP_READ));
  } else if (hasMapFlags) {
    mBridge->SendBufferReturnShmem(id, std::move(shmem));
  }

  return buffer.forget();
}

RefPtr<MappingPromise> Device::MapBufferAsync(RawId aId, uint32_t aMode,
                                              size_t aOffset, size_t aSize,
                                              ErrorResult& aRv) {
  ffi::WGPUHostMap mode;
  switch (aMode) {
    case dom::GPUMapMode_Binding::READ:
      mode = ffi::WGPUHostMap_Read;
      break;
    case dom::GPUMapMode_Binding::WRITE:
      mode = ffi::WGPUHostMap_Write;
      break;
    default:
      MOZ_CRASH("should have checked aMode in Buffer::MapAsync");
  }

  const CheckedInt<uint64_t> offset(aOffset);
  if (!offset.isValid()) {
    aRv.ThrowRangeError("Mapped offset is too large");
    return nullptr;
  }
  const CheckedInt<uint64_t> size(aSize);
  if (!size.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return nullptr;
  }

  return mBridge->SendBufferMap(aId, mode, offset.value(), size.value());
}

void Device::UnmapBuffer(RawId aId, ipc::Shmem&& aShmem, bool aFlush,
                         bool aKeepShmem) {
  if (mBridge->CanSend()) {
    mBridge->SendBufferUnmap(aId, std::move(aShmem), aFlush, aKeepShmem);
  }
}

already_AddRefed<Texture> Device::CreateTexture(
    const dom::GPUTextureDescriptor& aDesc) {
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateTexture(mId, aDesc);
  }
  RefPtr<Texture> texture = new Texture(this, id, aDesc);
  return texture.forget();
}

already_AddRefed<Sampler> Device::CreateSampler(
    const dom::GPUSamplerDescriptor& aDesc) {
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateSampler(mId, aDesc);
  }
  RefPtr<Sampler> sampler = new Sampler(this, id);
  return sampler.forget();
}

already_AddRefed<CommandEncoder> Device::CreateCommandEncoder(
    const dom::GPUCommandEncoderDescriptor& aDesc) {
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateCommandEncoder(mId, aDesc);
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
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateBindGroupLayout(mId, aDesc);
  }
  RefPtr<BindGroupLayout> object = new BindGroupLayout(this, id, true);
  return object.forget();
}
already_AddRefed<PipelineLayout> Device::CreatePipelineLayout(
    const dom::GPUPipelineLayoutDescriptor& aDesc) {
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreatePipelineLayout(mId, aDesc);
  }
  RefPtr<PipelineLayout> object = new PipelineLayout(this, id);
  return object.forget();
}
already_AddRefed<BindGroup> Device::CreateBindGroup(
    const dom::GPUBindGroupDescriptor& aDesc) {
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateBindGroup(mId, aDesc);
  }
  RefPtr<BindGroup> object = new BindGroup(this, id);
  return object.forget();
}

already_AddRefed<ShaderModule> Device::CreateShaderModule(
    JSContext* aCx, const dom::GPUShaderModuleDescriptor& aDesc) {
  Unused << aCx;
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateShaderModule(mId, aDesc);
  }
  RefPtr<ShaderModule> object = new ShaderModule(this, id);
  return object.forget();
}

already_AddRefed<ComputePipeline> Device::CreateComputePipeline(
    const dom::GPUComputePipelineDescriptor& aDesc) {
  PipelineCreationContext context = {mId};
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateComputePipeline(&context, aDesc);
  }
  RefPtr<ComputePipeline> object =
      new ComputePipeline(this, id, context.mImplicitPipelineLayoutId,
                          std::move(context.mImplicitBindGroupLayoutIds));
  return object.forget();
}

already_AddRefed<RenderPipeline> Device::CreateRenderPipeline(
    const dom::GPURenderPipelineDescriptor& aDesc) {
  PipelineCreationContext context = {mId};
  RawId id = 0;
  if (mBridge->CanSend()) {
    id = mBridge->DeviceCreateRenderPipeline(&context, aDesc);
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

  if (!mBridge->CanSend()) {
    promise->MaybeRejectWithOperationError("Internal communication error");
    return promise.forget();
  }

  std::shared_ptr<PipelineCreationContext> context(
      new PipelineCreationContext());
  context->mParentId = mId;
  mBridge->DeviceCreateComputePipelineAsync(context.get(), aDesc)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, context, promise](RawId aId) {
            RefPtr<ComputePipeline> object = new ComputePipeline(
                self, aId, context->mImplicitPipelineLayoutId,
                std::move(context->mImplicitBindGroupLayoutIds));
            promise->MaybeResolve(object);
          },
          [promise](const ipc::ResponseRejectReason&) {
            promise->MaybeRejectWithOperationError(
                "Internal communication error");
          });

  return promise.forget();
}

already_AddRefed<dom::Promise> Device::CreateRenderPipelineAsync(
    const dom::GPURenderPipelineDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!mBridge->CanSend()) {
    promise->MaybeRejectWithOperationError("Internal communication error");
    return promise.forget();
  }

  std::shared_ptr<PipelineCreationContext> context(
      new PipelineCreationContext());
  context->mParentId = mId;
  mBridge->DeviceCreateRenderPipelineAsync(context.get(), aDesc)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, context, promise](RawId aId) {
            RefPtr<RenderPipeline> object = new RenderPipeline(
                self, aId, context->mImplicitPipelineLayoutId,
                std::move(context->mImplicitBindGroupLayoutIds));
            promise->MaybeResolve(object);
          },
          [promise](const ipc::ResponseRejectReason&) {
            promise->MaybeRejectWithOperationError(
                "Internal communication error");
          });

  return promise.forget();
}

already_AddRefed<Texture> Device::InitSwapChain(
    const dom::GPUCanvasConfiguration& aDesc,
    const layers::CompositableHandle& aHandle, gfx::SurfaceFormat aFormat,
    gfx::IntSize* aCanvasSize) {
  if (!mBridge->CanSend()) {
    return nullptr;
  }

  gfx::IntSize size = *aCanvasSize;
  if (aDesc.mSize.WasPassed()) {
    const auto& descSize = aDesc.mSize.Value();
    if (descSize.IsRangeEnforcedUnsignedLongSequence()) {
      const auto& seq = descSize.GetAsRangeEnforcedUnsignedLongSequence();
      // TODO: add a check for `seq.Length()`
      size.width = AssertedCast<int>(seq[0]);
      size.height = AssertedCast<int>(seq[1]);
    } else if (descSize.IsGPUExtent3DDict()) {
      const auto& dict = descSize.GetAsGPUExtent3DDict();
      size.width = AssertedCast<int>(dict.mWidth);
      size.height = AssertedCast<int>(dict.mHeight);
    } else {
      MOZ_CRASH("Unexpected union");
    }
    *aCanvasSize = size;
  }

  const layers::RGBDescriptor rgbDesc(size, aFormat);
  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  mBridge->DeviceCreateSwapChain(mId, rgbDesc, maxBufferCount, aHandle);

  dom::GPUTextureDescriptor desc;
  desc.mDimension = dom::GPUTextureDimension::_2d;
  auto& sizeDict = desc.mSize.SetAsGPUExtent3DDict();
  sizeDict.mWidth = size.width;
  sizeDict.mHeight = size.height;
  sizeDict.mDepthOrArrayLayers = 1;
  desc.mFormat = aDesc.mFormat;
  desc.mMipLevelCount = 1;
  desc.mSampleCount = 1;
  desc.mUsage = aDesc.mUsage | dom::GPUTextureUsage_Binding::COPY_SRC;
  return CreateTexture(desc);
}

bool Device::CheckNewWarning(const nsACString& aMessage) {
  return mKnownWarnings.EnsureInserted(aMessage);
}

void Device::Destroy() {
  // TODO
}

void Device::PushErrorScope(const dom::GPUErrorFilter& aFilter) {
  if (mBridge->CanSend()) {
    mBridge->SendDevicePushErrorScope(mId);
  }
}

already_AddRefed<dom::Promise> Device::PopErrorScope(ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!mBridge->CanSend()) {
    promise->MaybeRejectWithOperationError("Internal communication error");
    return promise.forget();
  }

  auto errorPromise = mBridge->SendDevicePopErrorScope(mId);

  errorPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, promise](const MaybeScopedError& aMaybeError) {
        if (aMaybeError) {
          if (aMaybeError->operationError) {
            promise->MaybeRejectWithOperationError("Stack is empty");
          } else {
            dom::OwningGPUOutOfMemoryErrorOrGPUValidationError error;
            if (aMaybeError->validationMessage.IsEmpty()) {
              error.SetAsGPUOutOfMemoryError();
            } else {
              error.SetAsGPUValidationError() = new ValidationError(
                  self->GetParentObject(), aMaybeError->validationMessage);
            }
            promise->MaybeResolve(std::move(error));
          }
        } else {
          promise->MaybeResolveWithUndefined();
        }
      },
      [promise](const ipc::ResponseRejectReason&) {
        promise->MaybeRejectWithOperationError("Internal communication error");
      });

  return promise.forget();
}

}  // namespace mozilla::webgpu
