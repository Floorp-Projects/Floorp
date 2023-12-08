/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "Adapter.h"

#include <algorithm>
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

void AdapterInfo::GetWgpuName(nsString& s) const {
  s = mAboutSupportInfo->name;
}

uint32_t AdapterInfo::WgpuVendor() const { return mAboutSupportInfo->vendor; }

uint32_t AdapterInfo::WgpuDevice() const { return mAboutSupportInfo->device; }

void AdapterInfo::GetWgpuDeviceType(nsString& s) const {
  switch (mAboutSupportInfo->device_type) {
    case ffi::WGPUDeviceType_Cpu:
      s.AssignLiteral("Cpu");
      return;
    case ffi::WGPUDeviceType_DiscreteGpu:
      s.AssignLiteral("DiscreteGpu");
      return;
    case ffi::WGPUDeviceType_IntegratedGpu:
      s.AssignLiteral("IntegratedGpu");
      return;
    case ffi::WGPUDeviceType_VirtualGpu:
      s.AssignLiteral("VirtualGpu");
      return;
    case ffi::WGPUDeviceType_Other:
      s.AssignLiteral("Other");
      return;
    case ffi::WGPUDeviceType_Sentinel:
      break;
  }
  MOZ_CRASH("Bad `ffi::WGPUDeviceType`");
}

void AdapterInfo::GetWgpuDriver(nsString& s) const {
  s = mAboutSupportInfo->driver;
}

void AdapterInfo::GetWgpuDriverInfo(nsString& s) const {
  s = mAboutSupportInfo->driver_info;
}

void AdapterInfo::GetWgpuBackend(nsString& s) const {
  switch (mAboutSupportInfo->backend) {
    case ffi::WGPUBackend_Empty:
      s.AssignLiteral("Empty");
      return;
    case ffi::WGPUBackend_Vulkan:
      s.AssignLiteral("Vulkan");
      return;
    case ffi::WGPUBackend_Metal:
      s.AssignLiteral("Metal");
      return;
    case ffi::WGPUBackend_Dx12:
      s.AssignLiteral("Dx12");
      return;
    case ffi::WGPUBackend_Gl:
      s.AssignLiteral("Gl");
      return;
    case ffi::WGPUBackend_BrowserWebGpu:  // This should never happen, because
                                          // we _are_ the browser.
    case ffi::WGPUBackend_Sentinel:
      break;
  }
  MOZ_CRASH("Bad `ffi::WGPUBackend`");
}

// -

GPU_IMPL_CYCLE_COLLECTION(Adapter, mParent, mBridge, mFeatures, mLimits)
GPU_IMPL_JS_WRAP(Adapter)

static Maybe<ffi::WGPUFeatures> ToWGPUFeatures(
    const dom::GPUFeatureName aFeature) {
  switch (aFeature) {
    case dom::GPUFeatureName::Depth_clip_control:
      return Some(WGPUFeatures_DEPTH_CLIP_CONTROL);

    case dom::GPUFeatureName::Depth32float_stencil8:
      return Some(WGPUFeatures_DEPTH32FLOAT_STENCIL8);

    case dom::GPUFeatureName::Texture_compression_bc:
      return Some(WGPUFeatures_TEXTURE_COMPRESSION_BC);

    case dom::GPUFeatureName::Texture_compression_etc2:
      return Some(WGPUFeatures_TEXTURE_COMPRESSION_ETC2);

    case dom::GPUFeatureName::Texture_compression_astc:
      return Some(WGPUFeatures_TEXTURE_COMPRESSION_ASTC);

    case dom::GPUFeatureName::Timestamp_query:
      return Some(WGPUFeatures_TIMESTAMP_QUERY);

    case dom::GPUFeatureName::Indirect_first_instance:
      return Some(WGPUFeatures_INDIRECT_FIRST_INSTANCE);

    case dom::GPUFeatureName::Shader_f16:
      return Some(WGPUFeatures_SHADER_F16);

    case dom::GPUFeatureName::Rg11b10ufloat_renderable:
      return Some(WGPUFeatures_RG11B10UFLOAT_RENDERABLE);

    case dom::GPUFeatureName::Bgra8unorm_storage:
      return Some(WGPUFeatures_BGRA8UNORM_STORAGE);

    case dom::GPUFeatureName::Float32_filterable:
      return Some(WGPUFeatures_FLOAT32_FILTERABLE);

    case dom::GPUFeatureName::EndGuard_:
      break;
  }
  MOZ_CRASH("Bad GPUFeatureName.");
}

