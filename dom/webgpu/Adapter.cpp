/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Adapter.h"

#include "Device.h"
#include "Instance.h"
#include "SupportedFeatures.h"
#include "SupportedLimits.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

bool AdapterInfo::WrapObject(JSContext* const cx,
                             JS::Handle<JSObject*> givenProto,
                             JS::MutableHandle<JSObject*> reflector) {
  return dom::GPUAdapterInfo_Binding::Wrap(cx, this, givenProto, reflector);
}

// -

GPU_IMPL_CYCLE_COLLECTION(Adapter, mParent, mBridge, mFeatures, mLimits)
GPU_IMPL_JS_WRAP(Adapter)

Maybe<uint32_t> Adapter::MakeFeatureBits(
    const dom::Sequence<dom::GPUFeatureName>& aFeatures) {
  uint32_t bits = 0;
  for (const auto& feature : aFeatures) {
    if (feature == dom::GPUFeatureName::Depth_clip_control) {
      bits |= WGPUFeatures_DEPTH_CLIP_CONTROL;
    } else if (feature == dom::GPUFeatureName::Texture_compression_bc) {
      bits |= WGPUFeatures_TEXTURE_COMPRESSION_BC;
    } else if (feature == dom::GPUFeatureName::Indirect_first_instance) {
      bits |= WGPUFeatures_INDIRECT_FIRST_INSTANCE;
    } else {
      NS_WARNING(
          nsPrintfCString("Requested feature bit '%d' is not recognized.",
                          static_cast<int>(feature))
              .get());
      return Nothing();
    }
  }
  return Some(bits);
}

Adapter::Adapter(Instance* const aParent, WebGPUChild* const aBridge,
                 const ffi::WGPUAdapterInformation& aInfo)
    : ChildOf(aParent),
      mBridge(aBridge),
      mId(aInfo.id),
      mFeatures(new SupportedFeatures(this)),
      mLimits(
          new SupportedLimits(this, MakeUnique<ffi::WGPULimits>(aInfo.limits))),
      mIsFallbackAdapter(aInfo.ty == ffi::WGPUDeviceType_Cpu) {
  ErrorResult result;  // TODO: should this come from outside
  // This list needs to match `AdapterRequestDevice`
  if (aInfo.features & WGPUFeatures_DEPTH_CLIP_CONTROL) {
    dom::GPUSupportedFeatures_Binding::SetlikeHelpers::Add(
        mFeatures, u"depth-clip-control"_ns, result);
  }
  if (aInfo.features & WGPUFeatures_TEXTURE_COMPRESSION_BC) {
    dom::GPUSupportedFeatures_Binding::SetlikeHelpers::Add(
        mFeatures, u"texture-compression-bc"_ns, result);
  }
  if (aInfo.features & WGPUFeatures_INDIRECT_FIRST_INSTANCE) {
    dom::GPUSupportedFeatures_Binding::SetlikeHelpers::Add(
        mFeatures, u"indirect-first-instance"_ns, result);
  }
}

Adapter::~Adapter() { Cleanup(); }

void Adapter::Cleanup() {
  if (mValid && mBridge && mBridge->CanSend()) {
    mValid = false;
    mBridge->SendAdapterDestroy(mId);
  }
}

const RefPtr<SupportedFeatures>& Adapter::Features() const { return mFeatures; }
const RefPtr<SupportedLimits>& Adapter::Limits() const { return mLimits; }

already_AddRefed<dom::Promise> Adapter::RequestDevice(
    const dom::GPUDeviceDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!mBridge->CanSend()) {
    promise->MaybeRejectWithInvalidStateError(
        "WebGPUChild cannot send, must recreate Adapter");
    return promise.forget();
  }

  ffi::WGPULimits limits = {};
  auto request = mBridge->AdapterRequestDevice(mId, aDesc, &limits);
  if (request) {
    RefPtr<Device> device =
        new Device(this, request->mId, MakeUnique<ffi::WGPULimits>(limits));
    // copy over the features
    for (const auto& feature : aDesc.mRequiredFeatures) {
      NS_ConvertASCIItoUTF16 string(
          dom::GPUFeatureNameValues::GetString(feature));
      dom::GPUSupportedFeatures_Binding::SetlikeHelpers::Add(device->mFeatures,
                                                             string, aRv);
    }

    request->mPromise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [promise, device](bool aSuccess) {
          if (aSuccess) {
            promise->MaybeResolve(device);
          } else {
            // In this path, request->mId has an error entry in the wgpu
            // registry, so let Device::~Device clean things up on both the
            // child and parent side.
            promise->MaybeRejectWithInvalidStateError(
                "Unable to fulfill requested features and limits");
          }
        },
        [promise, device](const ipc::ResponseRejectReason& aReason) {
          // We can't be sure how far along the WebGPUParent got in handling
          // our AdapterRequestDevice message, but we can't communicate with it,
          // so clear up our client state for this Device without trying to
          // communicate with the parent about it.
          device->CleanupUnregisteredInParent();
          promise->MaybeRejectWithNotSupportedError("IPC error");
        });
  } else {
    promise->MaybeRejectWithNotSupportedError("Unable to instantiate a Device");
  }

  return promise.forget();
}

// -

already_AddRefed<dom::Promise> Adapter::RequestAdapterInfo(
    const dom::Sequence<nsString>& /*aUnmaskHints*/, ErrorResult& aRv) const {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (!promise) return nullptr;

  auto rai = UniquePtr<AdapterInfo>{new AdapterInfo};
  // E.g. rai.mDevice = SanitizeRenderer(deviceName);

  promise->MaybeResolve(std::move(rai));
  return promise.forget();
}

}  // namespace mozilla::webgpu
