/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Adapter.h"

#include "AdapterFeatures.h"
#include "AdapterLimits.h"
#include "Device.h"
#include "Instance.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Adapter, mParent, mBridge, mFeatures, mLimits)
GPU_IMPL_JS_WRAP(Adapter)

Adapter::Adapter(Instance* const aParent,
                 const ffi::WGPUAdapterInformation& aInfo)
    : ChildOf(aParent),
      mBridge(aParent->mBridge),
      mId(aInfo.id),
      mFeatures(new AdapterFeatures(this)),
      mLimits(new AdapterLimits(this, aInfo.limits)) {}

Adapter::~Adapter() { Cleanup(); }

void Adapter::Cleanup() {
  if (mValid && mBridge && mBridge->IsOpen()) {
    mValid = false;
    mBridge->SendAdapterDestroy(mId);
  }
}

const RefPtr<AdapterFeatures>& Adapter::Features() const { return mFeatures; }
const RefPtr<AdapterLimits>& Adapter::Limits() const { return mLimits; }

already_AddRefed<dom::Promise> Adapter::RequestDevice(
    const dom::GPUDeviceDescriptor& aDesc, ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  Maybe<RawId> id = mBridge->AdapterRequestDevice(mId, aDesc);
  if (id.isSome()) {
    RefPtr<Device> device = new Device(this, id.value());
    promise->MaybeResolve(device);
  } else {
    promise->MaybeRejectWithNotSupportedError("Unable to instanciate a Device");
  }

  return promise.forget();
}

}  // namespace webgpu
}  // namespace mozilla