static Maybe<ffi::WGPUFeatures> MakeFeatureBits(
    const dom::Sequence<dom::GPUFeatureName>& aFeatures) {
  ffi::WGPUFeatures bits = 0;
  for (const auto& feature : aFeatures) {
    const auto bit = ToWGPUFeatures(feature);
    if (!bit) {
      const auto featureStr = dom::GPUFeatureNameValues::GetString(feature);
      (void)featureStr;
      NS_WARNING(
          nsPrintfCString("Requested feature bit for '%s' is not implemented.",
                          featureStr.data())
              .get());
      return Nothing();
    }
    bits |= *bit;
  }
  return Some(bits);
}

Adapter::Adapter(Instance* const aParent, WebGPUChild* const aBridge,
                 const std::shared_ptr<ffi::WGPUAdapterInformation>& aInfo)
    : ChildOf(aParent),
      mBridge(aBridge),
      mId(aInfo->id),
      mFeatures(new SupportedFeatures(this)),
      mLimits(new SupportedLimits(this, aInfo->limits)),
      mInfo(aInfo) {
  ErrorResult ignoredRv;  // It's onerous to plumb this in from outside in this
                          // case, and we don't really need to.

  static const auto FEATURE_BY_BIT = []() {
    auto ret = std::unordered_map<ffi::WGPUFeatures, dom::GPUFeatureName>{};

    for (const auto feature :
         MakeEnumeratedRange(dom::GPUFeatureName::EndGuard_)) {
      const auto bitForFeature = ToWGPUFeatures(feature);
      if (!bitForFeature) {
        // There are some features that don't have bits.
        continue;
      }
      ret[*bitForFeature] = feature;
    }

    return ret;
  }();

  auto remainingFeatureBits = aInfo->features;
  auto bitMask = decltype(remainingFeatureBits){0};
  while (remainingFeatureBits) {
    if (bitMask) {
      bitMask <<= 1;
    } else {
      bitMask = 1;
    }
    const auto bit = remainingFeatureBits & bitMask;
    remainingFeatureBits &= ~bitMask;  // Clear bit.
    if (!bit) {
      continue;
    }

    const auto featureForBit = FEATURE_BY_BIT.find(bit);
    if (featureForBit != FEATURE_BY_BIT.end()) {
      mFeatures->Add(featureForBit->second, ignoredRv);
    } else {
      // We don't recognize that bit, but maybe it's a wpgu-native-only feature.
    }
  }
}

Adapter::~Adapter() { Cleanup(); }

void Adapter::Cleanup() {
  if (mValid && mBridge && mBridge->CanSend()) {
    mValid = false;
    mBridge->SendAdapterDrop(mId);
  }
}

const RefPtr<SupportedFeatures>& Adapter::Features() const { return mFeatures; }
const RefPtr<SupportedLimits>& Adapter::Limits() const { return mLimits; }
bool Adapter::IsFallbackAdapter() const {
  return mInfo->device_type == ffi::WGPUDeviceType::WGPUDeviceType_Cpu;
}

static std::string_view ToJsKey(const Limit limit) {
  switch (limit) {
    case Limit::MaxTextureDimension1D:
      return "maxTextureDimension1D";
    case Limit::MaxTextureDimension2D:
      return "maxTextureDimension2D";
    case Limit::MaxTextureDimension3D:
      return "maxTextureDimension3D";
    case Limit::MaxTextureArrayLayers:
      return "maxTextureArrayLayers";
    case Limit::MaxBindGroups:
      return "maxBindGroups";
    case Limit::MaxBindGroupsPlusVertexBuffers:
      return "maxBindGroupsPlusVertexBuffers";
    case Limit::MaxBindingsPerBindGroup:
      return "maxBindingsPerBindGroup";
    case Limit::MaxDynamicUniformBuffersPerPipelineLayout:
      return "maxDynamicUniformBuffersPerPipelineLayout";
    case Limit::MaxDynamicStorageBuffersPerPipelineLayout:
      return "maxDynamicStorageBuffersPerPipelineLayout";
    case Limit::MaxSampledTexturesPerShaderStage:
      return "maxSampledTexturesPerShaderStage";
    case Limit::MaxSamplersPerShaderStage:
      return "maxSamplersPerShaderStage";
    case Limit::MaxStorageBuffersPerShaderStage:
      return "maxStorageBuffersPerShaderStage";
    case Limit::MaxStorageTexturesPerShaderStage:
      return "maxStorageTexturesPerShaderStage";
    case Limit::MaxUniformBuffersPerShaderStage:
      return "maxUniformBuffersPerShaderStage";
    case Limit::MaxUniformBufferBindingSize:
      return "maxUniformBufferBindingSize";
    case Limit::MaxStorageBufferBindingSize:
      return "maxStorageBufferBindingSize";
    case Limit::MinUniformBufferOffsetAlignment:
      return "minUniformBufferOffsetAlignment";
    case Limit::MinStorageBufferOffsetAlignment:
      return "minStorageBufferOffsetAlignment";
    case Limit::MaxVertexBuffers:
      return "maxVertexBuffers";
    case Limit::MaxBufferSize:
      return "maxBufferSize";
    case Limit::MaxVertexAttributes:
      return "maxVertexAttributes";
    case Limit::MaxVertexBufferArrayStride:
      return "maxVertexBufferArrayStride";
    case Limit::MaxInterStageShaderComponents:
      return "maxInterStageShaderComponents";
    case Limit::MaxInterStageShaderVariables:
      return "maxInterStageShaderVariables";
    case Limit::MaxColorAttachments:
      return "maxColorAttachments";
    case Limit::MaxColorAttachmentBytesPerSample:
      return "maxColorAttachmentBytesPerSample";
    case Limit::MaxComputeWorkgroupStorageSize:
      return "maxComputeWorkgroupStorageSize";
    case Limit::MaxComputeInvocationsPerWorkgroup:
      return "maxComputeInvocationsPerWorkgroup";
    case Limit::MaxComputeWorkgroupSizeX:
      return "maxComputeWorkgroupSizeX";
    case Limit::MaxComputeWorkgroupSizeY:
      return "maxComputeWorkgroupSizeY";
    case Limit::MaxComputeWorkgroupSizeZ:
      return "maxComputeWorkgroupSizeZ";
    case Limit::MaxComputeWorkgroupsPerDimension:
      return "maxComputeWorkgroupsPerDimension";
  }
  MOZ_CRASH("Bad Limit");
}

