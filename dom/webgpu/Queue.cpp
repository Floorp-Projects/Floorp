/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Queue.h"

#include "CommandBuffer.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(Queue, mParent, mBridge)
GPU_IMPL_JS_WRAP(Queue)

Queue::Queue(Device* const aParent, WebGPUChild* aBridge, RawId aId)
    : ChildOf(aParent), mBridge(aBridge), mId(aId) {}

Queue::~Queue() { Cleanup(); }

void Queue::Submit(
    const dom::Sequence<OwningNonNull<CommandBuffer>>& aCommandBuffers) {
  nsTArray<RawId> list(aCommandBuffers.Length());
  for (uint32_t i = 0; i < aCommandBuffers.Length(); ++i) {
    auto idMaybe = aCommandBuffers[i]->Commit();
    if (idMaybe) {
      list.AppendElement(*idMaybe);
    }
  }

  mBridge->QueueSubmit(mId, list);
}

}  // namespace webgpu
}  // namespace mozilla
