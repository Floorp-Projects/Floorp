/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_INSTANCE_H_
#define GPU_INSTANCE_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/layers/BuildConstants.h"
#include "nsCOMPtr.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class Promise;
struct GPURequestAdapterOptions;
}  // namespace dom

namespace webgpu {
class Adapter;
class GPUAdapter;
class WebGPUChild;

class Instance final : public nsWrapperCache {
 public:
  GPU_DECL_CYCLE_COLLECTION(Instance)
  GPU_DECL_JS_WRAP(Instance)

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<Instance> Create(nsIGlobalObject* aOwner);

  already_AddRefed<dom::Promise> RequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions, ErrorResult& aRv);

  dom::GPUTextureFormat GetPreferredCanvasFormat() const {
    if (kIsAndroid) {
      return dom::GPUTextureFormat::Rgba8unorm;
    }
    return dom::GPUTextureFormat::Bgra8unorm;
  };

 private:
  explicit Instance(nsIGlobalObject* aOwner);
  virtual ~Instance();
  void Cleanup();

  nsCOMPtr<nsIGlobalObject> mOwner;

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_INSTANCE_H_
