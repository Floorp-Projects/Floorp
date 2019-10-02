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
class InstanceProvider;

class Instance final : public nsWrapperCache {
 public:
  GPU_DECL_CYCLE_COLLECTION(Instance)
  GPU_DECL_JS_WRAP(Instance)

  nsCOMPtr<nsIGlobalObject> mParent;

  static RefPtr<Instance> Create(nsIGlobalObject* parent);

 private:
  explicit Instance(nsIGlobalObject* parent);
  virtual ~Instance();

 public:
  nsIGlobalObject* GetParentObject() const { return mParent.get(); }
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_INSTANCE_H_
