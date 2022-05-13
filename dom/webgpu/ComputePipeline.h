/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ComputePipeline_H_
#define GPU_ComputePipeline_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsTArray.h"

namespace mozilla::webgpu {

class BindGroupLayout;
class Device;

class ComputePipeline final : public ObjectBase, public ChildOf<Device> {
  const RawId mImplicitPipelineLayoutId;
  const nsTArray<RawId> mImplicitBindGroupLayoutIds;

 public:
  GPU_DECL_CYCLE_COLLECTION(ComputePipeline)
  GPU_DECL_JS_WRAP(ComputePipeline)

  const RawId mId;

  ComputePipeline(Device* const aParent, RawId aId,
                  RawId aImplicitPipelineLayoutId,
                  nsTArray<RawId>&& aImplicitBindGroupLayoutIds);
  already_AddRefed<BindGroupLayout> GetBindGroupLayout(uint32_t index) const;

 private:
  ~ComputePipeline();
  void Cleanup();
};

}  // namespace mozilla::webgpu

#endif  // GPU_ComputePipeline_H_
