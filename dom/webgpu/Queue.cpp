/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Queue.h"

namespace mozilla {
namespace webgpu {

Queue::~Queue() = default;

GPU_IMPL_CYCLE_COLLECTION(Queue, mParent)
GPU_IMPL_JS_WRAP(Queue)

}  // namespace webgpu
}  // namespace mozilla
