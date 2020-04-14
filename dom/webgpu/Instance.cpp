/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Instance.h"

#include "Adapter.h"
#include "gfxConfig.h"
#include "nsIGlobalObject.h"
#include "ipc/WebGPUChild.h"
#include "ipc/WebGPUTypes.h"
#include "mozilla/layers/CompositorBridgeChild.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Instance, mBridge, mOwner)

/*static*/
already_AddRefed<Instance> Instance::Create(nsIGlobalObject* aOwner) {
  RefPtr<WebGPUChild> bridge;

  if (gfx::gfxConfig::IsEnabled(gfx::Feature::WEBGPU)) {
    bridge = layers::CompositorBridgeChild::Get()->GetWebGPUChild();
    if (NS_WARN_IF(!bridge)) {
      MOZ_CRASH("Failed to create an IPDL bridge for WebGPU!");
    }
  }

  RefPtr<Instance> result = new Instance(aOwner, bridge);
  return result.forget();
}

Instance::Instance(nsIGlobalObject* aOwner, WebGPUChild* aBridge)
    : mBridge(aBridge), mOwner(aOwner) {}

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
  if (!mBridge) {
    promise->MaybeRejectWithInvalidStateError("WebGPU is not enabled!");
    return promise.forget();
  }

  RefPtr<Instance> instance = this;

  mBridge->InstanceRequestAdapter(aOptions)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise, instance](RawId id) {
        MOZ_ASSERT(id != 0);
        RefPtr<Adapter> adapter = new Adapter(instance, id);
        promise->MaybeResolve(adapter);
      },
      [promise](const Maybe<ipc::ResponseRejectReason>& aRv) {
        if (aRv.isSome()) {
          promise->MaybeRejectWithAbortError("Internal communication error!");
        } else {
          promise->MaybeRejectWithInvalidStateError(
              "No matching adapter found!");
        }
      });

  return promise.forget();
}

}  // namespace webgpu
}  // namespace mozilla
