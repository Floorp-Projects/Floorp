/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderPipeline_H_
#define GPU_RenderPipeline_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsTArray.h"

namespace mozilla::webgpu {

class BindGroupLayout;
class Device;

class RenderPipeline final : public ObjectBase, public ChildOf<Device> {
  const RawId mImplicitPipelineLayoutId;
  const nsTArray<RawId> mImplicitBindGroupLayoutIds;

 public:
  GPU_DECL_CYCLE_COLLECTION(RenderPipeline)
  GPU_DECL_JS_WRAP(RenderPipeline)

  const RawId mId;

  RenderPipeline(Device* const aParent, RawId aId,
                 RawId aImplicitPipelineLayoutId,
                 nsTArray<RawId>&& aImplicitBindGroupLayoutIds);
  already_AddRefed<BindGroupLayout> GetBindGroupLayout(uint32_t index) const;

 private:
  virtual ~RenderPipeline();
  void Cleanup();
};

}  // namespace mozilla::webgpu

#endif  // GPU_RenderPipeline_H_
