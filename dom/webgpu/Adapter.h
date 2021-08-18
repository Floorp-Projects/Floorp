/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Adapter_H_
#define GPU_Adapter_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsString.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class Promise;
struct GPUDeviceDescriptor;
struct GPUExtensions;
struct GPUFeatures;
}  // namespace dom

namespace webgpu {
class AdapterFeatures;
class AdapterLimits;
class Device;
class Instance;
class WebGPUChild;
namespace ffi {
struct WGPUAdapterInformation;
}  // namespace ffi

class Adapter final : public ObjectBase, public ChildOf<Instance> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Adapter)
  GPU_DECL_JS_WRAP(Adapter)

  RefPtr<WebGPUChild> mBridge;

 private:
  ~Adapter();
  void Cleanup();

  const RawId mId;
  const nsString mName;
  // Cant have them as `const` right now, since we wouldn't be able
  // to unlink them in CC unlink.
  RefPtr<AdapterFeatures> mFeatures;
  RefPtr<AdapterLimits> mLimits;

 public:
  Adapter(Instance* const aParent, const ffi::WGPUAdapterInformation& aInfo);
  void GetName(nsString& out) const { out = mName; }
  const RefPtr<AdapterFeatures>& Features() const;
  const RefPtr<AdapterLimits>& Limits() const;

  already_AddRefed<dom::Promise> RequestDevice(
      const dom::GPUDeviceDescriptor& aDesc, ErrorResult& aRv);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Adapter_H_
