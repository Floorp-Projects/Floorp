/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "nsIGlobalObject.h"
#include "ipc/WebGPUChild.h"
#include "ipc/WebGPUTypes.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/gfx/gfxVars.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Instance, mOwner)

/*static*/
already_AddRefed<Instance> Instance::Create(nsIGlobalObject* aOwner) {
  RefPtr<Instance> result = new Instance(aOwner);
  return result.forget();
}

Instance::Instance(nsIGlobalObject* aOwner) : mOwner(aOwner) {}

Instance::~Instance() { Cleanup(); }

void Instance::Cleanup() {}

JSObject* Instance::WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) {
  return dom::GPU_Binding::Wrap(cx, this, givenProto);
}

already_AddRefed<dom::Promise> Instance::RequestAdapter(
    const dom::GPURequestAdapterOptions& aOptions, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(mOwner, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!gfx::gfxVars::AllowWebGPU()) {
    promise->MaybeRejectWithNotSupportedError("WebGPU is not enabled!");
    return promise.forget();
  }

  auto* const canvasManager = gfx::CanvasManagerChild::Get();
  if (!canvasManager) {
    promise->MaybeRejectWithInvalidStateError(
        "Failed to create CanavasManagerChild");
    return promise.forget();
  }

  RefPtr<WebGPUChild> bridge = canvasManager->GetWebGPUChild();
  if (!bridge) {
    promise->MaybeRejectWithInvalidStateError("Failed to create WebGPUChild");
    return promise.forget();
  }

  RefPtr<Instance> instance = this;

  bridge->InstanceRequestAdapter(aOptions)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, instance, bridge](ipc::ByteBuf aInfoBuf) {
        ffi::WGPUAdapterInformation info = {};
        ffi::wgpu_client_adapter_extract_info(ToFFI(&aInfoBuf), &info);
        MOZ_ASSERT(info.id != 0);
        RefPtr<Adapter> adapter = new Adapter(instance, bridge, info);
        promise->MaybeResolve(adapter);
      },
      [promise](const Maybe<ipc::ResponseRejectReason>& aResponseReason) {
        if (aResponseReason.isSome()) {
          promise->MaybeRejectWithAbortError("Internal communication error!");
        } else {
          promise->MaybeResolve(JS::NullHandleValue);
        }
      });

  return promise.forget();
}

}  // namespace mozilla::webgpu
