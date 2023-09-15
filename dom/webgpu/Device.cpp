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
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "Device.h"
#include "CommandEncoder.h"
#include "BindGroup.h"

#include "Adapter.h"
#include "Buffer.h"
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

namespace mozilla::webgpu {

mozilla::LazyLogModule gWebGPULog("WebGPU");

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(Device, DOMEventTargetHelper,
                                                 mBridge, mQueue, mFeatures,
                                                 mLimits, mLostPromise);
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(Device, DOMEventTargetHelper)
GPU_IMPL_JS_WRAP(Device)

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

  // Cycle collection may have disconnected the promise object.
  if (mLostPromise && mLostPromise->PromiseObj() != nullptr) {
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

bool Device::IsLost() const { return !mBridge || !mBridge->CanSend(); }

// Generate an error on the Device timeline for this device.
//
// aMessage is interpreted as UTF-8.
void Device::GenerateValidationError(const nsCString& aMessage) {
  if (IsLost()) {
    return;  // Just drop it?
  }
  mBridge->SendGenerateError(Some(mId), dom::GPUErrorFilter::Validation,
                             aMessage);
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
  return Buffer::Create(this, mId, aDesc, aRv);
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

MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION already_AddRefed<ShaderModule>
Device::CreateShaderModule(JSContext* aCx,
                           const dom::GPUShaderModuleDescriptor& aDesc) {
  Unused << aCx;

  if (!mBridge->CanSend()) {
    return nullptr;
  }

  ErrorResult err;
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), err);
  if (NS_WARN_IF(err.Failed())) {
    return nullptr;
  }

  return MOZ_KnownLive(mBridge)->DeviceCreateShaderModule(this, aDesc, promise);
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
    const layers::RemoteTextureOwnerId aOwnerId, gfx::SurfaceFormat aFormat,
    gfx::IntSize aCanvasSize) {
  if (!mBridge->CanSend()) {
    return nullptr;
  }

  const layers::RGBDescriptor rgbDesc(aCanvasSize, aFormat);
  // buffer count doesn't matter much, will be created on demand
  const size_t maxBufferCount = 10;
  mBridge->DeviceCreateSwapChain(mId, rgbDesc, maxBufferCount, aOwnerId);

  dom::GPUTextureDescriptor desc;
  desc.mDimension = dom::GPUTextureDimension::_2d;
  auto& sizeDict = desc.mSize.SetAsGPUExtent3DDict();
  sizeDict.mWidth = aCanvasSize.width;
  sizeDict.mHeight = aCanvasSize.height;
  sizeDict.mDepthOrArrayLayers = 1;
  desc.mFormat = aDesc.mFormat;
  desc.mMipLevelCount = 1;
  desc.mSampleCount = 1;
  desc.mUsage = aDesc.mUsage | dom::GPUTextureUsage_Binding::COPY_SRC;
  desc.mViewFormats = aDesc.mViewFormats;
  // TODO: `mColorSpace`: <https://bugzilla.mozilla.org/show_bug.cgi?id=1846608>
  // TODO: `mAlphaMode`: <https://bugzilla.mozilla.org/show_bug.cgi?id=1846605>
  return CreateTexture(desc);
}

bool Device::CheckNewWarning(const nsACString& aMessage) {
  return mKnownWarnings.EnsureInserted(aMessage);
}

void Device::Destroy() {
  // TODO
}

void Device::PushErrorScope(const dom::GPUErrorFilter& aFilter) {
  if (IsLost()) {
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

  if (IsLost()) {
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
