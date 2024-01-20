/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGPUChild.h"

#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Warnings.h"  // JS::WarnUTF8
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/GPUUncapturedErrorEvent.h"
#include "mozilla/webgpu/ValidationError.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "Adapter.h"
#include "DeviceLostInfo.h"
#include "PipelineLayout.h"
#include "Sampler.h"
#include "CompilationInfo.h"
#include "mozilla/ipc/RawShmem.h"
#include "Utility.h"

#include <utility>

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTION(WebGPUChild)

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
    RawId aSelfId, const ffi::WGPUDeviceDescriptor& aDesc) {
  RawId id = ffi::wgpu_client_make_device_id(mClient.get(), aSelfId);

  ByteBuf bb;
  ffi::wgpu_client_serialize_device_descriptor(&aDesc, ToFFI(&bb));

  DeviceRequest request;
  request.mId = id;
  request.mPromise = SendAdapterRequestDevice(aSelfId, std::move(bb), id);

  return Some(std::move(request));
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

RawId WebGPUChild::RenderBundleEncoderFinishError(RawId aDeviceId,
                                                  const nsString& aLabel) {
  webgpu::StringHelper label(aLabel);

  ipc::ByteBuf bb;
  RawId id = ffi::wgpu_client_create_render_bundle_error(
      mClient.get(), aDeviceId, label.Get(), ToFFI(&bb));

  if (!SendDeviceAction(aDeviceId, std::move(bb))) {
    MOZ_CRASH("IPC failure");
  }

  return id;
}

ipc::IPCResult WebGPUChild::RecvUncapturedError(const Maybe<RawId> aDeviceId,
                                                const nsACString& aMessage) {
  RefPtr<Device> device;
  if (aDeviceId) {
    const auto itr = mDeviceMap.find(*aDeviceId);
    if (itr != mDeviceMap.end()) {
      device = itr->second.get();
      MOZ_ASSERT(device);
    }
  }
  if (!device) {
    JsWarning(nullptr, aMessage);
  } else {
    // We don't want to spam the errors to the console indefinitely
    if (device->CheckNewWarning(aMessage)) {
      JsWarning(device->GetOwnerGlobal(), aMessage);

      dom::GPUUncapturedErrorEventInit init;
      init.mError = new ValidationError(device->GetParentObject(), aMessage);
      RefPtr<mozilla::dom::GPUUncapturedErrorEvent> event =
          dom::GPUUncapturedErrorEvent::Constructor(
              device, u"uncapturederror"_ns, init);
      device->DispatchEvent(*event);
    }
  }
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDropAction(const ipc::ByteBuf& aByteBuf) {
  const auto* byteBuf = ToFFI(&aByteBuf);
  ffi::wgpu_client_drop_action(mClient.get(), byteBuf);
  return IPC_OK();
}

ipc::IPCResult WebGPUChild::RecvDeviceLost(RawId aDeviceId,
                                           Maybe<uint8_t> aReason,
                                           const nsACString& aMessage) {
  RefPtr<Device> device;
  const auto itr = mDeviceMap.find(aDeviceId);
  if (itr != mDeviceMap.end()) {
    device = itr->second.get();
    MOZ_ASSERT(device);
  }

  if (device) {
    auto message = NS_ConvertUTF8toUTF16(aMessage);
    if (aReason.isSome()) {
      dom::GPUDeviceLostReason reason =
          static_cast<dom::GPUDeviceLostReason>(*aReason);
      device->ResolveLost(Some(reason), message);
    } else {
      device->ResolveLost(Nothing(), message);
    }
  }
  return IPC_OK();
}

void WebGPUChild::DeviceCreateSwapChain(
    RawId aSelfId, const RGBDescriptor& aRgbDesc, size_t maxBufferCount,
    const layers::RemoteTextureOwnerId& aOwnerId,
    bool aUseExternalTextureInSwapChain) {
  RawId queueId = aSelfId;  // TODO: multiple queues
  nsTArray<RawId> bufferIds(maxBufferCount);
  for (size_t i = 0; i < maxBufferCount; ++i) {
    bufferIds.AppendElement(
        ffi::wgpu_client_make_buffer_id(mClient.get(), aSelfId));
  }
  SendDeviceCreateSwapChain(aSelfId, queueId, aRgbDesc, bufferIds, aOwnerId,
                            aUseExternalTextureInSwapChain);
}

void WebGPUChild::QueueOnSubmittedWorkDone(
    const RawId aSelfId, const RefPtr<dom::Promise>& aPromise) {
  SendQueueOnSubmittedWorkDone(aSelfId)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aPromise]() { aPromise->MaybeResolveWithUndefined(); },
      [aPromise](const ipc::ResponseRejectReason& aReason) {
        aPromise->MaybeRejectWithNotSupportedError("IPC error");
      });
}

void WebGPUChild::SwapChainPresent(RawId aTextureId,
                                   const RemoteTextureId& aRemoteTextureId,
                                   const RemoteTextureOwnerId& aOwnerId) {
  // Hack: the function expects `DeviceId`, but it only uses it for `backend()`
  // selection.
  RawId encoderId = ffi::wgpu_client_make_encoder_id(mClient.get(), aTextureId);
  SendSwapChainPresent(aTextureId, encoderId, aRemoteTextureId, aOwnerId);
}

void WebGPUChild::RegisterDevice(Device* const aDevice) {
  mDeviceMap.insert({aDevice->mId, aDevice});
}

void WebGPUChild::UnregisterDevice(RawId aId) {
  mDeviceMap.erase(aId);
  if (IsOpen()) {
    SendDeviceDrop(aId);
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

    device->ResolveLost(Nothing(), u"WebGPUChild destroyed"_ns);
  }
}

}  // namespace mozilla::webgpu
