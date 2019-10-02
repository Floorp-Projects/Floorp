/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ComputePassEncoder_H_
#define GPU_ComputePassEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "ObjectModel.h"
#include "ProgrammablePassEncoder.h"

namespace mozilla {
namespace webgpu {

class Buffer;
class CommandEncoder;
class ComputePipeline;

class ComputePassEncoder final : public ProgrammablePassEncoder,
                                 public ChildOf<CommandEncoder> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      ComputePassEncoder, ProgrammablePassEncoder)
  GPU_DECL_JS_WRAP(ComputePassEncoder)

  ComputePassEncoder() = delete;

 private:
  virtual ~ComputePassEncoder();

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ComputePassEncoder_H_
