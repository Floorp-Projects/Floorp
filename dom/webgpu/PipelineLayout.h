/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_PipelineLayout_H_
#define GPU_PipelineLayout_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla::webgpu {

class Device;

class PipelineLayout final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(PipelineLayout)
  GPU_DECL_JS_WRAP(PipelineLayout)

  PipelineLayout(Device* const aParent, RawId aId);

  const RawId mId;

 private:
  virtual ~PipelineLayout();
  void Cleanup();
};

}  // namespace mozilla::webgpu

#endif  // GPU_PipelineLayout_H_
