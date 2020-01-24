/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderPassEncoder_H_
#define GPU_RenderPassEncoder_H_

#include "ObjectModel.h"
#include "RenderEncoderBase.h"

namespace mozilla {
namespace dom {
class DoubleSequenceOrGPUColorDict;
template <typename T>
class Sequence;
namespace binding_detail {
template <typename T>
class AutoSequence;
}  // namespace binding_detail
}  // namespace dom
namespace webgpu {

class CommandEncoder;
class RenderBundle;

class RenderPassEncoder final : public RenderEncoderBase,
                                public ChildOf<CommandEncoder> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(RenderPassEncoder,
                                                         RenderEncoderBase)
  GPU_DECL_JS_WRAP(RenderPassEncoder)

  RenderPassEncoder() = delete;

 protected:
  virtual ~RenderPassEncoder();
  void Cleanup() {}

 public:
  void SetBindGroup(uint32_t aSlot, const BindGroup& aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets) override;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_RenderPassEncoder_H_
