/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fence.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

Fence::~Fence() = default;

bool
Fence::Wait(const double milliseconds) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<dom::Promise>
Fence::Promise() const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(Fence)

} // namespace webgpu
} // namespace mozilla
