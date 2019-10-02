/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderEncoderBase_H_
#define GPU_RenderEncoderBase_H_

#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "ProgrammablePassEncoder.h"

namespace mozilla {
namespace webgpu {

class Buffer;
class ComputePipeline;

class RenderEncoderBase : public ProgrammablePassEncoder {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      RenderEncoderBase, ProgrammablePassEncoder)
  RenderEncoderBase() = delete;

 protected:
  virtual ~RenderEncoderBase();

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_RenderEncoderBase_H_
