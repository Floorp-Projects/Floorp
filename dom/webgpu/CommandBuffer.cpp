/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandBuffer.h"

#include "mozilla/dom/HTMLCanvasElement.h"
#include "Device.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandBuffer, mParent)
GPU_IMPL_JS_WRAP(CommandBuffer)

CommandBuffer::CommandBuffer(
    Device* const aParent, RawId aId,
    const WeakPtr<dom::HTMLCanvasElement>& aTargetCanvasElement)
    : ChildOf(aParent), mId(aId), mTargetCanvasElement(aTargetCanvasElement) {
  if (!aId) {
    mValid = false;
  }
}

CommandBuffer::~CommandBuffer() { Cleanup(); }

void CommandBuffer::Cleanup() {
  if (mValid && mParent) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendCommandBufferDestroy(mId);
    }
  }
}

Maybe<RawId> CommandBuffer::Commit() {
  if (!mValid) {
    return Nothing();
  }
  mValid = false;
  if (mTargetCanvasElement) {
    mTargetCanvasElement->InvalidateCanvasContent(nullptr);
  }
  return Some(mId);
}

}  // namespace webgpu
}  // namespace mozilla
