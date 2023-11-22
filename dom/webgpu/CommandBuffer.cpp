/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandBuffer.h"
#include "CommandEncoder.h"
#include "ipc/WebGPUChild.h"

#include "mozilla/webgpu/CanvasContext.h"
#include "Device.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandBuffer, mParent)
GPU_IMPL_JS_WRAP(CommandBuffer)

CommandBuffer::CommandBuffer(Device* const aParent, RawId aId,
                             nsTArray<WeakPtr<CanvasContext>>&& aTargetContexts,
                             RefPtr<CommandEncoder>&& aEncoder)
    : ChildOf(aParent), mId(aId), mTargetContexts(std::move(aTargetContexts)) {
  mEncoder = std::move(aEncoder);
  MOZ_RELEASE_ASSERT(aId);
}

CommandBuffer::~CommandBuffer() {}

void CommandBuffer::Cleanup() { mEncoder = nullptr; }

Maybe<RawId> CommandBuffer::Commit() {
  if (!mValid) {
    return Nothing();
  }
  mValid = false;
  for (const auto& targetContext : mTargetContexts) {
    if (targetContext) {
      targetContext->MaybeQueueSwapChainPresent();
    }
  }
  return Some(mId);
}

}  // namespace mozilla::webgpu
