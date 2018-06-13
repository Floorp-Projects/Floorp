/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Adapter.h"

#include "Instance.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

Adapter::~Adapter() = default;

void
Adapter::Extensions(dom::WebGPUExtensions& out) const
{
    MOZ_CRASH("todo");
}

void
Adapter::Features(dom::WebGPUFeatures& out) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Device>
Adapter::CreateDevice(const dom::WebGPUDeviceDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(Adapter)

} // namespace webgpu
} // namespace mozilla
