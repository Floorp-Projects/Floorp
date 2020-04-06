/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_INSTANCE_H_
#define GPU_INSTANCE_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "ObjectModel.h"

namespace mozilla {
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

  static already_AddRefed<Instance> Create(nsIGlobalObject* aOwner);

  already_AddRefed<dom::Promise> RequestAdapter(
      const dom::GPURequestAdapterOptions& aOptions, ErrorResult& aRv);

  const RefPtr<WebGPUChild> mBridge;

 private:
  explicit Instance(nsIGlobalObject* aOwner, WebGPUChild* aBridge);
  virtual ~Instance();
  void Cleanup();

  nsCOMPtr<nsIGlobalObject> mOwner;

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_INSTANCE_H_
