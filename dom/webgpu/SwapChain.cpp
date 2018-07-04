/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SwapChain.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

SwapChain::~SwapChain() = default;

void
SwapChain::Configure(const dom::WebGPUSwapChainDescriptor& desc) const
{
    MOZ_CRASH("todo");
}

already_AddRefed<Texture>
SwapChain::GetNextTexture() const
{
    MOZ_CRASH("todo");
}

void
SwapChain::Present() const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(SwapChain)

} // namespace webgpu
} // namespace mozilla
