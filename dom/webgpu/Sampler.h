/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SAMPLER_H_
#define GPU_SAMPLER_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class Sampler final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Sampler)
  GPU_DECL_JS_WRAP(Sampler)

  const RawId mId;

 private:
  Sampler() = delete;
  virtual ~Sampler();
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_SAMPLER_H_
