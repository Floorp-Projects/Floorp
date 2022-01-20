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
#include "Queue.h"
#include "RenderBundleEncoder.h"
#include "RenderPipeline.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureView.h"
#include "ValidationError.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

mozilla::LazyLogModule gWebGPULog("WebGPU");

NS_IMPL_CYCLE_COLLECTION_INHERITED(Device, DOMEventTargetHelper, mBridge,
                                   mQueue)
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

Device::Device(Adapter* const aParent, RawId aId)
    : DOMEventTargetHelper(aParent->GetParentObject()),
      mId(aId),
      mBridge(aParent->mBridge),
      mQueue(new class Queue(this, aParent->mBridge, aId)) {
  mBridge->RegisterDevice(mId, this);
}

Device::~Device() { Cleanup(); }

void Device::Cleanup() {
  if (mValid && mBridge && mBridge->IsOpen()) {
    mValid = false;
    mBridge->UnregisterDevice(mId);
  }
}

void Device::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void Device::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

const RefPtr<Queue>& Device::GetQueue() const { return mQueue; }

already_AddRefed<Buffer> Device::CreateBuffer(
    const dom::GPUBufferDescriptor& aDesc, ErrorResult& aRv) {
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
  RefPtr<Buffer> buffer = new Buffer(this, id, aDesc.mSize, hasMapFlags);
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
      aRv.ThrowInvalidAccessError(
          nsPrintfCString("Invalid map flag %u", aMode));
      return nullptr;
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
  mBridge->SendBufferUnmap(aId, std::move(aShmem), aFlush, aKeepShmem);
}

already_AddRefed<Texture> Device::CreateTexture(
    const dom::GPUTextureDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateTexture(mId, aDesc);
  RefPtr<Texture> texture = new Texture(this, id, aDesc);
  return texture.forget();
}

already_AddRefed<Sampler> Device::CreateSampler(
    const dom::GPUSamplerDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateSampler(mId, aDesc);
  RefPtr<Sampler> sampler = new Sampler(this, id);
  return sampler.forget();
}

already_AddRefed<CommandEncoder> Device::CreateCommandEncoder(
    const dom::GPUCommandEncoderDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateCommandEncoder(mId, aDesc);
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
  RawId id = mBridge->DeviceCreateBindGroupLayout(mId, aDesc);
  RefPtr<BindGroupLayout> object = new BindGroupLayout(this, id, true);
  return object.forget();
}
already_AddRefed<PipelineLayout> Device::CreatePipelineLayout(
    const dom::GPUPipelineLayoutDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreatePipelineLayout(mId, aDesc);
  RefPtr<PipelineLayout> object = new PipelineLayout(this, id);
  return object.forget();
}
already_AddRefed<BindGroup> Device::CreateBindGroup(
    const dom::GPUBindGroupDescriptor& aDesc) {
  RawId id = mBridge->DeviceCreateBindGroup(mId, aDesc);
  RefPtr<BindGroup> object = new BindGroup(this, id);
  return object.forget();
}

already_AddRefed<ShaderModule> Device::CreateShaderModule(
    JSContext* aCx, const dom::GPUShaderModuleDescriptor& aDesc) {
  Unused << aCx;
  RawId id = mBridge->DeviceCreateShaderModule(mId, aDesc);
  RefPtr<ShaderModule> object = new ShaderModule(this, id);
  return object.forget();
}

already_AddRefed<ComputePipeline> Device::CreateComputePipeline(
    const dom::GPUComputePipelineDescriptor& aDesc) {
  nsTArray<RawId> implicitBindGroupLayoutIds;
  RawId implicitPipelineLayoutId = 0;
  RawId id = mBridge->DeviceCreateComputePipeline(
      mId, aDesc, &implicitPipelineLayoutId, &implicitBindGroupLayoutIds);
  RefPtr<ComputePipeline> object =
      new ComputePipeline(this, id, implicitPipelineLayoutId,
                          std::move(implicitBindGroupLayoutIds));
  return object.forget();
}

already_AddRefed<RenderPipeline> Device::CreateRenderPipeline(
    const dom::GPURenderPipelineDescriptor& aDesc) {
  nsTArray<RawId> implicitBindGroupLayoutIds;
  RawId implicitPipelineLayoutId = 0;
  RawId id = mBridge->DeviceCreateRenderPipeline(
      mId, aDesc, &implicitPipelineLayoutId, &implicitBindGroupLayoutIds);
  RefPtr<RenderPipeline> object =
      new RenderPipeline(this, id, implicitPipelineLayoutId,
                         std::move(implicitBindGroupLayoutIds));
  return object.forget();
}

already_AddRefed<Texture> Device::InitSwapChain(
    const dom::GPUCanvasConfiguration& aDesc,
    wr::ExternalImageId aExternalImageId, gfx::SurfaceFormat aFormat,
    gfx::IntSize* aCanvasSize) {
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
  }
  *aCanvasSize = size;

  const layers::RGBDescriptor rgbDesc(size, aFormat);
  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  mBridge->DeviceCreateSwapChain(mId, rgbDesc, maxBufferCount,
                                 aExternalImageId);

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
  mBridge->SendDevicePushErrorScope(mId);
}

already_AddRefed<dom::Promise> Device::PopErrorScope(ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  auto errorPromise = mBridge->SendDevicePopErrorScope(mId);

  errorPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr{this}, promise](const MaybeScopedError& aMaybeError) {
        if (aMaybeError) {
          if (aMaybeError->operationError) {
            promise->MaybeRejectWithOperationError("Stack is empty");
          } else {
            dom::OwningGPUOutOfMemoryErrorOrGPUValidationError error;
            if (aMaybeError->validationMessage.IsEmpty()) {
              error.SetAsGPUOutOfMemoryError();
            } else {
              error.SetAsGPUValidationError() =
                  new ValidationError(self, aMaybeError->validationMessage);
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

}  // namespace webgpu
}  // namespace mozilla
