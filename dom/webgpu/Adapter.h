/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Adapter_H_
#define GPU_Adapter_H_

#include <memory>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class Promise;
struct GPUDeviceDescriptor;
struct GPUExtensions;
struct GPUFeatures;
enum class GPUFeatureName : uint8_t;
enum class WgpuBackend : uint8_t;
enum class WgpuDeviceType : uint8_t;
template <typename T>
class Sequence;
}  // namespace dom

namespace webgpu {
class Device;
class Instance;
class SupportedFeatures;
class SupportedLimits;
class WebGPUChild;
namespace ffi {
struct WGPUAdapterInformation;
}  // namespace ffi

class AdapterInfo final : public dom::NonRefcountedDOMObject {
 private:
  const std::shared_ptr<ffi::WGPUAdapterInformation> mAboutSupportInfo;

 public:
  explicit AdapterInfo(
      const std::shared_ptr<ffi::WGPUAdapterInformation>& aAboutSupportInfo)
      : mAboutSupportInfo(aAboutSupportInfo) {}

  void GetVendor(nsString& s) const { s = nsString(); }
  void GetArchitecture(nsString& s) const { s = nsString(); }
  void GetDevice(nsString& s) const { s = nsString(); }
  void GetDescription(nsString& s) const { s = nsString(); }

  // Non-standard field getters; see also TODO BUGZILLA LINK
  void GetWgpuName(nsString&) const;
  uint32_t WgpuVendor() const;
  uint32_t WgpuDevice() const;
  void GetWgpuDeviceType(nsString&) const;
  void GetWgpuDriver(nsString&) const;
  void GetWgpuDriverInfo(nsString&) const;
  void GetWgpuBackend(nsString&) const;

  bool WrapObject(JSContext*, JS::Handle<JSObject*>,
                  JS::MutableHandle<JSObject*>);
};

inline auto ToHexCString(const uint64_t v) {
  return nsPrintfCString("0x%" PRIx64, v);
}

class Adapter final : public ObjectBase, public ChildOf<Instance> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Adapter)
  GPU_DECL_JS_WRAP(Adapter)

  RefPtr<WebGPUChild> mBridge;

 private:
  ~Adapter();
  void Cleanup();

  const RawId mId;
  // Cant have them as `const` right now, since we wouldn't be able
  // to unlink them in CC unlink.
  RefPtr<SupportedFeatures> mFeatures;
  RefPtr<SupportedLimits> mLimits;

  const std::shared_ptr<ffi::WGPUAdapterInformation> mInfo;

 public:
  Adapter(Instance* const aParent, WebGPUChild* const aBridge,
          const std::shared_ptr<ffi::WGPUAdapterInformation>& aInfo);
  const RefPtr<SupportedFeatures>& Features() const;
  const RefPtr<SupportedLimits>& Limits() const;
  bool IsFallbackAdapter() const;

  nsCString LabelOrId() const {
    nsCString ret = this->CLabel();
    if (ret.IsEmpty()) {
      ret = ToHexCString(mId);
    }
    return ret;
  }

  already_AddRefed<dom::Promise> RequestDevice(
      const dom::GPUDeviceDescriptor& aDesc, ErrorResult& aRv);

  already_AddRefed<dom::Promise> RequestAdapterInfo(
      const dom::Sequence<nsString>& aUnmaskHints, ErrorResult& aRv) const;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Adapter_H_
