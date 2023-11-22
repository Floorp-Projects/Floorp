/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandBuffer_H_
#define GPU_CommandBuffer_H_

#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla::webgpu {

class CanvasContext;
class Device;

class CommandBuffer final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandBuffer)
  GPU_DECL_JS_WRAP(CommandBuffer)

  CommandBuffer(Device* const aParent, RawId aId,
                nsTArray<WeakPtr<CanvasContext>>&& aTargetContexts,
                RefPtr<CommandEncoder>&& aEncoder);

  Maybe<RawId> Commit();

 private:
  CommandBuffer() = delete;
  ~CommandBuffer();
  void Cleanup();

  const RawId mId;
  const nsTArray<WeakPtr<CanvasContext>> mTargetContexts;
  // Command buffers and encoders share the same identity (this is a
  // simplifcation currently made by wgpu). To avoid dropping the same ID twice,
  // the wgpu resource lifetime is tied to the encoder which is held alive by
  // the command buffer.
  RefPtr<CommandEncoder> mEncoder;
};

}  // namespace mozilla::webgpu

#endif  // GPU_CommandBuffer_H_