// -
// String helpers

static auto ToACString(const nsAString& s) { return NS_ConvertUTF16toUTF8(s); }

// -
// Adapter::RequestDevice

already_AddRefed<dom::Promise> Adapter::RequestDevice(
    const dom::GPUDeviceDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  ffi::WGPULimits deviceLimits = *mLimits->mFfi;
  for (const auto limit : MakeInclusiveEnumeratedRange(Limit::_LAST)) {
    const auto defaultValue = [&]() -> double {
      switch (limit) {
          // clang-format off
      case Limit::MaxTextureDimension1D: return 8192;
      case Limit::MaxTextureDimension2D: return 8192;
      case Limit::MaxTextureDimension3D: return 2048;
      case Limit::MaxTextureArrayLayers: return 256;
      case Limit::MaxBindGroups: return 4;
      case Limit::MaxBindGroupsPlusVertexBuffers: return 24;
      case Limit::MaxBindingsPerBindGroup: return 1000;
      case Limit::MaxDynamicUniformBuffersPerPipelineLayout: return 8;
      case Limit::MaxDynamicStorageBuffersPerPipelineLayout: return 4;
      case Limit::MaxSampledTexturesPerShaderStage: return 16;
      case Limit::MaxSamplersPerShaderStage: return 16;
      case Limit::MaxStorageBuffersPerShaderStage: return 8;
      case Limit::MaxStorageTexturesPerShaderStage: return 4;
      case Limit::MaxUniformBuffersPerShaderStage: return 12;
      case Limit::MaxUniformBufferBindingSize: return 65536;
      case Limit::MaxStorageBufferBindingSize: return 134217728;
      case Limit::MinUniformBufferOffsetAlignment: return 256;
      case Limit::MinStorageBufferOffsetAlignment: return 256;
      case Limit::MaxVertexBuffers: return 8;
      case Limit::MaxBufferSize: return 268435456;
      case Limit::MaxVertexAttributes: return 16;
      case Limit::MaxVertexBufferArrayStride: return 2048;
      case Limit::MaxInterStageShaderComponents: return 60;
      case Limit::MaxInterStageShaderVariables: return 16;
      case Limit::MaxColorAttachments: return 8;
      case Limit::MaxColorAttachmentBytesPerSample: return 32;
      case Limit::MaxComputeWorkgroupStorageSize: return 16384;
      case Limit::MaxComputeInvocationsPerWorkgroup: return 256;
      case Limit::MaxComputeWorkgroupSizeX: return 256;
      case Limit::MaxComputeWorkgroupSizeY: return 256;
      case Limit::MaxComputeWorkgroupSizeZ: return 64;
      case Limit::MaxComputeWorkgroupsPerDimension: return 65535;
          // clang-format on
      }
      MOZ_CRASH("Bad Limit");
    }();
    SetLimit(&deviceLimits, limit, defaultValue);
  }

  // -

  [&]() {  // So that we can `return;` instead of `return promise.forget();`.
    if (!mBridge->CanSend()) {
      promise->MaybeRejectWithInvalidStateError(
          "WebGPUChild cannot send, must recreate Adapter");
      return;
    }

    // -
    // Validate Features

    for (const auto requested : aDesc.mRequiredFeatures) {
      const bool supported = mFeatures->Features().count(requested);
      if (!supported) {
        const auto fstr = dom::GPUFeatureNameValues::GetString(requested);
        const auto astr = this->LabelOrId();
        nsPrintfCString msg(
            "requestDevice: Feature '%s' requested must be supported by "
            "adapter %s",
            fstr.data(), astr.get());
        promise->MaybeRejectWithTypeError(msg);
        return;
      }
    }

    // -
    // Validate Limits

    if (aDesc.mRequiredLimits.WasPassed()) {
      static const auto LIMIT_BY_JS_KEY = []() {
        std::unordered_map<std::string_view, Limit> ret;
        for (const auto limit : MakeInclusiveEnumeratedRange(Limit::_LAST)) {
          const auto jsKeyU8 = ToJsKey(limit);
          ret[jsKeyU8] = limit;
        }
        return ret;
      }();

      for (const auto& entry : aDesc.mRequiredLimits.Value().Entries()) {
        const auto& keyU16 = entry.mKey;
        const nsCString keyU8 = ToACString(keyU16);
        const auto itr = LIMIT_BY_JS_KEY.find(keyU8.get());
        if (itr == LIMIT_BY_JS_KEY.end()) {
          nsPrintfCString msg("requestDevice: Limit '%s' not recognized.",
                              keyU8.get());
          promise->MaybeRejectWithOperationError(msg);
          return;
        }

        const auto& limit = itr->second;
        uint64_t requestedValue = entry.mValue;
        const auto supportedValue = GetLimit(*mLimits->mFfi, limit);
        if (StringBeginsWith(keyU8, "max"_ns)) {
          if (requestedValue > supportedValue) {
            nsPrintfCString msg(
                "requestDevice: Request for limit '%s' must be <= supported "
                "%s, was %s.",
                keyU8.get(), std::to_string(supportedValue).c_str(),
                std::to_string(requestedValue).c_str());
            promise->MaybeRejectWithOperationError(msg);
            return;
          }
          // Clamp to default if lower than default
          requestedValue =
              std::max(requestedValue, GetLimit(deviceLimits, limit));
        } else {
          MOZ_ASSERT(StringBeginsWith(keyU8, "min"_ns));
          if (requestedValue < supportedValue) {
            nsPrintfCString msg(
                "requestDevice: Request for limit '%s' must be >= supported "
                "%s, was %s.",
                keyU8.get(), std::to_string(supportedValue).c_str(),
                std::to_string(requestedValue).c_str());
            promise->MaybeRejectWithOperationError(msg);
            return;
          }
          if (StringEndsWith(keyU8, "Alignment"_ns)) {
            if (!IsPowerOfTwo(requestedValue)) {
              nsPrintfCString msg(
                  "requestDevice: Request for limit '%s' must be a power of "
                  "two, "
                  "was %s.",
                  keyU8.get(), std::to_string(requestedValue).c_str());
              promise->MaybeRejectWithOperationError(msg);
              return;
            }
          }
          /// Clamp to default if higher than default
          requestedValue =
              std::min(requestedValue, GetLimit(deviceLimits, limit));
        }

        SetLimit(&deviceLimits, limit, requestedValue);
      }
    }

    // -

    ffi::WGPUDeviceDescriptor ffiDesc = {};
    ffiDesc.required_features = *MakeFeatureBits(aDesc.mRequiredFeatures);
    ffiDesc.required_limits = deviceLimits;
    auto request = mBridge->AdapterRequestDevice(mId, ffiDesc);
    if (!request) {
      promise->MaybeRejectWithNotSupportedError(
          "Unable to instantiate a Device");
      return;
    }
    RefPtr<Device> device =
        new Device(this, request->mId, ffiDesc.required_limits);
    for (const auto& feature : aDesc.mRequiredFeatures) {
      device->mFeatures->Add(feature, aRv);
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
  }();

  return promise.forget();
}

// -

already_AddRefed<dom::Promise> Adapter::RequestAdapterInfo(
    const dom::Sequence<nsString>& /*aUnmaskHints*/, ErrorResult& aRv) const {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (!promise) return nullptr;

  auto rai = UniquePtr<AdapterInfo>{new AdapterInfo(mInfo)};
  promise->MaybeResolve(std::move(rai));
  return promise.forget();
}

}  // namespace mozilla::webgpu
