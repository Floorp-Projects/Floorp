/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SAMPLER_H_
#define GPU_SAMPLER_H_

#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla::webgpu {

class Device;

class Sampler final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Sampler)
  GPU_DECL_JS_WRAP(Sampler)

  Sampler(Device* const aParent, RawId aId);

  const RawId mId;

 private:
  virtual ~Sampler();
  void Cleanup();
};

}  // namespace mozilla::webgpu

#endif  // GPU_SAMPLER_H_
