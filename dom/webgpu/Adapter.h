/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Adapter_H_
#define GPU_Adapter_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsString.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
class Promise;
struct GPUDeviceDescriptor;
struct GPUExtensions;
struct GPUFeatures;
}  // namespace dom

namespace webgpu {
class Device;
class Instance;

class Adapter final : public ObjectBase, public ChildOf<Instance> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Adapter)
  GPU_DECL_JS_WRAP(Adapter)

  const nsString mName;

 private:
  Adapter() = delete;
  virtual ~Adapter();

 public:
  void GetName(nsString& out) const { out = mName; }
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Adapter_H_
