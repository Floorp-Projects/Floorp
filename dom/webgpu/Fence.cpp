/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fence.h"

#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

Fence::~Fence() = default;

GPU_IMPL_CYCLE_COLLECTION(Fence, mParent)
GPU_IMPL_JS_WRAP(Fence)

}  // namespace webgpu
}  // namespace mozilla
